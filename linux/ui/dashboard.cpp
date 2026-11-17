#include "dashboard.h"
#include "value_controls.h"
#include <QSlider>
#include "library.h"
#include "overlay_editor.h"
#include "clip_player.h"
#include "../player/mpv_player.h"
#include "settings_page.h"
#include "theme.h"
#include <QApplication>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QFrame>
#include <QKeyEvent>
#include <QMenu>
#include <QAction>
#include <QMouseEvent>
#include <QVariantAnimation>
#include <QLabel>
#include <QBoxLayout>
#include <QScrollArea>
#include <QStyle>
#include <QLocale>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QShortcut>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStyleOptionButton>
#include <QUrl>
#include <QVBoxLayout>

namespace Shadowless {
namespace {
// Holds the notice label: grows with its text up to a cap, then scrolls.
class NoticeScroll : public QScrollArea {
public:
    static constexpr int MaxHeight = 96;
    explicit NoticeScroll(QLabel *label) {
        setObjectName("noticeScroll"); setFrameShape(QFrame::NoFrame); setWidgetResizable(true);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setWidget(label);
    }
    QSize sizeHint() const override {
        const int width = qMax(viewport()->width(), 200);
        const int height = widget()->hasHeightForWidth() ? widget()->heightForWidth(width) : widget()->sizeHint().height();
        return {QScrollArea::sizeHint().width(), qMin(height, MaxHeight)};
    }
    QSize minimumSizeHint() const override { return {0, qMin(sizeHint().height(), 40)}; }
protected:
    void resizeEvent(QResizeEvent *event) override { QScrollArea::resizeEvent(event); setFixedHeight(sizeHint().height()); }
};
} // namespace

namespace {
class CaptureModeTile : public QPushButton {
public:
    CaptureModeTile(const QString &title, const QString &detail) : QPushButton(title), m_detail(detail) {
        setProperty("glowRadius", 12);
        m_menu = new QMenu(this);
        m_menu->setObjectName("captureMenu");
        m_menu->setWindowFlags(m_menu->windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
        m_menu->setAttribute(Qt::WA_TranslucentBackground);
        setFixedHeight(138);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setAccessibleDescription(detail);
    }
    QMenu *menu() const { return m_menu; }
protected:
    QRect arrowRect() const { return QRect(width() - 38, 6, 32, 28); }
    // The tile is a menu button: it never toggles by itself, every action lives in the menu.
    void openMenu() {
        m_menu->setMinimumWidth(width());
        m_menu->exec(mapToGlobal(QPoint(0, height() + 6)));
    }
    void mouseReleaseEvent(QMouseEvent *event) override {
        const bool inside = event->button() == Qt::LeftButton && isDown() && rect().contains(event->position().toPoint());
        setDown(false);
        if (inside) openMenu();
    }
    void keyPressEvent(QKeyEvent *event) override {
        if (event->key() == Qt::Key_Down || event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter || event->key() == Qt::Key_Space) { openMenu(); return; }
        QPushButton::keyPressEvent(event);
    }
    void paintEvent(QPaintEvent *) override {
        QStyleOptionButton option;
        initStyleOption(&option);
        option.text.clear(); option.icon = {};
        QPainter painter(this);
        style()->drawControl(QStyle::CE_PushButton, &option, &painter, this);
        painter.setRenderHint(QPainter::Antialiasing);
        if (!isEnabled()) painter.setOpacity(.55);
        const auto accent = window()->palette().color(QPalette::Accent);
        const auto ink = palette().color(QPalette::WindowText);
        QColor badge = isChecked() ? accent : ink; badge.setAlpha(isChecked() ? 28 : 12);
        const QRectF emblem(width() / 2. - 28, 14, 56, 56);
        painter.setPen(QPen(isChecked() ? accent : QColor("#768089"), 2.6)); painter.setBrush(badge);
        if (isChecked()) painter.drawArc(emblem, 95 * 16, 305 * 16);
        else painter.drawEllipse(emblem);
        painter.setPen(Qt::NoPen); painter.setBrush(isChecked() ? accent : QColor("#768089"));
        painter.drawEllipse(emblem.center(), 6.5, 6.5);
        auto titleFont = font(); titleFont.setWeight(QFont::DemiBold); painter.setFont(titleFont);
        painter.setPen(isChecked() ? accent : ink);
        painter.drawText(QRect(8, 77, width() - 16, 24), Qt::AlignCenter, text());
        titleFont.setWeight(QFont::Normal); titleFont.setPixelSize(11); painter.setFont(titleFont);
        painter.setPen(QColor("#94a0a8"));
        painter.drawText(QRect(12, 102, width() - 24, 20), Qt::AlignCenter,
                         painter.fontMetrics().elidedText(m_detail, Qt::ElideRight, width() - 32));
        painter.setPen(QPen(QColor("#94a0a8"), 1.8, Qt::SolidLine, Qt::RoundCap));
        const auto c = arrowRect().center();
        painter.drawPolyline(QPolygonF{{c.x() - 5., c.y() - 2.}, {c.x() + 0., c.y() + 3.}, {c.x() + 5., c.y() - 2.}});
    }
private:
    QString m_detail;
    QMenu *m_menu;
};


// Soft hover / press feedback drawn over a button; Qt style sheets cannot animate, so this fades a tint in and out.
class ButtonGlow : public QWidget {
public:
    explicit ButtonGlow(QAbstractButton *button) : QWidget(button), m_button(button) {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        m_fade.setDuration(130); m_fade.setEasingCurve(QEasingCurve::OutCubic);
        connect(&m_fade, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) { m_level = v.toReal(); update(); });
        button->installEventFilter(this);
        setGeometry(button->rect()); raise(); show();
    }
protected:
    bool eventFilter(QObject *, QEvent *event) override {
        switch (event->type()) {
        case QEvent::Enter: case QEvent::HoverEnter: go(1); break;
        case QEvent::Leave: case QEvent::HoverLeave: go(0); break;
        case QEvent::MouseButtonPress: m_pressed = true; go(2); break;
        case QEvent::MouseButtonRelease: m_pressed = false; go(m_button->underMouse() ? 1 : 0); break;
        case QEvent::Resize: setGeometry(m_button->rect()); break;
        case QEvent::EnabledChange: if (!m_button->isEnabled()) { m_fade.stop(); m_level = 0; update(); } break;
        default: break;
        }
        return false;
    }
    void paintEvent(QPaintEvent *) override {
        if (m_level <= 0.01 || !m_button->isEnabled()) return;
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const double radius = m_button->property("glowRadius").isValid() ? m_button->property("glowRadius").toDouble() : 8;
        painter.setPen(Qt::NoPen);
        // 0..1 lightens on hover, 1..2 darkens on press
        painter.setBrush(m_level <= 1 ? QColor(255, 255, 255, int(m_level * 22)) : QColor(0, 0, 0, int((m_level - 1) * 70)));
        painter.drawRoundedRect(QRectF(rect()).adjusted(.5, .5, -.5, -.5), radius, radius);
    }
private:
    void go(qreal target) {
        m_fade.stop(); m_fade.setStartValue(m_level); m_fade.setEndValue(target); m_fade.start();
    }
    QAbstractButton *m_button;
    QVariantAnimation m_fade;
    qreal m_level = 0;
    bool m_pressed = false;
};

class CaptureSwitch : public QPushButton {
public:
    CaptureSwitch() {
        setCheckable(true);
        setFixedSize(46, 30);
    }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF track(2, 4, 42, 22);
        painter.setPen(hasFocus() ? QPen(window()->palette().color(QPalette::Accent), 1.5) : QPen(Qt::NoPen));
        painter.setBrush(isChecked() ? window()->palette().color(QPalette::Accent) : QColor("#2a3136"));
        if (!isEnabled()) painter.setOpacity(0.55);
        painter.drawRoundedRect(track, 11, 11);
        painter.setPen(Qt::NoPen);
        painter.setBrush(isDown() ? QColor("#a5b4b2") : QColor("#eef4f1"));
        painter.drawEllipse(QPointF(isChecked() ? 33 : 13, 15), 8, 8);
    }
};

class CaptureRing : public QWidget {
public:
    CaptureRing() { setFixedSize(46, 46); }

    void setState(bool ready, bool recording) {
        if (m_ready == ready && m_recording == recording) return;
        m_ready = ready;
        m_recording = recording;
        update();
    }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF ring = QRectF(rect()).adjusted(3, 3, -3, -3);
        const auto accent = m_recording ? QColor("#e0a163") : window()->palette().color(QPalette::Accent);
        painter.setPen(QPen(QColor("#2b3236"), 3));
        painter.drawEllipse(ring);
        if (m_ready) {
            painter.setPen(QPen(accent, 3, Qt::SolidLine, Qt::RoundCap));
            painter.drawArc(ring, 120 * 16, 290 * 16);
        }
        painter.setPen(Qt::NoPen);
        painter.setBrush(m_ready ? accent : QColor("#5d6870"));
        painter.drawEllipse(ring.center(), 4, 4);
    }
private:
    bool m_ready = false;
    bool m_recording = false;
};
}

void Dashboard::setAccent(const QString &name) {
    applyTheme(this, name);
    const auto icon = accentIcon(name);
    setWindowIcon(icon);
    m_logo->setPixmap(icon.pixmap(22, 22));
}

Dashboard::Dashboard(const GsrCapabilities &caps) {
    setObjectName("dashboard");
    setWindowTitle("Shadowless");

    m_panel = new QFrame(this);
    m_panel->setObjectName("shadowlessPanel");
    auto *shell = new QVBoxLayout(m_panel);
    shell->setContentsMargins(24, 20, 24, 18);
    shell->setSpacing(16);

    m_chrome = new QWidget(this); // floats at the screen edge, outside the content panel
    m_chrome->setObjectName("dashboardChrome");
    auto *header = new QHBoxLayout(m_chrome);
    header->setContentsMargins(0, 0, 0, 0);
    m_logo = new QLabel;
    header->addWidget(m_logo);
    auto *brand = new QLabel("Shadowless");
    brand->setObjectName("brand");
    header->addWidget(brand);
    header->addStretch();
    m_layers = new QPushButton("Capture overlays");
    m_layers->setObjectName("openOverlays");
    header->addWidget(m_layers);
    m_back = new QPushButton("Dashboard");
    header->addWidget(m_back);
    m_settingsButton = new QPushButton("Settings");
    m_settingsButton->setObjectName("openSettings");
    m_settingsButton->setIcon(QIcon(":/shadowless/icons/settings.svg"));
    header->addWidget(m_settingsButton);
    auto *close = new QPushButton("×");
    close->setObjectName("hideOverlay");
    close->setAccessibleName("Hide Shadowless");
    close->setText("Esc  ×");
    close->setToolTip("Hide Shadowless and return to the game");
    header->addWidget(close);

    m_pages = new QStackedWidget;
    m_home = new QWidget;
    m_homeLayout = new QBoxLayout(QBoxLayout::LeftToRight, m_home);
    m_homeLayout->setContentsMargins(0, 0, 0, 0);
    m_homeLayout->setSpacing(16);

    auto *controls = new QWidget;
    auto *homeLayout = new QVBoxLayout(controls);
    homeLayout->setContentsMargins(0, 0, 0, 0);
    homeLayout->setSpacing(8);
    m_controlScroll = new QScrollArea;
    m_controlScroll->setWidgetResizable(true);
    m_controlScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_controlScroll->setWidget(controls);
    m_homeLayout->addWidget(m_controlScroll, 1);

    auto *card = new QFrame;
    card->setObjectName("panel");
    auto *statusLayout = new QHBoxLayout(card);
    statusLayout->setContentsMargins(14, 10, 14, 10);
    statusLayout->setSpacing(14);
    m_ring = new CaptureRing;
    statusLayout->addWidget(m_ring);
    auto *statusText = new QVBoxLayout;
    statusText->setSpacing(2);
    m_status = new QLabel;
    m_status->setObjectName("statusTitle");
    statusText->addWidget(m_status);
    m_detail = new QLabel;
    m_detail->setTextFormat(Qt::PlainText);
    m_detail->setProperty("muted", true);
    m_detail->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    statusText->addWidget(m_detail);
    statusLayout->addLayout(statusText, 1);
    m_pause = new QPushButton("Pause recording");
    m_pause->setObjectName("pauseRecording");
    m_pause->setAccessibleName("Pause recording");
    m_pause->hide();
    statusLayout->addWidget(m_pause);
    connect(m_pause, &QPushButton::clicked, this, &Dashboard::recordingPauseRequested);
    homeLayout->addWidget(card);

    auto *overlayCard = new QFrame;
    overlayCard->setObjectName("panel");
    auto *overlayRow = new QHBoxLayout(overlayCard);
    overlayRow->setContentsMargins(14, 10, 14, 10);
    overlayRow->setSpacing(14);
    auto *overlayMark = new QLabel;
    overlayMark->setPixmap(QIcon(":/shadowless/icons/layers.svg").pixmap(26, 26));
    overlayMark->setObjectName("overlayMark");
    overlayMark->setFixedSize(46, 46);
    overlayMark->setAlignment(Qt::AlignCenter);
    overlayRow->addWidget(overlayMark);
    auto *overlayText = new QVBoxLayout;
    overlayText->setSpacing(2);
    auto *overlayTitle = new QLabel("Capture overlays");
    overlayTitle->setObjectName("statusTitle");
    overlayText->addWidget(overlayTitle);
    m_layerStatus = new QLabel;
    m_layerStatus->setProperty("muted", true);
    overlayText->addWidget(m_layerStatus);
    overlayRow->addLayout(overlayText, 1);
    m_layerToggle = new CaptureSwitch;
    m_layerToggle->setObjectName("overlayToggle");
    m_layerToggle->setAccessibleName("Enable capture overlays");
    overlayRow->addWidget(m_layerToggle);
    m_overlayPreview = new OverlayCanvas;
    m_overlayPreview->setObjectName("overlayPreview");
    m_overlayPreview->setProperty("compactPreview", true);
    m_overlayPreview->setCaptureVisible(false);
    m_overlayPreview->setAccessibleName("Custom overlay preview");
    m_overlayPreview->setMinimumSize(160, 90);
    m_overlayPreview->setFixedHeight(120);
    m_overlayPreview->setAttribute(Qt::WA_TransparentForMouseEvents);

    auto *modePanel = new QFrame;
    auto *modes = new QHBoxLayout(modePanel);
    modes->setContentsMargins(0, 2, 0, 2);
    modes->setSpacing(12);
    auto *replayTile = new CaptureModeTile("Instant Clip", "Start / stop instant replay");
    auto *recordTile = new CaptureModeTile("Recording", "Start / stop recording");
    m_replay = replayTile; m_record = recordTile;
    m_replayToggle = replayTile->menu()->addAction(QIcon(":/shadowless/icons/replay.svg"), "Turn Instant Clip on", this, [this] { emit recordingMode(false); });
    m_replaySave = replayTile->menu()->addAction(QIcon(":/shadowless/icons/film.svg"), "Save replay", this, &Dashboard::saveReplay);
    m_recordToggle = recordTile->menu()->addAction(QIcon(":/shadowless/icons/record.svg"), "Start recording", this, [this] { emit recordingMode(true); });
    m_recordPause = recordTile->menu()->addAction(QIcon(":/shadowless/icons/pause.svg"), "Pause recording", this, &Dashboard::recordingPauseRequested);
    for (auto *mode : {m_replay, m_record}) {
        mode->setObjectName("captureMode");
        mode->setCheckable(true);
        modes->addWidget(mode, 1);
    }
    homeLayout->addWidget(overlayCard);
    homeLayout->addWidget(modePanel);
    homeLayout->addWidget(m_overlayPreview);

    auto *audioCard = new QFrame;
    audioCard->setObjectName("panel");
    auto *quick = new QGridLayout(audioCard);
    quick->setContentsMargins(0, 0, 0, 0);
    quick->setHorizontalSpacing(8);
    quick->setVerticalSpacing(8);
    quick->setColumnStretch(0, 1);
    audioCard->setObjectName("audioGroup");
    m_systemAudio = new QPushButton("System audio");
    m_microphone = new QPushButton("Microphone");
    m_gameAudio = new QPushButton("Game");
    m_musicAudio = new QPushButton("Music");
    m_gameAudio->setObjectName("gameCaptureAudio");
    m_musicAudio->setObjectName("musicCaptureAudio");
    // Windows layout: one card per source = mute toggle (icon + name) + "100% ▾" button that opens a volume popup.
    auto addAudioRow = [this, quick](int row, int gridRow, int gridCol, QPushButton *button, QSlider *&slider, QLabel *&percent) {
        auto *card = new QFrame;
        card->setObjectName("audioCard");
        auto *cardLayout = new QHBoxLayout(card);
        cardLayout->setContentsMargins(0, 0, 0, 0);
        cardLayout->setSpacing(0);
        button->setCheckable(true);
        button->setProperty("audioRow", true);
        button->setIconSize(QSize(18, 18));
        button->setMinimumWidth(0);
        cardLayout->addWidget(button, 1);
        slider = new ClickToAdjust<QSlider>(Qt::Horizontal);
        slider->setProperty("audioRow", true);
        slider->setMinimumHeight(24);
        slider->setRange(0, 100);
        slider->setAccessibleName(button->text() + " capture volume");
        slider->setToolTip("Recording volume");
        auto *value = new QPushButton("100% ▾");
        value->setProperty("audioValue", true);
        value->setAccessibleName(button->text() + " volume");
        value->setToolTip("Recording volume");
        auto *popup = new QFrame(this, Qt::Popup);
        popup->setObjectName("volumePopup");
        auto *popupLayout = new QVBoxLayout(popup);
        popupLayout->setContentsMargins(16, 14, 16, 14);
        popupLayout->setSpacing(10);
        auto *head = new QHBoxLayout;
        auto *title = new QLabel(button->text());
        title->setStyleSheet("font-weight: 600;");
        auto *popupPercent = new QLabel("100%");
        popupPercent->setProperty("muted", true);
        head->addWidget(title, 1);
        head->addWidget(popupPercent);
        popupLayout->addLayout(head);
        popupLayout->addWidget(slider);
        percent = popupPercent;
        slider->setProperty("valueButton", QVariant::fromValue<QObject *>(value));
        popup->setFixedWidth(260);
        cardLayout->addWidget(value);
        connect(value, &QPushButton::clicked, this, [value, popup, button, title] {
            title->setText(button->text());
            popup->adjustSize();
            popup->move(value->mapToGlobal(QPoint(value->width() - popup->width(), value->height() + 8)));
            popup->show();
            popup->raise();
        });
        connect(slider, &QSlider::valueChanged, this, [value, popupPercent](int v) {
            value->setText(QString("%1% ▾").arg(v));
            popupPercent->setText(QString("%1%").arg(v));
        });
        connect(slider, &QSlider::valueChanged, this, [this, row](int v) {
            if (row == 1 || row == 2) emit roleVolumeChanged(row + 1, v);
            else emit volumeChanged(row == 3, v);
        });
        quick->addWidget(card, gridRow, gridCol);
    };
    addAudioRow(0, 0, 0, m_systemAudio, m_systemVolume, m_systemPercent);
    addAudioRow(1, 2, 0, m_gameAudio, m_gameVolume, m_gamePercent);
    addAudioRow(2, 3, 0, m_musicAudio, m_musicVolume, m_musicPercent);
    addAudioRow(3, 1, 0, m_microphone, m_microphoneVolume, m_microphonePercent);
    m_gameVolume->setObjectName("gameCaptureVolume");
    m_musicVolume->setObjectName("musicCaptureVolume");
    m_audioSettings = new QPushButton("Audio settings");
    m_audioSettings->setObjectName("settingsLink");
    quick->addWidget(m_audioSettings, 4, 0);
    m_audioTrackSummary = new QLabel;
    m_audioTrackSummary->setObjectName("audioTrackSummary");
    m_audioTrackSummary->setWordWrap(true);
    m_audioTrackSummary->setProperty("muted", true);
    quick->addWidget(m_audioTrackSummary, 5, 0);
    m_fourTrackSetup = new QPushButton("Set up four tracks");
    m_fourTrackSetup->setObjectName("fourTrackSetup");
    quick->addWidget(m_fourTrackSetup, 6, 0);
    connect(m_fourTrackSetup, &QPushButton::clicked, this, [this] {
        showPage(m_settings, 560);
        emit fourTrackSetupRequested();
        m_settings->showAudio();
    });
    homeLayout->addWidget(audioCard);

    // Format, free space and save folder share one card so they read as a group, separate from the audio card above.
    auto *storageCard = new QFrame;
    storageCard->setObjectName("panel");
    auto *storage = new QVBoxLayout(storageCard);
    storage->setContentsMargins(12, 8, 12, 10);
    storage->setSpacing(6);
    auto *summary = new QHBoxLayout;
    m_format = new QPushButton;
    m_format->setObjectName("settingsLink");
    m_format->setAccessibleName("Open video settings");
    m_format->setToolTip("Open video settings");
    m_disk = new QLabel;
    m_disk->setProperty("muted", true);
    summary->addWidget(m_format);
    summary->addStretch();
    summary->addWidget(m_disk);
    storage->addLayout(summary);
    m_diskUsage = new QProgressBar;
    m_diskUsage->setAccessibleName("Output volume usage");
    m_diskUsage->setRange(0, 100);
    m_diskUsage->setTextVisible(false);
    m_diskUsage->setFixedHeight(4);
    m_diskUsage->hide();
    storage->addWidget(m_diskUsage);
    auto *folderRow = new QHBoxLayout;
    m_folder = new QPushButton;
    m_folder->setObjectName("settingsLink");
    m_folder->setAccessibleName("Change save folder");
    folderRow->addWidget(m_folder, 1);
    m_openFolder = new QPushButton("Open folder");
    m_openFolder->setObjectName("clipsFolder");
    folderRow->addWidget(m_openFolder);
    storage->addLayout(folderRow);
    homeLayout->addWidget(storageCard);
    homeLayout->addStretch();
    auto *libraryCard = new QFrame;
    libraryCard->setObjectName("panel");
    auto *libraryLayout = new QVBoxLayout(libraryCard);
    libraryLayout->setContentsMargins(14, 10, 12, 12);
    m_library = new Library;
    libraryLayout->addWidget(m_library, 1);
    m_homeLayout->addWidget(libraryCard, 1);

    m_layers->setIcon(QIcon(":/shadowless/icons/layers.svg"));
    m_settingsButton->setIcon(QIcon(":/shadowless/icons/settings.svg"));
    m_back->setIcon(QIcon(":/shadowless/icons/back.svg"));
    m_replay->setIcon(QIcon(":/shadowless/icons/replay.svg"));
    m_record->setIcon(QIcon(":/shadowless/icons/record.svg"));
    m_systemAudio->setIcon(QIcon(":/shadowless/icons/speaker.svg"));
    m_microphone->setIcon(QIcon(":/shadowless/icons/microphone.svg"));
    m_gameAudio->setIcon(QIcon(":/shadowless/icons/game.svg"));
    m_musicAudio->setIcon(QIcon(":/shadowless/icons/music.svg"));
    m_openFolder->setIcon(QIcon(":/shadowless/icons/folder.svg"));
    m_folder->setIcon(QIcon(":/shadowless/icons/folder.svg"));
    m_format->setIcon(QIcon(":/shadowless/icons/video.svg"));
    m_audioSettings->setIcon(QIcon(":/shadowless/icons/settings.svg"));

    m_settings = new SettingsPage(caps);
    m_editor = new OverlayEditor;
    m_pages->addWidget(m_home);
    m_pages->addWidget(m_settings);
    m_pages->addWidget(m_editor);
    m_player = new ClipPlayer;
    m_pages->addWidget(m_player);
    connect(m_library, &Library::clipRequested, this, [this](const QString &path) {
        showPage(m_player, 980);
        m_player->open(path);
    });
    connect(m_player, &ClipPlayer::back, this, &Dashboard::home);
    connect(m_player, &ClipPlayer::editingChanged, this, [this] { positionPanel(); });
    connect(m_library, &Library::allClipsChanged, m_player, &ClipPlayer::setClips);
    connect(m_library, &Library::thumbnailReady, m_player, &ClipPlayer::setThumbnail);
    auto *playback = m_player->findChild<MpvPlayer *>();
    connect(playback, &MpvPlayer::stateChanged, m_library, [this, playback] { m_library->setPreviewActive(!playback->source().isEmpty()); });
    connect(m_player, &ClipPlayer::clipExported, this, [this] { m_library->refresh(m_directory); });
    connect(m_player, &ClipPlayer::fullscreenChanged, this, [this, shell](bool fullscreen) {
        const bool chrome = !fullscreen && m_pages->currentWidget() != m_player;
        m_chrome->setVisible(chrome);
        m_footer->setVisible(chrome);
        m_divider->setVisible(chrome);
        shell->setContentsMargins(fullscreen ? 0 : 20, fullscreen ? 0 : 16, fullscreen ? 0 : 20, fullscreen ? 0 : 16);
        positionPanel();
    });
    shell->addWidget(m_pages, 1);
    auto *rendering = new QLabel("rendering...");
    rendering->setObjectName("clipRenderingStatus");
    rendering->hide();
    shell->addWidget(rendering);
    connect(m_player, &ClipPlayer::exportingChanged, rendering, &QWidget::setVisible);
    m_notice = new QLabel;
    m_notice->setObjectName("notice");
    m_notice->setTextFormat(Qt::PlainText);
    m_notice->setWordWrap(true);
    m_notice->hide();
    // Long or stacked messages scroll inside a bounded box instead of being clipped or pushing the page.
    m_noticeBox = new NoticeScroll(m_notice);
    m_noticeBox->hide();
    shell->addWidget(m_noticeBox);
    auto *divider = new QFrame;
    m_divider = divider;
    divider->setObjectName("divider");
    divider->setFixedHeight(1);
    shell->addWidget(divider);
    m_footer = new QLabel;
    m_footer->setObjectName("footer");
    m_footer->setProperty("muted", true);
    shell->addWidget(m_footer);

    connect(close, &QPushButton::clicked, this, &Dashboard::hideRequested);
    connect(m_back, &QPushButton::clicked, this, &Dashboard::escape);

    connect(m_systemAudio, &QPushButton::clicked, this, [this](bool checked) { emit audioChanged(false, checked); });
    connect(m_microphone, &QPushButton::clicked, this, [this](bool checked) { emit audioChanged(true, checked); });
    connect(m_gameAudio, &QPushButton::clicked, this, [this](bool checked) { emit roleAudioChanged(2, checked); });
    connect(m_musicAudio, &QPushButton::clicked, this, [this](bool checked) { emit roleAudioChanged(3, checked); });
    auto openSettings = [this] {
        emit settingsRequested();
        showPage(m_settings, 560);
    };
    connect(m_settingsButton, &QPushButton::clicked, this, openSettings);
    connect(m_audioSettings, &QPushButton::clicked, this, [this, openSettings] {
        openSettings();
        m_settings->showAudio();
    });
    connect(m_format, &QPushButton::clicked, this, [this, openSettings] {
        openSettings();
        m_settings->showVideo();
    });
    connect(m_folder, &QPushButton::clicked, this, [this, openSettings] {
        openSettings();
        m_settings->showStorage();
    });
    connect(m_layerToggle, &QPushButton::clicked, this, [this](bool checked) {
        m_layerToggle->setChecked(!checked);
        emit overlayEnabled(checked);
    });
    connect(m_layers, &QPushButton::clicked, this, [this] {
        emit editorRequested();
        showPage(m_editor, 980);
    });
    connect(m_settings, &SettingsPage::cancelled, this, &Dashboard::home);
    connect(m_editor, &OverlayEditor::back, this, &Dashboard::home);
    connect(m_library, &Library::storageChanged, this, [this](qint64 available, qint64 total) {
        m_disk->setText(total > 0 ? QLocale().formattedDataSize(available, 1) + " free" : QString());
        m_diskUsage->setVisible(total > 0);
        if (width() < 800) positionPanel();
        if (total > 0) m_diskUsage->setValue(qBound(0, qRound(100. * (1. - double(available) / total)), 100));
    });
    connect(m_openFolder, &QPushButton::clicked, this, [this] {
        if (QDesktopServices::openUrl(QUrl::fromLocalFile(m_directory))) emit hideRequested();
        else notice("Could not open the clips folder.");
    });
    auto *escape = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(escape, &QShortcut::activated, this, &Dashboard::escape);
    setAccent("violet");
    setShortcutKeys({"Alt+Z", "Ctrl+Shift+F10", "Alt+F10", "Alt+F9"});
    resize(1280, 720);
    home();
    qApp->installEventFilter(this);
}

void Dashboard::setInputOverlay(const InputOverlaySettings &settings) { m_player->setInputOverlay(settings); }

void Dashboard::updateState(const GsrOptions &o, const OverlayScene &scene, bool active, bool ready, bool saving, bool stopping,
                            bool replayRecording, bool busy, bool paused, bool pausePending, bool pauseKnown) {
    m_overlayPreview->setVisible(!scene.layers.isEmpty());
    m_overlayPreview->setScene(scene, -1);
    const QString mode = o.continuousRecording ? "Recording" : "Instant Replay";
    const QString recordingStatus = !pauseKnown ? "Recording pause state unknown" :
                                    pausePending ? "Changing recording pause state…" : paused ? "Recording paused" : mode;
    m_status->setText(stopping ? "Finishing capture…" : active && !ready ? "Starting capture…" :
                      ready ? (o.continuousRecording ? recordingStatus : replayRecording ? "Recording + Instant Replay" : "Instant Replay is on")
                            : mode + " is off");
    const QString source = o.source == "portal" ? "Screen picker" : o.source;
    m_detail->setText(o.continuousRecording ? source + " · Continuous recording" :
                     QString("Last %1 sec · %2").arg(o.bufferSeconds).arg(source));
    m_detail->setToolTip(m_detail->text());
    static_cast<CaptureRing *>(m_ring)->setState(ready && !stopping && !paused && pauseKnown, o.continuousRecording);
    m_pause->setVisible(active && ready && o.continuousRecording);
    m_pause->setEnabled(!busy && !pausePending && !saving && !stopping && pauseKnown);
    m_pause->setText(pausePending ? "Please wait…" : paused ? "Resume recording" : "Pause recording");
    m_pause->setAccessibleName(m_pause->text());
    m_pause->setToolTip(pauseKnown ? "Pause or resume this recording without creating another file" : "Stop and save the recording before starting again");
    m_replay->setChecked(active && !o.continuousRecording);
    m_record->setChecked(active && (o.continuousRecording || replayRecording));
    const bool available = !stopping && !saving && !busy;
    m_replay->setEnabled(available && (!active || !o.continuousRecording));
    m_record->setEnabled(available && (!active || ready || o.continuousRecording));
    m_replay->setToolTip(active && o.continuousRecording ? "Stop recording before starting Instant Clip" :
                         replayRecording ? "Stop capture and finish the recording" :
                         m_replay->isChecked() ? "Stop instant replay" : "Start instant replay");
    m_record->setToolTip(m_record->isChecked() ? "Stop and save recording" : "Start recording");
    m_replayToggle->setText(m_replay->isChecked() ? "Turn Instant Clip off" : "Turn Instant Clip on");
    m_replayToggle->setEnabled(m_replay->isEnabled());
    m_replaySave->setEnabled(available && ready && !o.continuousRecording);
    m_recordToggle->setText(m_record->isChecked() ? "Stop and save recording" : "Start recording");
    m_recordToggle->setEnabled(m_record->isEnabled());
    m_recordPause->setText(paused ? "Resume recording" : "Pause recording");
    m_recordPause->setEnabled(m_pause->isVisible() && m_pause->isEnabled());
    auto updateAudio = [active, stopping](QPushButton *button, QSlider *slider, QLabel *percent,
                                           bool enabled, bool muted, int volume) {
        const QSignalBlocker blocker(slider);
        button->setChecked(enabled && !muted);
        button->setEnabled(!stopping && (enabled || !active));
        button->setToolTip(enabled ? (muted ? "Unmute in recordings" : "Mute in recordings") :
                           (active ? "Stop capture to enable this source" : "Enable this source"));
        slider->setEnabled(enabled && !stopping);
        slider->setValue(volume);
        percent->setText(QString("%1%").arg(volume));
        if (auto *value = qobject_cast<QPushButton *>(slider->property("valueButton").value<QObject *>())) {
            value->setText(QString("%1% ▾").arg(volume));
            value->setEnabled(enabled && !stopping);
        }
    };
    const bool grouped = o.fourTrackAudio || o.dynamicAudioEnabled();
    m_systemAudio->setText(grouped ? "PC" : "System audio");
    m_systemVolume->setAccessibleName(grouped ? "PC capture volume" : "System audio capture volume");
    updateAudio(m_systemAudio, m_systemVolume, m_systemPercent, o.fourTrackAudio || o.systemAudio, o.levels.systemMuted, o.levels.systemVolume);
    updateAudio(m_microphone, m_microphoneVolume, m_microphonePercent, o.microphone, o.levels.microphoneMuted, o.levels.microphoneVolume);
    m_systemAudio->parentWidget()->setVisible(!o.applicationAudio || o.fourTrackAudio);
    updateAudio(m_gameAudio, m_gameVolume, m_gamePercent, grouped, o.levels.gameMuted, o.levels.gameVolume);
    updateAudio(m_musicAudio, m_musicVolume, m_musicPercent, o.fourTrackAudio, o.levels.musicMuted, o.levels.musicVolume);
    m_gameAudio->parentWidget()->setVisible(grouped);
    m_musicAudio->parentWidget()->setVisible(o.fourTrackAudio);
    QStringList audioNames;
    if (o.dynamicAudioEnabled()) {
        audioNames = {"PC", "Game"};
        if (o.microphone) audioNames.append("Microphone");
    } else if (o.fourTrackAudio) audioNames = {"PC", "Game", "Music", "Microphone"};
    else if (o.applicationAudio) audioNames = {QString("%1 configured").arg(o.audioTracks().size())};
    else {
        if (o.systemAudio) audioNames.append("PC");
        if (o.microphone) audioNames.append("Microphone");
    }
    const bool mixed = !o.dynamicAudioEnabled() && !o.applicationAudio && !o.separateAudioTracks && audioNames.size() > 1;
    m_audioTrackSummary->setText((o.dynamicAudioEnabled() ? "Audio groups: " : "Audio tracks: ") + (audioNames.isEmpty() ? "none" :
        audioNames.join(mixed ? " + " : " · ")) + (mixed ? " (mixed)" : ""));
    m_fourTrackSetup->setVisible(!o.fourTrackAudio && !o.dynamicAudioEnabled());
    m_fourTrackSetup->setEnabled(!active && !stopping);
    m_fourTrackSetup->setToolTip(active ? "Stop capture before setting up four tracks" :
        "Choose separate PC, Game, Music and Microphone recording tracks");
    m_audioSettings->setText(o.applicationAudio ? "Application audio settings" : "Audio settings");
    m_replaySave->setText(saving ? "Saving replay…" : QString("Save replay (last %1 sec)").arg(o.bufferSeconds));

    m_format->setText(QString("%1  ·  %2 FPS  ·  %3").arg(o.codec.toUpper()).arg(o.frameRate).arg(o.container.toUpper()));
    m_layerToggle->setChecked(scene.enabled);
    const auto layerCount = scene.layers.size();
    const auto layerText = layerCount == 1 ? QString("1 layer") : QString("%1 layers").arg(layerCount);
    m_layerStatus->setText((scene.enabled ? "On · " : "Off · ") + layerText);
    m_directory = o.outputDirectory;
    m_openFolder->setEnabled(!m_directory.isEmpty());
    m_folder->setText(m_directory.isEmpty() ? "Clips folder" :
                      m_folder->fontMetrics().elidedText(m_directory, Qt::ElideMiddle, 220));
    m_folder->setToolTip(m_directory);
    if (width() < 800) positionPanel();
}

void Dashboard::notice(const QString &message) {
    m_notice->setText(message);
    m_notice->setToolTip(message);
    m_notice->setProperty("tone", message.startsWith("Saved ") ? "info" : "warning"); // confirmations vs problems
    m_notice->style()->unpolish(m_notice); m_notice->style()->polish(m_notice);
    m_noticeBox->setVisible(m_pages->currentWidget() == m_home && !message.isEmpty());
    m_noticeBox->setFixedHeight(m_noticeBox->sizeHint().height());
}

void Dashboard::setShortcutKeys(const QStringList &keys) {
    m_overlayKey = QKeySequence(keys.value(0), QKeySequence::PortableText);
    m_footer->setText(keys.value(0) + "  Show / hide    ·    " + keys.value(1) + "  Save replay");
    m_footer->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_status->setToolTip(keys.value(2) + "  Start / stop capture · " + keys.value(3) + "  Pause / resume recording");
}

void Dashboard::shortcutStatus(const QString &message) {
    m_settings->setShortcutStatus(message);
    m_footer->setToolTip(m_footer->text() + "\n" + message);
}

void Dashboard::setOverlayBusy(bool busy) {
    m_layerToggle->setEnabled(!busy);
    m_layers->setEnabled(!busy);
    m_layers->setToolTip(busy ? "Applying capture overlay changes" : "Edit capture overlays");
    m_layerToggle->setToolTip(busy ? "Applying capture overlay changes" : "Include layers in new capture frames");
}

void Dashboard::refreshLibrary(const QString &directory) { m_library->refresh(directory); }

void Dashboard::showPage(QWidget *page, int width) {
    const bool dashboard = page == m_home;
    const bool chrome = page != m_player;
    m_chrome->setVisible(chrome);
    m_footer->setVisible(chrome);
    m_back->setVisible(!dashboard);
    m_settingsButton->setVisible(dashboard);
    m_layers->setVisible(dashboard);
    m_panelWidth = width;
    m_panelHeight = dashboard ? 700 : 780;
    m_panel->setProperty("floating", dashboard); // home: cards float on the dimmed screen like the Windows overlay
    m_panel->style()->unpolish(m_panel); m_panel->style()->polish(m_panel);
    m_divider->setVisible(chrome && !dashboard);
    m_pages->setCurrentWidget(page);
    m_noticeBox->setVisible(dashboard && !m_notice->text().isEmpty());
    positionPanel();
}

void Dashboard::home() { showPage(m_home, 1034); }

void Dashboard::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    positionPanel();
}

void Dashboard::positionPanel() {
    if (m_player->fullscreen()) {
        m_panel->setGeometry(rect());
        return;
    }
    const bool onPlayer = m_pages->currentWidget() == m_player;
    const bool editing = onPlayer && m_player->editing();
    const bool chromeShown = !m_chrome->isHidden();
    const int top = chromeShown ? 72 : 12, availableHeight = qMax(0, height() - top - 12);
    if (chromeShown) { m_chrome->setGeometry(28, 18, qMax(0, width() - 56), 40); m_chrome->raise(); }
    // Watching scales with the window (but stays a card); editing takes the whole window.
    const int playerWidth = qBound(m_panelWidth, width() * 70 / 100, 1200), playerHeight = qBound(m_panelHeight, availableHeight * 80 / 100, 820);
    const auto panelWidth = editing ? qMax(0, width() - 24) : qBound(0, onPlayer ? playerWidth : m_panelWidth, qMax(0, width() - 24));
    const auto panelHeight = editing ? availableHeight : qBound(0, onPlayer ? playerHeight : m_panelHeight, availableHeight);
    const bool narrow = width() < 800;
    m_homeLayout->setDirection(narrow ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
    const int controlsHeight = qBound(0, panelHeight - 245, m_controlScroll->widget()->sizeHint().height());
    m_controlScroll->setMinimumHeight(narrow ? controlsHeight : 0);
    m_controlScroll->setMaximumHeight(narrow ? controlsHeight : QWIDGETSIZE_MAX);
    m_panel->setGeometry((width() - panelWidth) / 2, top + (availableHeight - panelHeight) / 2, panelWidth, panelHeight);
}

void Dashboard::escape() {
    if (m_pages->currentWidget() == m_player && m_player->handleEscape()) {
        return;
    }
    if (m_pages->currentWidget() == m_home) emit hideRequested();
    else {
        if (m_pages->currentWidget() == m_editor) m_editor->flush();
        home();
    }
}

bool Dashboard::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::Show) {
        auto *button = qobject_cast<QPushButton *>(watched);
        if (button && isAncestorOf(button) && !button->property("glow").toBool() && !dynamic_cast<CaptureSwitch *>(button)) {
            button->setProperty("glow", true);
            new ButtonGlow(button);
        }
        return false;
    }
    if (!isVisible() || (event->type() != QEvent::ShortcutOverride && event->type() != QEvent::KeyPress)) return false;
    auto *key = static_cast<QKeyEvent *>(event);
    if (m_overlayKey != QKeySequence(key->keyCombination())) return false;
    // Include popup widgets and take precedence over shortcut editors.
    for (auto *widget = qobject_cast<QWidget *>(watched); widget; widget = widget->parentWidget()) {
        if (widget != this) continue;
        event->accept();
        if (event->type() == QEvent::KeyPress && !key->isAutoRepeat()) emit hideRequested();
        return true;
    }
    return false;
}

void Dashboard::closeEvent(QCloseEvent *event) {
    event->ignore();
    emit hideRequested();
}

void Dashboard::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(8, 10, 12, 205));
}
} // namespace Shadowless
