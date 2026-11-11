#include "compositor.h"
#include "../overlays/media.h"
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QFile>
#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <thread>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <sys/stat.h>

namespace Shadowless {
struct CompositorWorker::Data {
    std::atomic<bool> stopping{false};
    std::thread thread;
    int server = -1;
    QString socketPath;
    QSize canvas;
    std::mutex mutex;
    QVector<RenderLayer> published;
    OverlayScene scene;
    struct Asset { QString path; QSize size; std::shared_ptr<Media> media; qint64 started; };
    std::map<QString, Asset> assets;
    QElapsedTimer clock;
    int previewServer = -1;
    std::thread previewThread;
    std::atomic<bool> previewDemand{false};
    std::mutex previewMutex;
    QImage previewFrame;

    void runPreview() {
        int peer = -1; QByteArray input, output; QElapsedTimer age;
        auto closePeer = [&] {
            previewDemand = false;
            if (peer >= 0) ::close(peer);
            peer = -1; input.clear(); output.clear();
            std::lock_guard<std::mutex> lock(previewMutex); previewFrame = {};
        };
        while (!stopping) {
            pollfd fds[2] = {{previewServer, POLLIN, 0}, {peer, static_cast<short>(output.isEmpty() ? POLLIN : POLLIN | POLLOUT), 0}};
            ::poll(fds, 2, peer >= 0 ? 20 : 100);
            if (fds[0].revents & POLLIN) {
                const int fd = ::accept4(previewServer, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
                if (peer < 0) { peer = fd; age.start(); } else if (fd >= 0) ::close(fd);
            }
            if (peer >= 0 && (fds[1].revents & POLLIN)) {
                char bytes[32]; const auto count = ::recv(peer, bytes, sizeof(bytes), 0);
                if (count <= 0) { closePeer(); continue; }
                input.append(bytes, count);
                if (input == "FRAME\n") previewDemand = true;
                else if (input.size() >= 6) { closePeer(); continue; }
            }
            if (peer >= 0 && output.isEmpty()) {
                QImage frame;
                { std::lock_guard<std::mutex> lock(previewMutex); frame.swap(previewFrame); }
                if (!frame.isNull()) {
                    output = "SLP1 " + QByteArray::number(frame.width()) + ' ' + QByteArray::number(frame.height()) + ' '
                        + QByteArray::number(canvas.width()) + ' ' + QByteArray::number(canvas.height()) + '\n';
                    output.append(reinterpret_cast<const char *>(frame.constBits()), frame.sizeInBytes());
                }
            }
            if (peer >= 0 && !output.isEmpty() && (fds[1].revents & POLLOUT)) {
                const auto count = ::send(peer, output.constData(), output.size(), MSG_NOSIGNAL);
                if (count > 0) { output.remove(0, count); if (output.isEmpty()) closePeer(); }
                else if (errno != EAGAIN && errno != EINTR) closePeer();
            }
            if (peer >= 0 && ((fds[1].revents & (POLLHUP | POLLERR | POLLNVAL)) || age.elapsed() > 1500)) closePeer();
        }
        closePeer();
    }

    bool apply(const OverlayScene &next, QString *error) {
        std::map<QString, Asset> replacement;
        if (next.enabled) for (const auto &layer : next.layers) {
            if (!layer.enabled || layer.opacity == 0) continue;
            const QSize size(qMax(1, qRound(canvas.width() * layer.width)), qMax(1, qRound(canvas.height() * layer.height)));
            const auto found = assets.find(layer.id);
            if (found != assets.end() && found->second.path == layer.path && found->second.size == size)
                replacement.emplace(layer.id, found->second);
            else {
                auto media = std::make_shared<Media>();
                if (!media->open(layer.path, size, error)) return false;
                replacement.emplace(layer.id, Asset{layer.path, size, media, clock.elapsed()});
            }
        }
        assets = std::move(replacement); scene = next; publish(); return true;
    }
    void publish() {
        QVector<RenderLayer> frames;
        if (scene.enabled) for (const auto &layer : scene.layers) {
            const auto found = assets.find(layer.id);
            if (found != assets.end()) frames.append({layer, found->second.media->frame()});
        }
        std::lock_guard<std::mutex> lock(mutex); published = frames;
    }
    void run() {
        clock.start();
        int peer = -1; QByteArray input, output; qint64 connectedAt = 0;
        while (!stopping) {
            pollfd fds[2] = {{server, POLLIN, 0}, {peer, static_cast<short>(output.isEmpty() ? POLLIN : POLLOUT), 0}};
            bool animation = false;
            for (const auto &entry : assets) if (entry.second.media->animated()) { animation = true; break; }
            ::poll(fds, 2, animation ? 10 : 100);
            if (fds[0].revents & POLLIN) {
                const int accepted = ::accept4(server, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
                if (peer < 0) { peer = accepted; input.clear(); output.clear(); connectedAt = clock.elapsed(); }
                else if (accepted >= 0) ::close(accepted);
            }
            if (peer >= 0 && (fds[1].revents & POLLIN)) {
                char bytes[4096]; const auto count = ::recv(peer, bytes, sizeof(bytes), 0);
                if (count > 0) input.append(bytes, static_cast<qsizetype>(count));
                else if (count == 0) { ::close(peer); peer = -1; }
                if (input.size() > 65536) { ::close(peer); peer = -1; }
                else if (peer >= 0 && input.contains('\n')) {
                    QJsonParseError parse; OverlayScene candidate; QString error;
                    const auto document = QJsonDocument::fromJson(input.left(input.indexOf('\n')), &parse);
                    bool ok = parse.error == QJsonParseError::NoError && document.isObject();
                    if (ok) ok = OverlayScene::fromJson(document.object(), &candidate, &error);
                    else error = "Invalid overlay JSON.";
                    if (ok) ok = apply(candidate, &error);
                    output = QJsonDocument(QJsonObject{{"ok", ok}, {"error", error}}).toJson(QJsonDocument::Compact) + '\n';
                }
            }
            if (peer >= 0 && !output.isEmpty() && (fds[1].revents & POLLOUT)) {
                const auto sent = ::send(peer, output.constData(), static_cast<size_t>(output.size()), MSG_NOSIGNAL);
                if (sent > 0) output.remove(0, static_cast<qsizetype>(sent));
                if (output.isEmpty() || (sent < 0 && errno != EAGAIN && errno != EINTR)) { ::close(peer); peer = -1; }
            }
            if (peer >= 0 && ((fds[1].revents & (POLLHUP | POLLERR | POLLNVAL)) || clock.elapsed() - connectedAt > 15000)) {
                ::close(peer); peer = -1;
            }
            bool changed = false;
            for (auto &entry : assets) changed |= entry.second.media->advance(clock.elapsed() - entry.second.started);
            if (changed) publish();
        }
        if (peer >= 0) ::close(peer);
    }
    ~Data() {
        stopping = true;
        if (thread.joinable()) thread.join();
        if (previewThread.joinable()) previewThread.join();
        if (server >= 0) ::close(server);
        if (previewServer >= 0) ::close(previewServer);
        if (!socketPath.isEmpty()) QFile::remove(socketPath);
        if (!socketPath.isEmpty()) QFile::remove(socketPath + ".preview");
    }
};
CompositorWorker::CompositorWorker() : d(std::make_unique<Data>()) {}
CompositorWorker::~CompositorWorker() = default;
bool CompositorWorker::start(const QString &path, QSize canvas, QString *error) {
    if (d->server >= 0) { *error = "Compositor is already running."; return false; }
    const auto encoded = path.toUtf8(); sockaddr_un address{}; address.sun_family = AF_UNIX;
    if (encoded.isEmpty() || encoded.size() >= static_cast<qsizetype>(sizeof(address.sun_path)) || canvas.isEmpty()) {
        *error = "Invalid compositor socket or dimensions."; return false;
    }
    memcpy(address.sun_path, encoded.constData(), static_cast<size_t>(encoded.size()));
    const int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0 || ::bind(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0 || ::listen(fd, 4) < 0) {
        if (fd >= 0) ::close(fd);
        *error = "Cannot create private compositor socket."; return false;
    }
    d->server = fd; d->canvas = canvas; d->socketPath = path;
    ::chmod(encoded.constData(), 0600);
    const auto previewPath = (path + ".preview").toUtf8();
    if (previewPath.size() >= static_cast<qsizetype>(sizeof(address.sun_path))) {
        *error = "Invalid preview socket path."; return false;
    }
    memset(address.sun_path, 0, sizeof(address.sun_path));
    memcpy(address.sun_path, previewPath.constData(), previewPath.size());
    d->previewServer = ::socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (d->previewServer < 0 || ::bind(d->previewServer, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0 || ::listen(d->previewServer, 2) < 0) {
        *error = "Cannot create private preview socket."; return false;
    }
    ::chmod(previewPath.constData(), 0600);
    d->previewThread = std::thread([this] { d->runPreview(); });
    d->thread = std::thread([this] { d->run(); }); error->clear(); return true;
}
bool CompositorWorker::snapshot(QVector<RenderLayer> *layers) {
    std::unique_lock<std::mutex> lock(d->mutex, std::try_to_lock);
    if (!lock.owns_lock()) return false;
    *layers = d->published; return true;
}
bool CompositorWorker::previewRequested() const { return d->previewDemand.load(std::memory_order_relaxed); }
bool CompositorWorker::publishPreview(const QImage &image) {
    if (!previewRequested() || image.isNull() || image.width() > 640 || image.height() > 640 || image.format() != QImage::Format_RGBA8888) return false;
    std::unique_lock<std::mutex> lock(d->previewMutex, std::try_to_lock);
    if (!lock.owns_lock()) return false;
    d->previewFrame = image; d->previewDemand = false; return true;
}
} // namespace Shadowless
