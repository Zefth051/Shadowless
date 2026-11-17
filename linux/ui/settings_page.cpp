#include "settings_page.h"
#include "value_controls.h"
#include "theme.h"
#include "input_overlay_preview.h"
#include "../overlays/input/core/layout.h"
#include "../platform/hyprland/bindings.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStackedWidget>
#include <QScrollBar>
#include <QStyle>
#include <QVBoxLayout>
#include <QKeySequenceEdit>
#include <QSet>
#include <QTableWidget>
#include <QHeaderView>
#include <QIcon>
#include <QRegularExpressionValidator>
#include <QtConcurrentRun>
#include <QPainter>
#include <QDir>
#include <QFile>
#include <unistd.h>

namespace Shadowless {
namespace {
class SettingsSwitch : public QCheckBox {
public:
    explicit SettingsSwitch(const QString &text) : QCheckBox(text) {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setMinimumHeight(40);
        setCursor(Qt::PointingHandCursor);
        setToolTip(text);
    }
    QSize sizeHint() const override { return {fontMetrics().horizontalAdvance(text()) + 64, 40}; }
    QSize minimumSizeHint() const override { return {64, 40}; }
protected:
    bool hitButton(const QPoint &point) const override { return rect().contains(point); }
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        if (!isEnabled()) painter.setOpacity(.5);
        const auto accent = window()->palette().color(QPalette::Accent);
        const QRectF track(width() - 40, (height() - 22) / 2., 38, 22);
        painter.setPen(hasFocus() ? QPen(accent, 1.5) : QPen(Qt::NoPen));
        painter.setBrush(isChecked() ? accent : QColor("#363c42"));
        painter.drawRoundedRect(track, 11, 11);
        painter.setPen(Qt::NoPen);
        const auto thumb = isChecked() ? window()->palette().color(QPalette::HighlightedText) : QColor("#f1f5f4");
        painter.setBrush(isDown() ? thumb.darker(120) : thumb);
        painter.drawEllipse(QPointF(track.left() + (isChecked() ? 27 : 11), track.center().y()), 8, 8);
        painter.setPen(palette().color(QPalette::WindowText));
        painter.drawText(QRect(0, 0, width() - 56, height()), Qt::AlignLeft | Qt::AlignVCenter,
                         fontMetrics().elidedText(text(), Qt::ElideRight, width() - 56));
    }
};
}
SettingsPage::SettingsPage(const GsrCapabilities &caps, QWidget *parent) : QWidget(parent), m_caps(caps) {
    setObjectName("settingsPage");
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *heading = new QLabel("Settings");
    heading->setObjectName("heading");
    layout->addWidget(heading);
    auto *pages = new QStackedWidget;
    auto *settings = new QWidget;
    auto *settingsLayout = new QVBoxLayout(settings);
    settingsLayout->setContentsMargins(0, 0, 0, 0);
    pages->addWidget(settings);
    layout->addWidget(pages, 1);

    auto *scroll = new QScrollArea;
    m_scroll = scroll;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *body = new QWidget;
    auto *bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(0, 8, 10, 12);
    bodyLayout->setSpacing(12);

    auto section = [](QVBoxLayout *parent, const QString &name) {
        auto *label = new QLabel(name);
        label->setObjectName("sectionTitle");
        parent->addWidget(label);
        auto *card = new QFrame;
        card->setObjectName("settingsSection");
        auto *form = new QFormLayout(card);
        form->setContentsMargins(14, 8, 14, 8);
        form->setHorizontalSpacing(16);
        form->setVerticalSpacing(8);
        form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        form->setRowWrapPolicy(QFormLayout::WrapLongRows);
        parent->addWidget(card);
        return form;
    };
    auto note = [](const QString &text) {
        auto *label = new QLabel(text);
        label->setWordWrap(true);
        label->setProperty("muted", true);
        return label;
    };

    m_activeNotice = note({});
    m_activeNotice->setObjectName("captureActiveNotice");
    settingsLayout->addWidget(m_activeNotice);
    m_captureFields = new QWidget;
    m_captureFields->setObjectName("captureSettings");
    auto *capture = new QVBoxLayout(m_captureFields);
    capture->setContentsMargins(0, 0, 0, 0);
    capture->setSpacing(9);
    auto *replay = section(capture, "INSTANT REPLAY");
    m_buffer = new ClickToAdjust<QSpinBox>;
    m_buffer->setRange(1, 3600);
    m_buffer->setSuffix(" sec");
    m_buffer->setAccessibleName("Clip duration");
    replay->addRow("Clip duration", m_buffer);

    auto *video = section(capture, "VIDEO");
    m_source = new ClickToAdjust<QComboBox>;
    m_source->addItems(caps.sources);
    video->addRow("Capture source", m_source);
    m_codec = new ClickToAdjust<QComboBox>;
    m_codec->addItems(caps.codecs);
    video->addRow("Video codec", m_codec);
    auto addPresets = [video](const QString &label, const QString &name,
                              const QList<int> &presets, int minimum, int maximum, const QString &unit,
                              QComboBox **selector, QSpinBox **manual) {
        auto *field = new QWidget;
        auto *fields = new QVBoxLayout(field);
        fields->setContentsMargins(0, 0, 0, 0);
        fields->setSpacing(6);

        auto *combo = new ClickToAdjust<QComboBox>;
        combo->setObjectName(name);
        combo->setAccessibleName(label);
        for (const int value : presets)
            combo->addItem(value == 0 ? QString("Automatic") : QString::number(value) + unit, value);
        combo->addItem("Custom", -1);
        fields->addWidget(combo);

        auto *custom = new ClickToAdjust<QSpinBox>;
        custom->setRange(minimum, maximum);
        custom->setSuffix(unit);
        custom->setAccessibleName("Custom " + label.toLower());
        custom->setObjectName(name + "Custom");
        custom->hide();
        fields->addWidget(custom);
        connect(combo, &QComboBox::currentIndexChanged, custom, [combo, custom] {
            custom->setVisible(combo->currentData().toInt() == -1);
        });

        video->addRow(label, field);
        *selector = combo;
        *manual = custom;
    };
    addPresets("Bitrate", "captureBitrate", {0, 10, 20, 35, 50, 80}, 1, 100, " Mbps",
               &m_bitrate, &m_customBitrate);
    addPresets("Frame rate", "captureFps", {30, 60, 120, 144, 240}, 1, 240, " FPS",
               &m_fps, &m_customFps);
    video->addRow(note("Frame rates above 60 FPS are experimental. Recordings exclude the mouse cursor."));

    auto *audio = section(capture, "AUDIO");
    m_audioMode = new ClickToAdjust<QComboBox>;
    m_audioMode->setObjectName("audioRoutingMode");
    m_audioMode->addItems({"Automatic application tracks", "Selected applications", "Four tracks: PC / Game / Music / Microphone"});
    audio->addRow("Audio routing", m_audioMode);
    audio->addRow(note("Automatic tracks discovers applications throughout capture, across all output devices. PC groups all audio except applications marked Game. Use Track to choose default inclusion and More tracks in the editor to select applications individually. Shared browser audio identities cannot be separated per tab."));
    audio->addRow(note("For four independent tracks, choose Four tracks above, then assign the game and music player below. Desktop audio records one mix, plus the microphone when separated. Recording adds no virtual input devices."));
    m_system = new SettingsSwitch("Record system audio");
    audio->addRow(m_system);
    m_audioOutput = new ClickToAdjust<QComboBox>;
    m_audioOutput->setObjectName("audioOutput");
    m_audioOutput->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_audioOutput->setMinimumContentsLength(16);
    audio->addRow("Output device", m_audioOutput);
    m_microphone = new SettingsSwitch("Record microphone");
    audio->addRow(m_microphone);
    m_audioInput = new ClickToAdjust<QComboBox>;
    m_audioInput->setObjectName("audioInput");
    m_audioInput->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_audioInput->setMinimumContentsLength(16);
    audio->addRow("Input device", m_audioInput);
    m_microphoneBoost = new ClickToAdjust<QSpinBox>;
    m_microphoneBoost->setRange(0, 20);
    m_microphoneBoost->setSuffix(" dB");
    m_microphoneBoost->setAccessibleName("Microphone boost");
    audio->addRow("Microphone boost", m_microphoneBoost);
    m_microphoneTrack = new ClickToAdjust<QComboBox>;
    m_microphoneTrack->setObjectName("microphoneAudioTrack");
    for (int i = 1; i <= 6; ++i) m_microphoneTrack->addItem(QString("Track %1").arg(i));
    audio->addRow("Microphone track", m_microphoneTrack);
    m_separate = new SettingsSwitch("Separate system and microphone tracks");
    audio->addRow(m_separate);
    m_pcTrack = new ClickToAdjust<QComboBox>;
    m_pcTrack->setObjectName("pcAudioTrack");
    m_pcTrack->addItem("Off");
    for (int i = 1; i <= 6; ++i) m_pcTrack->addItem(QString("Track %1").arg(i));
    audio->addRow("PC / other applications track", m_pcTrack);
    auto *applicationHelp = note("Four tracks fixes PC to 1, Game to 2, Music to 3 and Microphone to 4. Assign your game to Game and your music player to Music. Start audio in each application and refresh to discover it; saved assignments also work when an app is quiet or not running. PC captures other applications and excludes every assigned app. Browser tabs may share one audio identity.");
    audio->addRow(applicationHelp);
    m_applications = new QTableWidget(0, 2);
    m_applications->setHorizontalHeaderLabels({"Application", "Track"});
    m_applications->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_applications->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_applications->verticalHeader()->hide();
    m_applications->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_applications->setSelectionMode(QAbstractItemView::NoSelection);
    m_applications->setFixedHeight(150);
    audio->addRow(m_applications);
    m_refreshAudio = new QPushButton("Refresh devices and apps");
    audio->addRow(m_refreshAudio);
    m_audioStatus = note({});
    m_audioStatus->setTextFormat(Qt::PlainText);
    m_audioStatus->hide();
    audio->addRow(m_audioStatus);
    m_audioCodec = new ClickToAdjust<QComboBox>;
    m_audioCodec->addItem("AAC", "aac");
    m_audioCodec->addItem("Opus", "opus");
    audio->addRow("Audio codec", m_audioCodec);
    m_audioBitrate = new ClickToAdjust<QComboBox>;
    m_audioBitrate->addItem("Automatic", 0);
    for (const int value : {64, 96, 128, 160, 192, 256, 320})
        m_audioBitrate->addItem(QString("%1 kbps").arg(value), value);
    audio->addRow("Audio bitrate", m_audioBitrate);
    connect(m_refreshAudio, &QPushButton::clicked, this, &SettingsPage::refreshAudio);
    connect(&m_audioProbe, &QFutureWatcher<AudioCatalog>::finished, this, [this] {
        m_audioCatalog = m_audioProbe.result();
        populateAudio(m_audioOutput->currentData().toString(), m_audioInput->currentData().toString(), applicationTracks());
        m_refreshAudio->setEnabled(true);
        m_audioStatus->setText(m_audioCatalog.error);
        m_audioStatus->setVisible(!m_audioCatalog.error.isEmpty());
    });
    auto updateTracks = [this, audio, applicationHelp] {
        const bool apps = m_audioMode->currentIndex() != 0;
        const bool four = m_audioMode->currentIndex() == 2;
        audio->setRowVisible(m_system, !apps);
        audio->setRowVisible(m_audioOutput, false);
        audio->setRowVisible(m_separate, false);
        audio->setRowVisible(applicationHelp, apps);
        audio->setRowVisible(m_applications, apps);
        audio->setRowVisible(m_microphoneTrack, apps && !four);
        audio->setRowVisible(m_pcTrack, apps && !four);
        m_microphone->setEnabled(!four);
        m_audioOutput->setEnabled(m_system->isChecked());
        m_audioInput->setEnabled(m_microphone->isChecked());
        m_microphoneBoost->setEnabled(m_microphone->isChecked());
        m_microphoneTrack->setEnabled(m_microphone->isChecked());
        m_separate->setEnabled(m_system->isChecked() && m_microphone->isChecked());
    };
    connect(m_audioMode, &QComboBox::currentIndexChanged, this, updateTracks);
    connect(m_audioMode, &QComboBox::currentIndexChanged, this, [this] {
        const auto tracks = applicationTracks();
        if (m_audioMode->currentIndex() == 2) {
            m_pcTrack->setCurrentIndex(1);
            m_microphoneTrack->setCurrentIndex(3);
            m_microphone->setChecked(true);
        }
        populateAudio(m_audioOutput->currentData().toString(), m_audioInput->currentData().toString(), tracks);
    });
    connect(m_system, &QCheckBox::toggled, this, updateTracks);
    connect(m_microphone, &QCheckBox::toggled, this, updateTracks);
    updateTracks();
    populateAudio({}, {}, {});

    auto *storage = section(capture, "STORAGE");
    m_organizeByGame = new SettingsSwitch("Organize clips into folders by game");
    m_organizeByGame->setObjectName("organizeByGame");
    storage->addRow(m_organizeByGame);
    storage->addRow(note("Only new clips are organized. Unidentified games stay in the main save folder."));
    m_container = new ClickToAdjust<QComboBox>;
    m_container->addItems({"mp4", "mkv"});
    storage->addRow("File format", m_container);
    auto *folderRow = new QHBoxLayout;
    m_folder = new QLineEdit;
    m_folder->setMinimumWidth(100);
    m_folder->setAccessibleName("Save folder");
    folderRow->addWidget(m_folder, 1);
    auto *browse = new QPushButton("Browse…");
    folderRow->addWidget(browse);
    storage->addRow("Save folder", folderRow);
    bodyLayout->addWidget(m_captureFields);

    auto *picker = new QFileDialog(this, "Choose the clips folder");
    picker->setOption(QFileDialog::DontUseNativeDialog);
    picker->setWindowFlags(Qt::Widget);
    picker->setFileMode(QFileDialog::Directory);
    pages->addWidget(picker);
    connect(browse, &QPushButton::clicked, this, [this, pages, picker] {
        picker->setDirectory(m_folder->text());
        pages->setCurrentWidget(picker);
    });
    connect(picker, &QFileDialog::accepted, this, [this, pages, picker, settings] {
        if (m_captureFields->isEnabled()) m_folder->setText(picker->selectedFiles().value(0));
        pages->setCurrentWidget(settings);
    });
    connect(picker, &QFileDialog::rejected, this, [pages, settings] { pages->setCurrentWidget(settings); });
    m_pages = pages;

    auto *startup = section(bodyLayout, "STARTUP");
    m_launchAtLogin = new SettingsSwitch("Launch Shadowless at login");
    m_launchAtLogin->setObjectName("launchAtLogin");
    startup->addRow(m_launchAtLogin);
    m_startReplayOnLaunch = new SettingsSwitch("Start Instant Clip when Shadowless launches");
    m_startReplayOnLaunch->setObjectName("startReplayOnLaunch");
    startup->addRow(m_startReplayOnLaunch);
    m_discordPresence = new SettingsSwitch("Show Discord activity while clipping a game");
    m_discordPresence->setObjectName("discordPresence");
    startup->addRow(m_discordPresence);
    m_idleCompression = new SettingsSwitch("Compress older clips to HEVC while no game is open");
    m_idleCompression->setObjectName("idleCompression");
    startup->addRow(m_idleCompression);
    startup->addRow(note("Compression is lossy and off by default. Clips older than 10 minutes are re-encoded at low priority; each original is replaced only after the new file is validated and smaller."));
    startup->addRow(note("Replay starts on any launch, including manual launches. Both options are independent."));

    auto *appearance = section(bodyLayout, "APPEARANCE");
    m_accent = new ClickToAdjust<QComboBox>;
    m_accent->setObjectName("accentColor");
    for (const auto &name : accentNames()) {
        auto label = name;
        label[0] = label[0].toUpper();
        m_accent->addItem(accentIcon(name), label, name);
    }
    m_accent->addItem("Custom HEX", "custom");
    appearance->addRow("Accent color", m_accent);
    m_customAccent = new QLineEdit;
    m_customAccent->setObjectName("customAccent");
    m_customAccent->setAccessibleName("Custom accent HEX color");
    m_customAccent->setPlaceholderText("#63e0bd");
    m_customAccent->setMaxLength(7);
    m_customAccent->setValidator(new QRegularExpressionValidator(QRegularExpression("#[0-9a-fA-F]{6}"), m_customAccent));
    m_customAccent->setEnabled(false);
    m_customAccent->setToolTip("Enter a six-digit color, for example #63e0bd");
    appearance->addRow("Custom HEX", m_customAccent);
    connect(m_accent, &QComboBox::currentIndexChanged, this, [this] {
        m_customAccent->setEnabled(m_accent->currentData().toString() == "custom");
    });

    auto *overlay = section(bodyLayout, "OVERLAY");
    m_indicator = new SettingsSwitch("Show capture status while playing");
    overlay->addRow(m_indicator);
    m_indicatorPosition = new ClickToAdjust<QComboBox>;
    m_indicatorPosition->setObjectName("indicatorPosition");
    m_indicatorPosition->addItem("Left up", "top-left");
    m_indicatorPosition->addItem("Right up", "top-right");
    m_indicatorPosition->addItem("Left down", "bottom-left");
    m_indicatorPosition->addItem("Right down", "bottom-right");
    m_indicatorPosition->setCurrentIndex(1);
    overlay->addRow("Indicator position", m_indicatorPosition);
    overlay->addRow(note("The indicator is experimental and lets mouse and keyboard input pass through."));

    auto *peripheral = section(bodyLayout, "PERIPHERAL OVERLAY");
    m_inputPreview = new InputOverlayPreview;
    peripheral->addRow(m_inputPreview);
    m_inputLayout = new ClickToAdjust<QComboBox>;
    m_inputLayout->setObjectName("inputOverlayLayout");
    m_inputLayout->setAccessibleName("Peripheral overlay");
    m_inputLayout->addItem("None", "none");
    for (const QString &id : Input::Layout::builtinIds()) {
        Input::Layout layout; QString error;
        m_inputLayout->addItem(Input::Layout::builtin(id, &layout, &error) ? layout.name() : id, id);
    }
    peripheral->addRow("Peripheral overlay", m_inputLayout);
    m_inputCorner = new ClickToAdjust<QComboBox>;
    m_inputCorner->setObjectName("inputOverlayCorner");
    m_inputCorner->addItem("Left down", "bottom-left");
    m_inputCorner->addItem("Right down", "bottom-right");
    m_inputCorner->addItem("Left up", "top-left");
    m_inputCorner->addItem("Right up", "top-right");
    peripheral->addRow("Position", m_inputCorner);
    m_inputScale = new ClickToAdjust<QSpinBox>;
    m_inputScale->setObjectName("inputOverlayScale");
    m_inputScale->setRange(50, 200); m_inputScale->setSingleStep(10); m_inputScale->setSuffix(" %"); m_inputScale->setValue(100);
    peripheral->addRow("Size", m_inputScale);
    m_inputOpacity = new ClickToAdjust<QSpinBox>;
    m_inputOpacity->setObjectName("inputOverlayOpacity");
    m_inputOpacity->setRange(20, 100); m_inputOpacity->setSingleStep(10); m_inputOpacity->setSuffix(" %"); m_inputOpacity->setValue(100);
    peripheral->addRow("Opacity", m_inputOpacity);
    m_inputSidecar = new SettingsSwitch("Save input events next to each clip for the editor overlay");
    m_inputSidecar->setObjectName("inputOverlaySidecar");
    peripheral->addRow(m_inputSidecar);
    m_inputNotice = note({});
    m_inputNotice->setObjectName("inputOverlayNotice");
    peripheral->addRow(m_inputNotice);
    m_inputAccess = note({});
    m_inputAccess->setObjectName("inputOverlayAccess");
    peripheral->addRow(m_inputAccess);
    m_inputSetup = note(
        "Shadowless never changes permissions and does not need the \"input\" group. To allow reading only the device classes you need, "
        "create /etc/udev/rules.d/70-shadowless-input.rules with one line per class, for example:\n"
        "SUBSYSTEM==\"input\", KERNEL==\"event*\", ENV{ID_INPUT_MOUSE}==\"1\", TAG+=\"uaccess\"\n"
        "(ID_INPUT_KEYBOARD and ID_INPUT_JOYSTICK work the same way), then run: sudo udevadm control --reload && sudo udevadm trigger -s input. "
        "Details: docs/LINUX_INPUT_OVERLAY.md.");
    m_inputSetup->setObjectName("inputOverlaySetup");
    m_inputSetup->setTextFormat(Qt::PlainText);
    m_inputSetup->setTextInteractionFlags(Qt::TextSelectableByMouse);
    peripheral->addRow(m_inputSetup);
    connect(m_inputLayout, &QComboBox::currentIndexChanged, this, [this] { updateInputOverlay(); });
    connect(m_inputOpacity, &QSpinBox::valueChanged, this, [this] { updateInputOverlay(); });
    updateInputOverlay();

    auto *shortcuts = section(bodyLayout, "GLOBAL SHORTCUTS");
    for (const auto &label : {"Show / hide overlay", "Save replay", "Start / stop capture", "Pause / resume recording"}) {
        auto *edit = new QKeySequenceEdit;
        edit->setMaximumSequenceLength(1);
        edit->setAccessibleName(label);
        if (auto *inner = edit->findChild<QLineEdit *>()) inner->setAccessibleName(label);
        m_keys.append(edit);
        shortcuts->addRow(label, edit);
    }
    m_shortcuts = note("Registering desktop shortcuts…");
    m_shortcuts->setTextFormat(Qt::PlainText);
    m_shortcuts->setTextInteractionFlags(Qt::TextSelectableByMouse);
    shortcuts->addRow(m_shortcuts);
    bodyLayout->addStretch();
    for (auto *form : body->findChildren<QFormLayout *>()) {
        for (int row = 0; row < form->rowCount(); ++row) {
            const auto *item = form->itemAt(row, QFormLayout::LabelRole);
            if (item && item->widget()) {
                item->widget()->setFixedWidth(140);
                if (auto *label = qobject_cast<QLabel *>(item->widget())) label->setWordWrap(true);
            }
        }
    }
    for (auto *box : body->findChildren<QComboBox *>()) {
        box->setMinimumWidth(0);
        box->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    }

    scroll->setWidget(body);
    auto *sections = new QHBoxLayout;
    auto addSectionLink = [this, sections](const QString &text, QWidget *field) {
        auto *link = new QPushButton(text);
        link->setObjectName("sectionLink");
        link->setProperty("active", m_sectionLinks.isEmpty());
        connect(link, &QPushButton::clicked, this, [this, field] { focusSection(field); });
        sections->addWidget(link);
        m_sectionLinks.append({link, field});
    };
    addSectionLink("Replay", m_buffer);
    addSectionLink("Video", m_source);
    addSectionLink("Audio", m_audioMode);
    addSectionLink("Storage", m_container);
    addSectionLink("General", m_launchAtLogin);
    settingsLayout->addLayout(sections);
    connect(scroll->verticalScrollBar(), &QScrollBar::valueChanged, this, [this] { updateActiveSection(); });
    settingsLayout->addWidget(scroll, 1);
    m_error = new QLabel;
    m_error->setObjectName("settingsError");
    m_error->setWordWrap(true);
    m_error->setTextFormat(Qt::PlainText);
    settingsLayout->addWidget(m_error);
    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    auto *cancel = new QPushButton("Cancel");
    cancel->setObjectName("cancelSettings");
    buttons->addWidget(cancel);
    auto *save = new QPushButton("Save settings");
    save->setObjectName("primary");
    buttons->addWidget(save);
    settingsLayout->addLayout(buttons);
    connect(cancel, &QPushButton::clicked, this, &SettingsPage::cancelled);
    connect(save, &QPushButton::clicked, this, [this] {
        auto options = m_original;
        if (m_captureFields->isEnabled()) {
            options.source = m_source->currentText(); options.codec = m_codec->currentText(); options.frameRate = m_fps->currentData().toInt();
            if (options.frameRate == -1) options.frameRate = m_customFps->value();
            options.container = m_container->currentText(); options.bufferSeconds = m_buffer->value(); options.bitrateMbps = m_bitrate->currentData().toInt();
            if (options.bitrateMbps == -1) options.bitrateMbps = m_customBitrate->value();
            options.outputDirectory = m_folder->text(); options.systemAudio = m_system->isChecked(); options.microphone = m_microphone->isChecked(); options.separateAudioTracks = m_separate->isChecked();
            options.organizeByGame = m_organizeByGame->isChecked();
            options.audioOutput = m_audioOutput->currentData().toString();
            options.audioInput = m_audioInput->currentData().toString();
            options.applicationAudio = m_audioMode->currentIndex() != 0;
            if (!options.applicationAudio) options.dynamicApplicationAudio = true;
            options.fourTrackAudio = m_audioMode->currentIndex() == 2;
            options.applicationTracks = applicationTracks();
            options.levels.microphoneBoost = m_microphoneBoost->value();
            options.microphoneTrack = m_microphoneTrack->currentIndex() + 1;
            options.pcTrack = m_pcTrack->currentIndex();
            options.audioCodec = m_audioCodec->currentData().toString();
            options.audioBitrateKbps = m_audioBitrate->currentData().toInt();
        }
        const auto error = options.validate(m_caps); if (!error.isEmpty()) { m_error->setText(error); return; }
        QStringList keys; QSet<QString> distinct;
        for (auto *edit : m_keys) {
            const auto key = edit->keySequence().toString(QKeySequence::PortableText); HyprBinding binding;
            if (!HyprBinding::parse(key, {}, &binding) || distinct.contains(key)) { m_error->setText("Choose four different shortcuts using letters, digits, function keys, Space, Pause or Print."); return; }
            distinct.insert(key); keys.append(key);
        }
        auto accent = m_accent->currentData().toString();
        if (accent == "custom") {
            if (!m_customAccent->hasAcceptableInput()) {
                m_error->setText("Enter an accent color as #RRGGBB, for example #63e0bd."); return;
            }
            accent = m_customAccent->text().toLower();
        }
        emit saved(options, m_indicator->isChecked(), keys, accent, m_indicatorPosition->currentData().toString());
    });
}
bool SettingsPage::launchAtLogin() const { return m_launchAtLogin->isChecked(); }
bool SettingsPage::discordPresence() const { return m_discordPresence->isChecked(); }
void SettingsPage::setDiscordPresence(bool enabled) { m_discordPresence->setChecked(enabled); }
bool SettingsPage::idleCompression() const { return m_idleCompression->isChecked(); }
void SettingsPage::setIdleCompression(bool enabled) { m_idleCompression->setChecked(enabled); }
bool SettingsPage::startReplayOnLaunch() const { return m_startReplayOnLaunch->isChecked(); }
void SettingsPage::showError(const QString &error) { m_error->setText(error); }
void SettingsPage::edit(const GsrOptions &o, bool indicator, bool active, const QStringList &keys, const QString &accent, const QString &position, bool launch, bool replay) {
    m_launchAtLogin->setChecked(launch);
    m_startReplayOnLaunch->setChecked(replay);
    m_indicatorPosition->setCurrentIndex(qMax(0, m_indicatorPosition->findData(position)));
    const bool custom = QRegularExpression("^#[0-9a-fA-F]{6}$").match(accent).hasMatch();
    m_customAccent->setText(custom ? accent.toLower() : QString());
    m_accent->setCurrentIndex(qMax(0, m_accent->findData(custom ? "custom" : accent)));
    m_pages->setCurrentIndex(0);
    m_scroll->verticalScrollBar()->setValue(0);
    m_original = o; m_source->setCurrentText(o.source); m_codec->setCurrentText(o.codec); m_customFps->setValue(o.frameRate);
    const int fpsIndex = m_fps->findData(o.frameRate);
    m_fps->setCurrentIndex(fpsIndex >= 0 ? fpsIndex : m_fps->count() - 1);
    m_container->setCurrentText(o.container);
    m_buffer->setValue(o.bufferSeconds);
    m_customBitrate->setValue(o.bitrateMbps == 0 ? 35 : o.bitrateMbps);
    const int bitrateIndex = m_bitrate->findData(o.bitrateMbps);
    m_bitrate->setCurrentIndex(bitrateIndex >= 0 ? bitrateIndex : m_bitrate->count() - 1);
    m_folder->setText(o.outputDirectory);
    m_organizeByGame->setChecked(o.organizeByGame);
    m_system->setChecked(o.systemAudio); m_microphone->setChecked(o.microphone); m_separate->setChecked(o.separateAudioTracks); m_indicator->setChecked(indicator);
    m_audioMode->setCurrentIndex(o.fourTrackAudio ? 2 : o.applicationAudio ? 1 : 0);
    m_pcTrack->setCurrentIndex(o.pcTrack);
    m_microphoneBoost->setValue(o.levels.microphoneBoost);
    m_microphoneTrack->setCurrentIndex(o.microphoneTrack - 1);
    m_audioCodec->setCurrentIndex(m_audioCodec->findData(o.audioCodec));
    if (m_audioBitrate->findData(o.audioBitrateKbps) < 0)
        m_audioBitrate->addItem(QString("%1 kbps").arg(o.audioBitrateKbps), o.audioBitrateKbps);
    m_audioBitrate->setCurrentIndex(m_audioBitrate->findData(o.audioBitrateKbps));
    populateAudio(o.audioOutput, o.audioInput, o.applicationTracks);
    refreshAudio();
    setCaptureActive(active); m_error->clear();
    updateInputOverlay();
    for (int i = 0; i < m_keys.size(); ++i) m_keys[i]->setKeySequence(QKeySequence(keys.value(i)));
}
InputOverlaySettings SettingsPage::inputOverlay() const {
    InputOverlaySettings v;
    v.layout = m_inputLayout->currentData().toString();
    v.sidecar = m_inputSidecar->isChecked();
    v.corner = m_inputCorner->currentData().toString();
    v.scale = m_inputScale->value(); v.opacity = m_inputOpacity->value();
    return v.sanitized();
}
void SettingsPage::setInputOverlay(const InputOverlaySettings &value) {
    const auto v = value.sanitized();
    m_inputLayout->setCurrentIndex(qMax(0, m_inputLayout->findData(v.layout)));
    m_inputCorner->setCurrentIndex(qMax(0, m_inputCorner->findData(v.corner)));
    m_inputScale->setValue(v.scale); m_inputOpacity->setValue(v.opacity);
    m_inputSidecar->setChecked(v.sidecar);
    updateInputOverlay();
}
// Reflects the selected layout: preview, which controls apply, privacy wording and device access.
void SettingsPage::updateInputOverlay() {
    const QString id = m_inputLayout->currentData().toString();
    const bool enabled = id != Input::Layout::none();
    m_inputPreview->setLayoutId(id);
    m_inputPreview->setOpacity(m_inputOpacity->value());
    for (QWidget *w : QList<QWidget *>{m_inputCorner, m_inputScale, m_inputOpacity, m_inputSidecar}) w->setEnabled(enabled);
    if (!enabled) m_inputSidecar->setChecked(false);
    Input::Layout layout; QString error;
    const bool typing = enabled && Input::Layout::builtin(id, &layout, &error) && layout.recordsTypingKeys();
    m_inputNotice->setText(!enabled ? "Off. No keyboard, mouse or gamepad input is read until you choose an overlay."
        : typing ? "This layout includes every letter key. The timing of key presses could be used to reconstruct typed text, "
                   "so enable the saved events only if you accept that. Prefer a gameplay layout."
                 : "Only the keys and buttons shown above are read. Nothing is logged or stored unless saved events are on, "
                   "and then only those keys with their timing, never text.");
    // Access check: read permission on any input event device (no device is opened).
    bool readable = false, present = false;
    for (const QString &name : QDir("/dev/input").entryList({"event*"}, QDir::System | QDir::Files | QDir::NoDotAndDotDot)) {
        present = true;
        if (::access(QFile::encodeName("/dev/input/" + name).constData(), R_OK) == 0) { readable = true; break; }
    }
    m_inputAccess->setVisible(enabled && !readable);
    m_inputSetup->setVisible(enabled && !readable);
    m_inputAccess->setText(present ? "Shadowless cannot read your input devices yet, so the overlay will stay empty. Setup steps are below."
                                   : "No input devices were found.");
}
void SettingsPage::showVideo() {
    focusSection(m_source);
}

void SettingsPage::showStorage() {
    focusSection(m_container);
}

void SettingsPage::showAudio() {
    focusSection(m_audioMode);
}

void SettingsPage::focusSection(QWidget *field) {
    QTimer::singleShot(0, this, [this, field] {
        if (!field->isVisible()) return;
        if (field->isEnabled()) field->setFocus();
        m_scroll->verticalScrollBar()->setValue(field->mapTo(m_scroll->widget(), QPoint()).y() - 38);
        updateActiveSection();
    });
}

void SettingsPage::updateActiveSection() {
    if (m_sectionLinks.isEmpty() || !m_scroll) return;
    auto *bar = m_scroll->verticalScrollBar();
    QPushButton *active = m_sectionLinks.first().first;
    for (const auto &entry : m_sectionLinks) {
        if (!entry.second->isVisible()) continue;
        if (entry.second->mapTo(m_scroll->widget(), QPoint()).y() <= bar->value() + 60) active = entry.first;
    }
    if (bar->maximum() > 0 && bar->value() >= bar->maximum()) active = m_sectionLinks.last().first;
    for (const auto &entry : m_sectionLinks) {
        const bool on = entry.first == active;
        if (entry.first->property("active").toBool() == on) continue;
        entry.first->setProperty("active", on);
        entry.first->style()->unpolish(entry.first); entry.first->style()->polish(entry.first);
    }
}

void SettingsPage::refreshAudio() {
    if (m_audioProbe.isRunning()) return;
    m_refreshAudio->setEnabled(false);
    m_audioStatus->setText("Looking for audio devices and applications…");
    m_audioStatus->show();
    m_audioProbe.setFuture(QtConcurrent::run([] { return AudioCatalog::probe(); }));
}

void SettingsPage::populateAudio(const QString &output, const QString &input, const QMap<QString, int> &tracks) {
    const auto populate = [](QComboBox *box, const QList<AudioDevice> &devices, const QString &selected, const QString &fallback) {
        box->clear();
        box->addItem(fallback, QString());
        for (const auto &device : devices) box->addItem(device.description, device.id);
        if (!selected.isEmpty() && box->findData(selected) < 0) box->addItem(selected + " (unavailable)", selected);
        box->setCurrentIndex(qMax(0, box->findData(selected)));
    };
    populate(m_audioOutput, m_audioCatalog.outputs, output, "Default output");
    populate(m_audioInput, m_audioCatalog.inputs, input, "Default input");

    auto apps = m_audioCatalog.applications;
    for (const auto &name : tracks.keys())
        if (!apps.contains(name, Qt::CaseInsensitive)) apps.append(name);
    apps.sort(Qt::CaseInsensitive);
    m_applications->setRowCount(0);
    for (const auto &name : apps) {
        int track = 0;
        for (auto it = tracks.cbegin(); it != tracks.cend(); ++it)
            if (it.key().compare(name, Qt::CaseInsensitive) == 0) track = it.value();
        const auto row = m_applications->rowCount();
        m_applications->insertRow(row);
        QString iconName = name.trimmed().toLower();
        iconName.replace(' ', '-');
        QIcon icon = QIcon::fromTheme(m_audioCatalog.applicationIcons.value(name));
        if (icon.isNull()) icon = QIcon::fromTheme(iconName);
        if (icon.isNull()) icon = QIcon::fromTheme("application-x-executable");
        if (icon.isNull()) icon = QIcon(":/shadowless/icons/application.svg");
        auto *application = new QTableWidgetItem(icon, name);
        m_applications->setItem(row, 0, application);
        auto *choice = new ClickToAdjust<QComboBox>;
        choice->setAccessibleName("Audio track for " + name);
        choice->addItem("Unassigned / PC", 0);
        if (m_audioMode->currentIndex() == 2) {
            choice->addItem("Game (track 2)", 2);
            choice->addItem("Music (track 3)", 3);
        } else {
            for (int i = 1; i <= 6; ++i) choice->addItem(QString("Track %1").arg(i), i);
        }
        choice->setCurrentIndex(qMax(0, choice->findData(track)));
        m_applications->setCellWidget(row, 1, choice);
    }
}

QMap<QString, int> SettingsPage::applicationTracks() const {
    QMap<QString, int> tracks;
    for (int row = 0; row < m_applications->rowCount(); ++row) {
        const auto track = static_cast<QComboBox *>(m_applications->cellWidget(row, 1))->currentData().toInt();
        if (track > 0) tracks.insert(m_applications->item(row, 0)->text(), track);
    }
    return tracks;
}

void SettingsPage::setShortcutStatus(const QString &message) { m_shortcuts->setText(message); }
void SettingsPage::setCaptureActive(bool active) {
    m_captureFields->setEnabled(!active);
    m_activeNotice->setText("Capture is active. Stop Instant Clip or recording from the dashboard to change capture settings.");
    m_activeNotice->setVisible(active);
}
} // namespace Shadowless
