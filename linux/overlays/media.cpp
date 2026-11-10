#include "media.h"
#include <QFileInfo>
#include <QImageReader>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

namespace Shadowless {
struct Media::Data {
    QImage image;
    std::unique_ptr<QImageReader> imageReader;
    QString imagePath;
    AVFormatContext *format = nullptr;
    AVCodecContext *codec = nullptr;
    AVFrame *decoded = nullptr;
    AVPacket *packet = nullptr;
    SwsContext *scaler = nullptr;
    int stream = -1;
    QSize maximum;
    double frameMs = 33.333;
    qint64 nextMs = 0;
    bool ended = false;
    ~Data() {
        sws_freeContext(scaler); av_packet_free(&packet); av_frame_free(&decoded);
        avcodec_free_context(&codec); avformat_close_input(&format);
    }
    bool readFrame() {
        for (int count = 0; count < 4096; ++count) {
            int rc = avcodec_receive_frame(codec, decoded);
            if (rc == 0) {
                if (decoded->width < 1 || decoded->height < 1 || decoded->width > 8192 || decoded->height > 8192) return false;
                const QSize size = QSize(decoded->width, decoded->height).scaled(maximum, Qt::KeepAspectRatio);
                QImage next(size, QImage::Format_RGBA8888);
                if (next.isNull()) return false;
                scaler = sws_getCachedContext(scaler, decoded->width, decoded->height,
                    static_cast<AVPixelFormat>(decoded->format), size.width(), size.height(), AV_PIX_FMT_RGBA,
                    SWS_BILINEAR, nullptr, nullptr, nullptr);
                if (!scaler) return false;
                uint8_t *out[] = {next.bits()}; int strides[] = {static_cast<int>(next.bytesPerLine())};
                sws_scale(scaler, decoded->data, decoded->linesize, 0, decoded->height, out, strides);
                // Packet duration preserves variable frame timing when available.
                if (decoded->duration > 0) frameMs = qBound(1., decoded->duration * av_q2d(format->streams[stream]->time_base) * 1000., 60000.);
                image = next; return true;
            }
            if (rc == AVERROR_EOF) {
                if (ended) return false;
                if (av_seek_frame(format, stream, 0, AVSEEK_FLAG_BACKWARD) < 0) return false;
                avcodec_flush_buffers(codec); ended = true; continue;
            }
            if (rc != AVERROR(EAGAIN)) return false;
            rc = av_read_frame(format, packet);
            if (rc < 0) { avcodec_send_packet(codec, nullptr); continue; }
            if (packet->stream_index == stream) {
                rc = avcodec_send_packet(codec, packet);
                av_packet_unref(packet);
                if (rc < 0 && rc != AVERROR(EAGAIN)) return false;
            } else av_packet_unref(packet);
        }
        return false;
    }
};
Media::Media() : d(std::make_unique<Data>()) {}
Media::~Media() = default;
bool Media::open(const QString &path, QSize maximum, QString *error) {
    d = std::make_unique<Data>(); d->maximum = maximum.boundedTo(QSize(1920, 1080)).expandedTo(QSize(1, 1));
    auto fail = [&](const QString &reason) { *error = QFileInfo(path).fileName() + ": " + reason; return false; };
    const QFileInfo file(path);
    if (!file.isFile() || !file.isReadable()) return fail("Cannot read media file.");
    const auto extension = file.suffix().toLower();
    if (QStringList{"png", "jpg", "jpeg", "bmp", "webp", "gif"}.contains(extension)) {
        auto reader = std::make_unique<QImageReader>(path); reader->setAutoTransform(true);
        const auto size = reader->size();
        if (size.isEmpty() || size.width() > 16384 || size.height() > 16384) return fail("Image dimensions are unsupported.");
        reader->setScaledSize(size.scaled(d->maximum, Qt::KeepAspectRatio));
        d->image = reader->read().convertToFormat(QImage::Format_RGBA8888);
        if (d->image.isNull()) return fail(reader->errorString());
        if (reader->supportsAnimation() && reader->imageCount() != 1) {
            d->nextMs = qMax(10, reader->nextImageDelay()); d->imagePath = path; d->imageReader = std::move(reader);
        }
        error->clear(); return true;
    }
    AVDictionary *opts = nullptr;
    av_dict_set(&opts, "protocol_whitelist", "file", 0);
    av_dict_set(&opts, "probesize", "5000000", 0);
    av_dict_set(&opts, "analyzeduration", "5000000", 0);
    const int opened = avformat_open_input(&d->format, path.toUtf8().constData(), nullptr, &opts);
    av_dict_free(&opts);
    if (opened < 0 || avformat_find_stream_info(d->format, nullptr) < 0) return fail("Cannot open video stream.");
    const AVCodec *decoder = nullptr;
    d->stream = av_find_best_stream(d->format, AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
    if (d->stream < 0 || !decoder) return fail("No decodable video stream.");
    const auto *parameters = d->format->streams[d->stream]->codecpar;
    if (parameters->width < 1 || parameters->height < 1 || parameters->width > 8192 || parameters->height > 8192)
        return fail("Video dimensions are unsupported.");
    d->codec = avcodec_alloc_context3(decoder);
    if (!d->codec || avcodec_parameters_to_context(d->codec, parameters) < 0) return fail("Cannot initialize video decoder.");
    d->codec->thread_count = 1;
    if (avcodec_open2(d->codec, decoder, nullptr) < 0) return fail("Cannot initialize video decoder.");
    d->decoded = av_frame_alloc(); d->packet = av_packet_alloc();
    if (!d->decoded || !d->packet) return fail("Cannot allocate video decoder.");
    const double fps = av_q2d(av_guess_frame_rate(d->format, d->format->streams[d->stream], nullptr));
    if (fps > 0) d->frameMs = qMax(1., 1000. / fps);
    if (!d->readFrame()) return fail("Cannot decode first video frame.");
    d->nextMs = qRound64(d->frameMs); error->clear(); return true;
}
bool Media::advance(qint64 elapsedMs) {
    if (elapsedMs < d->nextMs) return false;
    if (d->imageReader) {
        auto image = d->imageReader->read();
        if (image.isNull()) {
            d->imageReader = std::make_unique<QImageReader>(d->imagePath);
            d->imageReader->setScaledSize(d->imageReader->size().scaled(d->maximum, Qt::KeepAspectRatio));
            image = d->imageReader->read();
        }
        if (image.isNull()) return false;
        d->image = image.convertToFormat(QImage::Format_RGBA8888);
        d->nextMs += qMax(10, d->imageReader->nextImageDelay()); return true;
    }
    if (!d->codec) return false;
    bool changed = false;
    for (int count = 0; count < 8 && elapsedMs >= d->nextMs; ++count) {
        d->ended = false;
        if (!d->readFrame()) break;
        d->nextMs += qRound64(d->frameMs); changed = true;
    }
    return changed;
}
const QImage &Media::frame() const { return d->image; }
bool Media::animated() const { return d->codec != nullptr || d->imageReader != nullptr; }
} // namespace Shadowless
