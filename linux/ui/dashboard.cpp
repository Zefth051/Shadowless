#include "dashboard.h"
#include "value_controls.h"
#include <QSlider>
#include "library.h"
#include "overlay_editor.h"
#include "settings_page.h"
#include "theme.h"
#include <QButtonGroup>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QFrame>
#include <QLabel>
#include <QBoxLayout>
#include <QScrollArea>
#include <QLocale>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace Shadowless {
namespace {
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
        painter.setPen(hasFocus() ? QPen(palette().color(QPalette::Accent), 1.5) : QPen(Qt::NoPen));
        painter.setBrush(isChecked() ? palette().color(QPalette::Accent) : QColor("#2a3136"));
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
        const auto accent = m_recording ? QColor("#e0a163") : palette().color(QPalette::Accent);
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
    shell->setContentsMargins(16, 10, 16, 10);
    shell->setSpacing(12);

    auto *header = new QHBoxLayout;
    m_logo = new QLabel;
    header->addWidget(m_logo);
    auto *brand = new QLabel("Shadowless");
    brand->setObjectName("brand");
    header->addWidget(brand);
    header->addStretch();
    m_layers = new QPushButton("Capture overlays");
    m_layers->setObjectName("openOverlays");
    header->addWidget(m_layers);
    m_back = new QPushButton("‹  Dashboard");
    header->addWidget(m_back);
    m_settingsButton = new QPushButton("Settings");
    m_settingsButton->setObjectName("openSettings");
    header->addWidget(m_settingsButton);
    auto *close = new QPushButton("×");
    close->setObjectName("hideOverlay");
    close->setAccessibleName("Hide Shadowless");
    close->setText("Esc  ×");
    close->setToolTip("Hide Shadowless and return to the game");
    header->addWidget(close);
    shell->addLayout(header);

    m_pages = new QStackedWidget;
    m_home = new QWidget;
    m_homeLayout = new QBoxLayout(QBoxLayout::LeftToRight, m_home);
    m_homeLayout->setContentsMargins(0, 0, 0, 0);
    m_homeLayout->setSpacing(20);

    auto *controls = new QWidget;
    auto *homeLayout = new QVBoxLayout(controls);
    homeLayout->setContentsMargins(0, 0, 0, 0);
    homeLayout->setSpacing(12);
    m_controlScroll = new QScrollArea;
    m_controlScroll->setWidgetResizable(true);
    m_controlScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_controlScroll->setWidget(controls);
    m_homeLayout->addWidget(m_controlScroll, 1);

    auto *card = new QFrame;
    card->setObjectName("panel");
    auto *statusLayout = new QHBoxLayout(card);
    statusLayout->setContentsMargins(12, 10, 12, 10);
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
    m_toggle = new CaptureSwitch;
    m_toggle->setObjectName("captureToggle");
    m_toggle->setAccessibleName("Toggle capture");
    statusLayout->addWidget(m_toggle);
    homeLayout->addWidget(card);

    auto *overlayCard = new QFrame;
    overlayCard->setObjectName("panel");
    auto *overlayRow = new QHBoxLayout(overlayCard);
    overlayRow->setContentsMargins(12, 10, 12, 10);
    auto *overlayMark = new QLabel("▱");
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
    homeLayout->addWidget(overlayCard);

    auto *modePanel = new QFrame;
    modePanel->setObjectName("segments");
    auto *modes = new QHBoxLayout(modePanel);
    modes->setContentsMargins(2, 2, 2, 2);
    modes->setSpacing(2);
    auto *group = new QButtonGroup(this);
    m_replay = new QPushButton("Instant Replay");
    m_record = new QPushButton("Recording");
    for (auto *mode : {m_replay, m_record}) {
        mode->setObjectName("segment");
        mode->setCheckable(true);
        group->addButton(mode);
        modes->addWidget(mode, 1);
    }
    homeLayout->addWidget(modePanel);

    auto *quick = new QGridLayout;
    quick->setColumnStretch(1, 1);
    m_systemAudio = new QPushButton("System audio");
    m_microphone = new QPushButton("Microphone");
    auto addAudioRow = [this, quick](int row, QPushButton *button, QSlider *&slider, QLabel *&percent) {
        button->setCheckable(true);
        quick->addWidget(button, row, 0);
        slider = new ClickToAdjust<QSlider>(Qt::Horizontal);
        slider->setRange(0, 100);
        slider->setAccessibleName(button->text() + " capture volume");
        slider->setToolTip("Recording volume");
        quick->addWidget(slider, row, 1);
        percent = new QLabel("100%");
        percent->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        percent->setMinimumWidth(percent->sizeHint().width());
        percent->setProperty("muted", true);
        quick->addWidget(percent, row, 2);
        connect(slider, &QSlider::valueChanged, this, [this, row](int value) { emit volumeChanged(row == 1, value); });
    };
    addAudioRow(0, m_systemAudio, m_systemVolume, m_systemPercent);
    addAudioRow(1, m_microphone, m_microphoneVolume, m_microphonePercent);
    m_audioSettings = new QPushButton("Audio settings");
    m_audioSettings->setObjectName("settingsLink");
    quick->addWidget(m_audioSettings, 2, 0, 1, 3);
    homeLayout->addLayout(quick);

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
    homeLayout->addLayout(summary);
    m_diskUsage = new QProgressBar;
    m_diskUsage->setAccessibleName("Output volume usage");
    m_diskUsage->setRange(0, 100);
    m_diskUsage->setTextVisible(false);
    m_diskUsage->setFixedHeight(4);
    m_diskUsage->hide();
    homeLayout->addWidget(m_diskUsage);

    auto *actions = new QHBoxLayout;
    m_action = new QPushButton;
    m_action->setObjectName("captureAction");
    actions->addWidget(m_action, 1);
    m_seconds = new ClickToAdjust<QSpinBox>;
    m_seconds->setRange(1, 30);
    m_seconds->setValue(10);
    m_seconds->setSuffix(" sec");
    m_seconds->setAccessibleName("Replay save duration");
    m_seconds->setToolTip("Duration saved by the dashboard button");
    actions->addWidget(m_seconds);
    homeLayout->addLayout(actions);
    m_replayRecording = new QPushButton("Start recording");
    m_replayRecording->setObjectName("replayRecording");
    m_replayRecording->setToolTip("Record continuously while instant replay stays on");
    m_replayRecording->hide();
    homeLayout->addWidget(m_replayRecording);

    auto *folderRow = new QHBoxLayout;
    m_folder = new QPushButton;
    m_folder->setObjectName("settingsLink");
    m_folder->setAccessibleName("Change save folder");
    folderRow->addWidget(m_folder, 1);
    m_openFolder = new QPushButton("Open folder");
    m_openFolder->setObjectName("clipsFolder");
    folderRow->addWidget(m_openFolder);
    homeLayout->addLayout(folderRow);
    homeLayout->addStretch();
    auto *libraryCard = new QFrame;
    libraryCard->setObjectName("panel");
    auto *libraryLayout = new QVBoxLayout(libraryCard);
    libraryLayout->setContentsMargins(14, 10, 12, 12);
    m_library = new Library;
    libraryLayout->addWidget(m_library, 1);
    m_homeLayout->addWidget(libraryCard, 1);

    m_settings = new SettingsPage(caps);
    m_editor = new OverlayEditor;
    m_pages->addWidget(m_home);
    m_pages->addWidget(m_settings);
    m_pages->addWidget(m_editor);
    shell->addWidget(m_pages, 1);
    m_notice = new QLabel;
    m_notice->setObjectName("notice");
    m_notice->setTextFormat(Qt::PlainText);
    m_notice->setWordWrap(true);
    m_notice->setMaximumHeight(46);
    m_notice->hide();
    shell->addWidget(m_notice);
    auto *divider = new QFrame;
    divider->setObjectName("divider");
    divider->setFixedHeight(1);
    shell->addWidget(divider);
    m_footer = new QLabel;
    m_footer->setProperty("muted", true);
    shell->addWidget(m_footer);

    connect(close, &QPushButton::clicked, this, &Dashboard::hideRequested);
    connect(m_back, &QPushButton::clicked, this, &Dashboard::escape);
    connect(m_toggle, &QPushButton::clicked, this, [this](bool checked) {
        m_toggle->setChecked(!checked);
        emit toggleCapture();
    });
    connect(m_action, &QPushButton::clicked, this, [this] {
        if (m_actionSavesReplay) emit saveReplay(m_seconds->value());
        else emit toggleCapture();
    });
    connect(m_seconds, &QSpinBox::valueChanged, this, [this](int seconds) {
        m_preferredSeconds = seconds;
        updateActionText();
    });
    connect(m_replayRecording, &QPushButton::clicked, this, &Dashboard::toggleReplayRecording);
    connect(m_replay, &QPushButton::clicked, this, [this] { emit recordingMode(false); });
    connect(m_record, &QPushButton::clicked, this, [this] { emit recordingMode(true); });
    connect(m_systemAudio, &QPushButton::clicked, this, [this](bool checked) { emit audioChanged(false, checked); });
    connect(m_microphone, &QPushButton::clicked, this, [this](bool checked) { emit audioChanged(true, checked); });
    auto openSettings = [this] {
        emit settingsRequested();
        showPage(m_settings, 510);
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
    connect(m_library, &Library::openingClip, this, &Dashboard::hideRequested);
    connect(m_library, &Library::storageChanged, this, [this](qint64 available, qint64 total) {
        m_disk->setText(total > 0 ? QLocale().formattedDataSize(available, 1) + " free" : QString());
        m_diskUsage->setVisible(total > 0);
        if (width() < 800) positionPanel();
        if (total > 0) m_diskUsage->setValue(qBound(0, qRound(100. * (1. - double(available) / total)), 100));
    });
    connect(m_library, &Library::openFailed, this, &Dashboard::notice);
    connect(m_openFolder, &QPushButton::clicked, this, [this] {
        if (QDesktopServices::openUrl(QUrl::fromLocalFile(m_directory))) emit hideRequested();
        else notice("Could not open the clips folder.");
    });
    auto *escape = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(escape, &QShortcut::activated, this, &Dashboard::escape);
    setAccent("mint");
    setShortcutKeys({"Alt+Z", "Ctrl+Shift+F10", "Alt+F10"});
    resize(1280, 720);
    home();
}

void Dashboard::updateState(const GsrOptions &o, const OverlayScene &scene, bool active, bool ready, bool saving, bool stopping,
                            bool replayRecording, bool busy) {
    const QString mode = o.continuousRecording ? "Recording" : "Instant Replay";
    m_status->setText(stopping ? "Finishing capture…" : active && !ready ? "Starting capture…" :
                      ready ? (o.continuousRecording ? mode : replayRecording ? "Recording + Instant Replay" : "Instant Replay is on")
                            : mode + " is off");
    const QString source = o.source == "portal" ? "Screen picker" : o.source;
    m_detail->setText(o.continuousRecording ? source + " · Continuous recording" :
                     QString("Last %1 sec · %2").arg(o.bufferSeconds).arg(source));
    m_detail->setToolTip(m_detail->text());
    static_cast<CaptureRing *>(m_ring)->setState(ready && !stopping, o.continuousRecording);
    m_toggle->setChecked(active);
    m_toggle->setEnabled(!stopping);
    m_replay->setChecked(!o.continuousRecording);
    m_record->setChecked(o.continuousRecording);
    m_replay->setEnabled(!active);
    m_record->setEnabled(!active);
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
    };
    updateAudio(m_systemAudio, m_systemVolume, m_systemPercent, o.systemAudio, o.levels.systemMuted, o.levels.systemVolume);
    updateAudio(m_microphone, m_microphoneVolume, m_microphonePercent, o.microphone, o.levels.microphoneMuted, o.levels.microphoneVolume);
    for (auto *widget : {static_cast<QWidget *>(m_systemAudio), static_cast<QWidget *>(m_systemVolume), static_cast<QWidget *>(m_systemPercent)})
        widget->setVisible(!o.applicationAudio);
    m_audioSettings->setText(o.applicationAudio ? "Application audio settings" : "Audio settings");
    m_replayRecording->setText(replayRecording ? "Stop recording" : "Start recording");
    m_replayRecording->setVisible(ready && !o.continuousRecording);
    m_replayRecording->setEnabled(!busy && !saving && !stopping);

    const QSignalBlocker blocker(m_seconds);
    m_seconds->setMaximum(o.bufferSeconds);
    m_seconds->setValue(m_preferredSeconds);
    m_seconds->setVisible(!o.continuousRecording);
    m_seconds->setEnabled(!saving && !stopping);
    m_actionSavesReplay = ready && !o.continuousRecording && !saving && !stopping;
    m_actionText = stopping ? "Finishing…" : active && !ready ? "Starting…" : saving ? "Saving…" :
        o.continuousRecording ? (active ? "Stop recording" : "Start recording") : "Start instant replay";
    updateActionText();
    m_action->setEnabled(!stopping && !saving && !busy && (!active || ready));

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

void Dashboard::updateActionText() {
    m_action->setText(m_actionSavesReplay ? QString("Save %1 sec").arg(m_seconds->value()) : m_actionText);
}

void Dashboard::notice(const QString &message) {
    m_notice->setText(message);
    m_notice->setToolTip(message);
    m_notice->setVisible(!message.isEmpty());
}

void Dashboard::setShortcutKeys(const QStringList &keys) {
    m_footer->setText(keys.value(0) + "  Show / hide    ·    " + keys.value(1) + "  Save replay");
    m_footer->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_action->setToolTip("Escape returns to the game. The global replay shortcut saves up to 10 sec.");
    m_toggle->setToolTip(keys.value(2) + "  Start / stop capture");
    m_seconds->setToolTip("Duration saved by this button. " + keys.value(1) + " saves up to 10 sec.");
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
    m_back->setVisible(!dashboard);
    m_settingsButton->setVisible(dashboard);
    m_layers->setVisible(dashboard);
    m_panelWidth = width;
    m_panelHeight = dashboard ? 680 : 780;
    m_pages->setCurrentWidget(page);
    positionPanel();
}

void Dashboard::home() { showPage(m_home, 1034); }

void Dashboard::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    positionPanel();
}

void Dashboard::positionPanel() {
    const auto panelWidth = qBound(0, width() - 24, m_panelWidth);
    const auto panelHeight = qBound(0, height() - 24, m_panelHeight);
    const bool narrow = width() < 800;
    m_homeLayout->setDirection(narrow ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
    const int controlsHeight = qBound(0, panelHeight - 245, m_controlScroll->widget()->sizeHint().height());
    m_controlScroll->setMinimumHeight(narrow ? controlsHeight : 0);
    m_controlScroll->setMaximumHeight(narrow ? controlsHeight : QWIDGETSIZE_MAX);
    m_panel->setGeometry((width() - panelWidth) / 2, (height() - panelHeight) / 2, panelWidth, panelHeight);
}

void Dashboard::escape() {
    if (m_pages->currentWidget() == m_home) emit hideRequested();
    else {
        if (m_pages->currentWidget() == m_editor) m_editor->flush();
        home();
    }
}

void Dashboard::closeEvent(QCloseEvent *event) {
    event->ignore();
    emit hideRequested();
}

void Dashboard::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(11, 13, 15, 154));
}
} // namespace Shadowless
