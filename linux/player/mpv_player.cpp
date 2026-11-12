#include "mpv_player.h"
#include <QGuiApplication>
#include <QtGui/qguiapplication_platform.h>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QPainter>
#include <QVBoxLayout>
#include <QTimer>
#include <mpv/render_gl.h>
#include <clocale>

namespace Shadowless {
namespace {
const mpv_node *field(const mpv_node &node, const char *key) {
    if (node.format != MPV_FORMAT_NODE_MAP) return nullptr;
    for (int i = 0; i < node.u.list->num; ++i)
        if (qstrcmp(node.u.list->keys[i], key) == 0) return &node.u.list->values[i];
    return nullptr;
}
QString stringField(const mpv_node &node, const char *key) {
    const auto *value = field(node, key);
    return value && value->format == MPV_FORMAT_STRING ? QString::fromUtf8(value->u.string) : QString();
}
}
class MpvSurface : public QOpenGLWidget {
public:
    explicit MpvSurface(MpvPlayer *owner) : QOpenGLWidget(owner), m_owner(owner) {
        setMouseTracking(true);
    }
protected:
    void initializeGL() override {
        connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, [this] {
            makeCurrent(); m_owner->freeRender(); doneCurrent();
        }, Qt::DirectConnection);
        m_owner->initializeRender();
    }
    void paintGL() override {
        if (!m_owner->m_render) m_owner->initializeRender();
        if (!m_owner->m_render) {
            context()->functions()->glClear(GL_COLOR_BUFFER_BIT); return;
        }
        mpv_opengl_fbo fbo{int(defaultFramebufferObject()), qRound(width() * devicePixelRatioF()),
                           qRound(height() * devicePixelRatioF()), 0};
        int flip = 1;
        mpv_render_param params[]{{MPV_RENDER_PARAM_OPENGL_FBO, &fbo}, {MPV_RENDER_PARAM_FLIP_Y, &flip}, {MPV_RENDER_PARAM_INVALID, nullptr}};
        mpv_render_context_render(m_owner->m_render, params);
        emit m_owner->frameRendered();
    }
    void mouseDoubleClickEvent(QMouseEvent *) override { emit m_owner->doubleClicked(); }
private:
    MpvPlayer *m_owner;
};

MpvPlayer::MpvPlayer(QWidget *parent) : QWidget(parent) {
    setObjectName("mpvPlayer");
    setMinimumSize(160, 90);
    setMouseTracking(true);
    // Headless Qt platforms have no OpenGL surface; use libmpv's software renderer there.
    m_software = QGuiApplication::platformName() == "offscreen" || QGuiApplication::platformName() == "minimal";
    if (!m_software) {
        auto *layout = new QVBoxLayout(this); layout->setContentsMargins(0, 0, 0, 0);
        m_surface = new MpvSurface(this); layout->addWidget(m_surface);
    }
}
MpvPlayer::~MpvPlayer() {
    // Parent widgets may already be deleting controls connected to our signals.
    blockSignals(true);
    release();
    delete m_surface; m_surface = nullptr;
}
void MpvPlayer::release() {
    stop();
    if (m_mpv) mpv_set_wakeup_callback(m_mpv, nullptr, nullptr);
    if (m_surface && m_surface->context()) m_surface->makeCurrent();
    freeRender();
    if (m_surface && m_surface->context()) m_surface->doneCurrent();
    if (m_mpv) { mpv_terminate_destroy(m_mpv); m_mpv = nullptr; }
    m_updatePending = false; m_eventsPending = false;
}
bool MpvPlayer::initialize() {
    if (m_mpv) return true;
    // QApplication adopts the desktop locale; libmpv requires C numeric parsing.
    std::setlocale(LC_NUMERIC, "C");
    m_mpv = mpv_create();
    if (!m_mpv) { emit error("Could not create the media player."); return false; }
    const std::pair<const char *, const char *> options[]{
        {"config", "no"}, {"terminal", "no"}, {"input-default-bindings", "no"},
        {"input-vo-keyboard", "no"}, {"osc", "no"}, {"load-scripts", "no"},
        {"vo", "libmpv"}, {"hwdec", m_software ? "no" : "auto-safe"}, {"vd-lavc-threads", "2"},
        // Use required pipewire-pulse; libmpv 0.41 native PipeWire initialization can deadlock.
        {"ao", "pulse"},
        {"ad-lavc-threads", "1"}, {"demuxer-max-bytes", "32MiB"},
        {"demuxer-max-back-bytes", "8MiB"}, {"cache", "no"}, {"keep-open", "yes"},
        {"volume", "80"}, {"audio-client-name", "Shadowless player"}
    };
    for (const auto &option : options) mpv_set_option_string(m_mpv, option.first, option.second);
    const auto result = mpv_initialize(m_mpv);
    if (result < 0) {
        emit error(QString("Could not initialize the media player: %1").arg(mpv_error_string(result)));
        mpv_terminate_destroy(m_mpv); m_mpv = nullptr; return false;
    }
    mpv_set_wakeup_callback(m_mpv, [](void *ctx) {
        auto *self = static_cast<MpvPlayer *>(ctx);
        if (!self->m_eventsPending.exchange(true))
            QMetaObject::invokeMethod(self, [self] { self->m_eventsPending = false; self->events(); }, Qt::QueuedConnection);
    }, this);
    if (qEnvironmentVariableIsSet("SHADOWLESS_PLAYER_TRACE")) mpv_request_log_messages(m_mpv, "debug");
    mpv_observe_property(m_mpv, 0, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, 0, "track-list", MPV_FORMAT_NODE);
    mpv_observe_property(m_mpv, 0, "hwdec-current", MPV_FORMAT_STRING);
    if (m_software) initializeRender();
    else if (m_surface->context()) {
        m_surface->makeCurrent(); initializeRender(); m_surface->doneCurrent();
    }
    return true;
}
void MpvPlayer::initializeRender() {
    if (!m_mpv || m_render) return;
    mpv_opengl_init_params gl{[](void *, const char *name) -> void * {
        return reinterpret_cast<void *>(QOpenGLContext::currentContext()->getProcAddress(name));
    }, nullptr};
    QVector<mpv_render_param> params{{MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(m_software ? MPV_RENDER_API_TYPE_SW : MPV_RENDER_API_TYPE_OPENGL)}};
    if (!m_software) {
        params.append({MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl});
#if QT_CONFIG(wayland)
        if (auto *wayland = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>())
            params.append({MPV_RENDER_PARAM_WL_DISPLAY, wayland->display()});
#endif
    }
    params.append({MPV_RENDER_PARAM_INVALID, nullptr});
    const auto result = mpv_render_context_create(&m_render, m_mpv, params.data());
    if (result < 0) { emit error(QString("Could not initialize video rendering: %1").arg(mpv_error_string(result))); return; }
    mpv_render_context_set_update_callback(m_render, [](void *ctx) {
        auto *self = static_cast<MpvPlayer *>(ctx);
        if (self->m_updatePending.exchange(true)) return;
        QMetaObject::invokeMethod(self, [self] {
            self->m_updatePending = false;
            if (!self->isVisible() || self->m_source.isEmpty()) return;
            if (self->m_software) self->renderSoftware();
            else self->m_surface->update();
        }, Qt::QueuedConnection);
    }, this);
    loadPending();
}
void MpvPlayer::freeRender() {
    if (!m_render) return;
    mpv_render_context_set_update_callback(m_render, nullptr, nullptr);
    mpv_render_context_free(m_render); m_render = nullptr;
}
void MpvPlayer::command(const QStringList &args) {
    if (!m_mpv) return;
    QList<QByteArray> bytes; for (const auto &arg : args) bytes.append(arg.toUtf8());
    QVector<const char *> argv; for (const auto &arg : bytes) argv.append(arg.constData()); argv.append(nullptr);
    mpv_command_async(m_mpv, 0, argv.data());
}
void MpvPlayer::property(const char *name, const QString &value) {
    command({"set", QString::fromLatin1(name), value});
}
void MpvPlayer::open(const QString &path) {
    stop();
    m_requestedPause = false;
    m_source = path; m_loadPending = true;
    if (initialize()) { loadPending(); if (m_surface) m_surface->update(); }
}
void MpvPlayer::loadPending() {
    if (!m_loadPending || !m_render) return;
    m_loadPending = false;
    property("lavfi-complex", ""); property("aid", "auto"); property("pause", "no");
    command({"loadfile", m_source});
}
void MpvPlayer::stop() {
    ++m_audioGeneration; m_audioReloadPending = false; m_tracksSelected = false; m_selectedTracks.clear();
    m_hardwareDecoder.clear();
    m_loadPending = false;
    command({"stop"});
    m_source.clear(); m_frame = {}; m_tracks.clear(); m_position = m_duration = 0;
    m_rangeIn = m_rangeOut = 0;
    m_seekTarget = -1;
    m_loaded = false; m_paused = true; emit stateChanged(); update();
}
void MpvPlayer::pause(bool paused) {
    const auto position = m_seekTarget >= 0 ? m_seekTarget : m_position;
    if (!paused && m_rangeOut > m_rangeIn && (position < m_rangeIn || position >= m_rangeOut)) seek(m_rangeIn);
    m_requestedPause = paused; property("pause", paused ? "yes" : "no");
}
void MpvPlayer::setPlaybackRange(qint64 inMs, qint64 outMs) {
    inMs = qMax<qint64>(0, inMs);
    if (outMs <= inMs) inMs = outMs = 0;
    if (m_rangeIn == inMs && m_rangeOut == outMs) return;
    m_rangeIn = inMs; m_rangeOut = outMs;
    if (outMs > inMs && (m_position < inMs || m_position > outMs)) seek(m_position);
}
void MpvPlayer::seek(qint64 ms) {
    if (!m_loaded) return;
    ms = qBound<qint64>(0, ms, m_duration);
    if (m_rangeOut > m_rangeIn) ms = qBound(m_rangeIn, ms, m_rangeOut);
    m_seekTarget = ms;
    if (m_audioReloadPending) m_audioReloadPosition = ms;
    command({"seek", QString::number(ms / 1000., 'f', 3), "absolute+exact"});
}
void MpvPlayer::setVolume(int volume) { property("volume", QString::number(qBound(0, volume, 100))); }
void MpvPlayer::setMuted(bool muted) { property("mute", muted ? "yes" : "no"); }
void MpvPlayer::setSpeed(double speed) { property("speed", QString::number(qBound(.25, speed, 4.))); }
void MpvPlayer::setAudioTracks(const QVector<int> &ids) {
    if (m_tracksSelected && ids == m_selectedTracks) return;
    m_tracksSelected = true; m_selectedTracks = ids;
    if (m_audioReloadPending) return;
    m_audioReloadPending = true; m_audioReloadPosition = m_position;
    const auto generation = m_audioGeneration;
    QTimer::singleShot(0, this, [this, generation] {
        if (generation != m_audioGeneration || m_source.isEmpty()) return;
        m_audioReloadPending = false;
        QString graph;
        if (m_selectedTracks.size() > 1) {
            for (int id : m_selectedTracks) graph += QString("[aid%1]").arg(id);
            graph += QString("amix=inputs=%1:normalize=0:dropout_transition=0,alimiter=limit=0.95:level=disabled[ao]").arg(m_selectedTracks.size());
        }
        // Reload this clip at the same position: libmpv 0.41 can lose video on an in-place audio graph refresh.
        command({"stop"});
        property("lavfi-complex", graph);
        property("aid", m_selectedTracks.size() == 1 ? QString::number(m_selectedTracks.first()) : "no");
        command({"loadfile", m_source, "replace", "-1", QString("start=%1,pause=%2")
            .arg(m_audioReloadPosition / 1000., 0, 'f', 3).arg(m_requestedPause ? "yes" : "no")});
    });
}
void MpvPlayer::events() {
    if (!m_mpv) return;
    for (;;) {
        const auto *event = mpv_wait_event(m_mpv, 0);
        if (event->event_id == MPV_EVENT_NONE) break;
        if (event->event_id == MPV_EVENT_PLAYBACK_RESTART) m_seekTarget = -1;
        if (event->event_id == MPV_EVENT_LOG_MESSAGE) {
            const auto *log = static_cast<mpv_event_log_message *>(event->data);
            qInfo().noquote() << log->prefix << QString::fromUtf8(log->text).trimmed();
        }
        if (event->event_id == MPV_EVENT_COMMAND_REPLY && event->error < 0 && m_loaded && !m_source.isEmpty())
            emit error(QString("Playback command failed: %1").arg(mpv_error_string(event->error)));
        if (event->event_id == MPV_EVENT_FILE_LOADED && !m_source.isEmpty()) {
            m_loaded = true;
            property("pause", m_requestedPause ? "yes" : "no"); emit stateChanged();
        }
        if (event->event_id == MPV_EVENT_END_FILE) {
            const auto *end = static_cast<mpv_event_end_file *>(event->data);
            if (end->error < 0 && !m_source.isEmpty()) emit error(QString("Could not play this clip: %1").arg(mpv_error_string(end->error)));
        }
        if (event->event_id != MPV_EVENT_PROPERTY_CHANGE || m_source.isEmpty()) continue;
        const auto *prop = static_cast<mpv_event_property *>(event->data);
        if (!prop->data) continue;
        if (qstrcmp(prop->name, "duration") == 0 && prop->format == MPV_FORMAT_DOUBLE) m_duration = qRound64(*static_cast<double *>(prop->data) * 1000.);
        else if (qstrcmp(prop->name, "time-pos") == 0 && prop->format == MPV_FORMAT_DOUBLE) {
            m_position = qRound64(*static_cast<double *>(prop->data) * 1000.);
            // Exact seeks can land on the next frame; only constrain advancing playback.
            if (!m_requestedPause && m_seekTarget < 0 && m_rangeOut > m_rangeIn) {
                if (m_position < m_rangeIn) seek(m_rangeIn);
                else if (m_position >= m_rangeOut) {
                    pause(true); seek(m_rangeOut);
                }
            }
        }
        else if (qstrcmp(prop->name, "pause") == 0 && prop->format == MPV_FORMAT_FLAG) m_paused = *static_cast<int *>(prop->data);
        else if (qstrcmp(prop->name, "hwdec-current") == 0 && prop->format == MPV_FORMAT_STRING) m_hardwareDecoder = QString::fromUtf8(*static_cast<char **>(prop->data));
        else if (qstrcmp(prop->name, "track-list") == 0 && prop->format == MPV_FORMAT_NODE) {
            const auto &node = *static_cast<mpv_node *>(prop->data);
            if (m_tracksSelected && node.format == MPV_FORMAT_NODE_ARRAY && node.u.list->num == 0) continue;
            m_tracks.clear();
            if (node.format == MPV_FORMAT_NODE_ARRAY) for (int i = 0; i < node.u.list->num; ++i) {
                const auto &track = node.u.list->values[i]; const auto *id = field(track, "id");
                if (stringField(track, "type") != "audio" || !id || id->format != MPV_FORMAT_INT64) continue;
                QString title = stringField(track, "title");
                if (title.isEmpty()) title = QString("Audio track %1").arg(m_tracks.size() + 1);
                m_tracks.append({int(id->u.int64), title});
            }
            emit tracksChanged();
        }
        emit stateChanged();
    }
}
void MpvPlayer::renderSoftware() {
    if (!m_render || !isVisible() || m_source.isEmpty()) return;
    const QSize size = this->size().boundedTo(QSize(640, 360));
    m_frame = QImage(size, QImage::Format_RGB32);
    int dimensions[]{size.width(), size.height()}; size_t stride = size_t(m_frame.bytesPerLine());
    mpv_render_param params[]{{MPV_RENDER_PARAM_SW_SIZE, dimensions}, {MPV_RENDER_PARAM_SW_FORMAT, const_cast<char *>("bgr0")},
        {MPV_RENDER_PARAM_SW_STRIDE, &stride}, {MPV_RENDER_PARAM_SW_POINTER, m_frame.bits()}, {MPV_RENDER_PARAM_INVALID, nullptr}};
    mpv_render_context_render(m_render, params); update(); emit frameRendered();
}
void MpvPlayer::paintEvent(QPaintEvent *) {
    if (!m_software) return;
    QPainter painter(this); painter.fillRect(rect(), Qt::black);
    if (!m_frame.isNull()) painter.drawImage(rect(), m_frame);
}
void MpvPlayer::mouseDoubleClickEvent(QMouseEvent *) { emit doubleClicked(); }
}
