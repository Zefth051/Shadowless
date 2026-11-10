#include "../capture/gsr_capture.h"
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QQueue>
#include <QRadioButton>
#include <QScrollArea>
#include <QSettings>
#include <QShortcut>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QUrl>
#include <QVBoxLayout>
#include <QXmlStreamReader>

using namespace Captail;

static QHash<QString, QString> resources(const QString &path, const QString &element) {
    QFile file(path);
    QHash<QString, QString> values;
    if (!file.open(QIODevice::ReadOnly)) return values;
    QXmlStreamReader xml(&file);
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == element) {
            const auto key = xml.attributes().value("http://schemas.microsoft.com/winfx/2006/xaml", "Key").toString();
            values[key] = xml.readElementText();
        }
    }
    return values;
}

class CaptureRing : public QWidget {
public:
    bool enabled = false;
    explicit CaptureRing(QWidget *parent = nullptr) : QWidget(parent) {
        setFixedSize(48, 48); setStyleSheet("background: transparent;");
    }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this); painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(enabled ? QColor("#63e0bd") : QColor("#4a545b"), 3));
        painter.drawArc(QRectF(4, 4, 40, 40), 60 * 16, 310 * 16);
        painter.setPen(Qt::NoPen);
        painter.setBrush(enabled ? QColor("#63e0bd") : QColor("#4a545b"));
        painter.drawEllipse(QPointF(24, 24), 4, 4);
    }
};

class CaptailWindow : public QWidget {
    GsrCapabilities m_capabilities;
    GsrOptions m_options;
    GsrCapture m_engine;
    QSettings m_settings;
    QHash<QString, QString> m_strings;
    CaptureRing *m_ring;
    QLabel *m_status, *m_detail, *m_format, *m_notice, *m_folder;
    QPushButton *m_toggle, *m_settingsButton, *m_save;
    QRadioButton *m_replayMode, *m_recordMode;
    QCheckBox *m_system, *m_microphone;
    QSpinBox *m_saveSeconds;
    QVBoxLayout *m_clips;
    bool m_closing = false, m_stopping = false, m_saving = false;
    QProcess m_thumbnail;
    QTimer m_thumbnailTimeout;
    QQueue<QPair<QPointer<QLabel>, QString>> m_thumbnailQueue;
    QPointer<QLabel> m_currentThumbnail;

public:
    explicit CaptailWindow(const GsrCapabilities &capabilities, const QString &probeError)
        : m_capabilities(capabilities), m_settings("Captail", "LinuxGsrSpike"),
          m_strings(resources(":/captail/strings.xaml", "String")) {
        setWindowTitle("Captail — Linux preview");
        setWindowIcon(QIcon(":/captail/icon.ico"));
        setMinimumSize(420, 600); resize(460, 750);
        loadSettings();
        const auto colors = resources(":/captail/theme.xaml", "Color");
        auto color = [&colors](const QString &key, const QString &fallback) {
            return QColor(colors.value(key, fallback)).name(QColor::HexRgb);
        };
        setStyleSheet(QString(R"(
            QWidget { background: %1; color: %2; font-family: sans-serif; font-size: 13px; }
            QLabel { background: transparent; }
            QLabel[muted="true"] { color: %3; font-size: 12px; }
            QFrame#card, QWidget#clip { background: %4; border: 1px solid %5; border-radius: 12px; }
            QPushButton { background: %4; border: 1px solid %5; border-radius: 8px; padding: 9px 12px; }
            QPushButton:hover { border-color: %6; }
            QPushButton:disabled { color: %3; }
            QPushButton#save { background: %6; color: #0b201a; border: 0; font-weight: bold; font-size: 15px; padding: 13px; }
            QPushButton#save:disabled { background: #29433c; color: %3; }
            QRadioButton { background: #101416; padding: 11px; border-radius: 8px; color: %3; }
            QRadioButton:checked { background: #1d3530; color: %6; font-weight: bold; }
            QRadioButton::indicator { width: 0; }
            QCheckBox { spacing: 7px; }
            QComboBox, QSpinBox, QLineEdit { background: #101416; border: 1px solid %5; border-radius: 6px; padding: 7px; }
            QScrollArea { border: 0; }
            QScrollBar:vertical { background: transparent; width: 8px; }
            QScrollBar::handle:vertical { background: #39444a; border-radius: 4px; min-height: 24px; }
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        )").arg(color("ColorBgWindow", "#141719"), color("ColorTextPrimary", "#e7ecea"),
            color("ColorTextSecondary", "#8b969d"), color("ColorBgSurface", "#181d20"),
            color("ColorBorder", "#242a2e"), color("ColorAccent", "#63e0bd")));

        auto *layout = new QVBoxLayout(this); layout->setContentsMargins(18, 18, 18, 16); layout->setSpacing(14);
        auto *header = new QHBoxLayout;
        auto *icon = new QLabel; icon->setPixmap(windowIcon().pixmap(24, 24)); header->addWidget(icon);
        auto *brand = new QLabel(text("L.Brand")); brand->setStyleSheet("font-weight: bold; font-size: 16px;"); header->addWidget(brand);
        header->addStretch();
        m_settingsButton = new QPushButton(text("L.Settings")); header->addWidget(m_settingsButton); layout->addLayout(header);

        auto *card = new QFrame; card->setObjectName("card"); auto *statusRow = new QHBoxLayout(card);
        m_ring = new CaptureRing; statusRow->addWidget(m_ring);
        auto *statusText = new QVBoxLayout;
        m_status = new QLabel; m_status->setStyleSheet("font-size: 15px; font-weight: bold;");
        m_detail = new QLabel; m_detail->setProperty("muted", true); m_detail->setWordWrap(true);
        statusText->addWidget(m_status); statusText->addWidget(m_detail); statusRow->addLayout(statusText, 1);
        m_toggle = new QPushButton("Start"); m_toggle->setObjectName("captureToggle"); statusRow->addWidget(m_toggle); layout->addWidget(card);

        auto *modes = new QHBoxLayout;
        m_replayMode = new QRadioButton(text("L.Library.ModeReplay"));
        m_recordMode = new QRadioButton(text("L.Library.ModeRecording"));
        m_replayMode->setChecked(!m_options.continuousRecording); m_recordMode->setChecked(m_options.continuousRecording);
        modes->addWidget(m_replayMode, 1); modes->addWidget(m_recordMode, 1); layout->addLayout(modes);

        auto *audio = new QHBoxLayout;
        m_system = new QCheckBox("System audio"); m_system->setChecked(m_options.systemAudio);
        m_microphone = new QCheckBox(text("L.Audio.Microphone")); m_microphone->setChecked(m_options.microphone);
        audio->addWidget(m_system); audio->addWidget(m_microphone); audio->addStretch(); layout->addLayout(audio);
        m_format = new QLabel; m_format->setProperty("muted", true); layout->addWidget(m_format);

        auto *saveRow = new QHBoxLayout;
        m_save = new QPushButton(text("L.Tray.Save")); m_save->setObjectName("save");
        m_save->setToolTip("Ctrl+Shift+F10 while this window is focused");
        m_saveSeconds = new QSpinBox; m_saveSeconds->setRange(1, m_options.bufferSeconds); m_saveSeconds->setValue(qMin(10, m_options.bufferSeconds)); m_saveSeconds->setSuffix(" sec");
        saveRow->addWidget(m_save, 1); saveRow->addWidget(m_saveSeconds); layout->addLayout(saveRow);
        m_folder = new QLabel; m_folder->setProperty("muted", true); m_folder->setTextInteractionFlags(Qt::TextBrowserInteraction);
        m_folder->setOpenExternalLinks(false); layout->addWidget(m_folder);
        auto *libraryHeader = new QHBoxLayout;
        auto *recent = new QLabel(text("L.Library.Recent")); recent->setProperty("muted", true); libraryHeader->addWidget(recent); libraryHeader->addStretch();
        auto *refresh = new QPushButton("↻"); refresh->setToolTip("Refresh clips"); libraryHeader->addWidget(refresh); layout->addLayout(libraryHeader);
        auto *scroll = new QScrollArea; scroll->setWidgetResizable(true);
        auto *clipContainer = new QWidget; m_clips = new QVBoxLayout(clipContainer); m_clips->setContentsMargins(0, 0, 4, 0); m_clips->setSpacing(8); m_clips->setAlignment(Qt::AlignTop);
        scroll->setWidget(clipContainer); layout->addWidget(scroll, 1);
        m_notice = new QLabel; m_notice->setWordWrap(true); m_notice->setTextFormat(Qt::PlainText); m_notice->setProperty("muted", true); layout->addWidget(m_notice);
        auto *footer = new QLabel("Linux preview"); footer->setProperty("muted", true); footer->setToolTip("Qt 6 / C++ · GPU Screen Recorder " + m_capabilities.version); layout->addWidget(footer);

        connect(m_settingsButton, &QPushButton::clicked, this, [this] { editSettings(); });
        connect(m_toggle, &QPushButton::clicked, this, [this] {
            m_notice->clear();
            if (m_engine.active()) { m_stopping = true; m_engine.stop(); }
            else { m_options.systemAudio = m_system->isChecked(); m_options.microphone = m_microphone->isChecked(); saveSettings(); m_engine.start(m_options, m_capabilities); }
            updateState();
        });
        connect(m_recordMode, &QRadioButton::toggled, this, [this](bool recording) { m_options.continuousRecording = recording; saveSettings(); updateState(); });
        connect(m_save, &QPushButton::clicked, this, [this] { m_saving = true; updateState(); m_engine.saveReplay(m_saveSeconds->value()); });
        auto *saveShortcut = new QShortcut(QKeySequence("Ctrl+Shift+F10"), this);
        connect(saveShortcut, &QShortcut::activated, m_save, [this] { if (m_save->isEnabled()) m_save->click(); });
        connect(m_folder, &QLabel::linkActivated, this, [this] { QDesktopServices::openUrl(QUrl::fromLocalFile(m_options.outputDirectory)); });
        connect(refresh, &QPushButton::clicked, this, [this] { refreshLibrary(); });
        connect(&m_engine, &GsrCapture::started, this, [this] { updateState(); });
        connect(&m_engine, &GsrCapture::stopped, this, [this] {
            m_stopping = false; m_saving = false; updateState(); refreshLibrary();
            if (m_closing) close();
        });
        connect(&m_engine, &GsrCapture::clipSaved, this, [this](const QString &path) {
            m_saving = false; m_notice->setText("Saved " + QFileInfo(path).fileName()); updateState(); refreshLibrary();
        });
        connect(&m_engine, &GsrCapture::error, this, [this](const QString &message) {
            m_saving = false; m_notice->setText(message); updateState();
        });
        m_thumbnailTimeout.setSingleShot(true);
        connect(&m_thumbnailTimeout, &QTimer::timeout, &m_thumbnail, &QProcess::kill);
        connect(&m_thumbnail, &QProcess::finished, this, [this](int code, QProcess::ExitStatus) {
            m_thumbnailTimeout.stop();
            if (code == 0 && m_currentThumbnail) {
                QPixmap image;
                if (image.loadFromData(m_thumbnail.readAllStandardOutput(), "PNG")) m_currentThumbnail->setPixmap(image);
            }
            m_thumbnail.readAllStandardError();
            nextThumbnail();
        });
        if (!probeError.isEmpty()) m_notice->setText(probeError);
        updateState(); refreshLibrary();
    }

protected:
    void closeEvent(QCloseEvent *event) override {
        if (m_engine.active()) {
            event->ignore(); m_closing = true; m_stopping = true; m_engine.stop(); updateState();
        } else { m_thumbnail.kill(); m_thumbnail.waitForFinished(1000); event->accept(); }
    }

private:
    QString text(const QString &key) const { return m_strings.value(key, key); }

    void loadSettings() {
        m_options.source = m_settings.value("source", m_capabilities.sources.value(0)).toString();
        m_options.codec = m_settings.value("codec", "h264").toString();
        if (!m_capabilities.sources.contains(m_options.source)) m_options.source = m_capabilities.sources.value(0);
        if (!m_capabilities.codecs.contains(m_options.codec)) m_options.codec = m_capabilities.codecs.value(0);
        m_options.container = m_settings.value("container", "mp4").toString();
        m_options.frameRate = m_settings.value("fps", 60).toInt();
        m_options.bufferSeconds = qBound(1, m_settings.value("bufferSeconds", 30).toInt(), 3600);
        m_options.bitrateMbps = m_settings.value("bitrateMbps", 0).toInt();
        m_options.systemAudio = m_settings.value("systemAudio", true).toBool();
        m_options.microphone = m_settings.value("microphone", false).toBool();
        m_options.separateAudioTracks = m_settings.value("separateAudioTracks", false).toBool();
        m_options.continuousRecording = m_settings.value("continuousRecording", false).toBool();
        auto videos = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
        if (videos.isEmpty()) videos = QDir::home().filePath("Videos");
        m_options.outputDirectory = m_settings.value("outputDirectory", QDir(videos).filePath("Captail")).toString();
    }

    void saveSettings() {
        m_settings.setValue("source", m_options.source); m_settings.setValue("codec", m_options.codec);
        m_settings.setValue("container", m_options.container); m_settings.setValue("fps", m_options.frameRate);
        m_settings.setValue("bufferSeconds", m_options.bufferSeconds); m_settings.setValue("bitrateMbps", m_options.bitrateMbps);
        m_settings.setValue("systemAudio", m_system->isChecked()); m_settings.setValue("microphone", m_microphone->isChecked());
        m_settings.setValue("separateAudioTracks", m_options.separateAudioTracks);
        m_settings.setValue("continuousRecording", m_options.continuousRecording); m_settings.setValue("outputDirectory", m_options.outputDirectory);
    }

    void updateState() {
        const bool active = m_engine.active(), ready = m_engine.running();
        m_ring->enabled = ready; m_ring->update();
        m_status->setText(m_stopping ? "Finishing recording…" : active && !ready ? "Starting capture…" : ready
            ? text(m_options.continuousRecording ? "L.Capture.RecordingEnabled" : "L.Status.Enabled")
            : m_options.continuousRecording ? "Recording is off" : text("L.Status.Disabled"));
        m_detail->setText(m_options.source + (m_options.continuousRecording ? " · Continuous recording" : QString(" · Last %1 sec").arg(m_options.bufferSeconds)));
        m_toggle->setText(active ? "Stop" : "Start");
        m_toggle->setEnabled(!m_stopping && (!m_capabilities.codecs.isEmpty() || active));
        m_settingsButton->setEnabled(!active); m_replayMode->setEnabled(!active); m_recordMode->setEnabled(!active);
        m_system->setEnabled(!active); m_microphone->setEnabled(!active);
        m_save->setVisible(!m_options.continuousRecording); m_saveSeconds->setVisible(!m_options.continuousRecording);
        m_save->setEnabled(ready && !m_saving && !m_stopping); m_save->setText(m_saving ? "Saving…" : text("L.Tray.Save"));
        m_saveSeconds->setEnabled(!m_saving && !m_stopping);
        QFileInfo storagePath(QDir(m_options.outputDirectory).absolutePath());
        while (!storagePath.exists() && !storagePath.isRoot()) storagePath.setFile(storagePath.absolutePath());
        const QStorageInfo storage(storagePath.absoluteFilePath());
        const auto disk = storage.isValid() && storage.isReady()
            ? QString("%1 GB free").arg(storage.bytesAvailable() / (1024 * 1024 * 1024)) : QString("Disk unavailable");
        m_format->setText(QString("%1  ·  %2 FPS  ·  %3").arg(m_options.codec.toUpper()).arg(m_options.frameRate).arg(disk));
        m_folder->setText("<a style='color:#8b969d; text-decoration:none;' href='folder'>" + m_options.outputDirectory.toHtmlEscaped() + "</a>");
    }

    void editSettings() {
        QDialog dialog(this); dialog.setWindowTitle(text("L.Settings")); dialog.setMinimumWidth(410);
        auto *layout = new QVBoxLayout(&dialog); auto *form = new QFormLayout; form->setSpacing(12);
        auto *source = new QComboBox; source->addItems(m_capabilities.sources); source->setCurrentText(m_options.source); form->addRow("Screen", source);
        auto *codec = new QComboBox; codec->addItems(m_capabilities.codecs); codec->setCurrentText(m_options.codec); form->addRow("Video codec", codec);
        auto *fps = new QComboBox; fps->addItems({"30", "60", "120", "144", "240"}); fps->setCurrentText(QString::number(m_options.frameRate)); form->addRow("Frame rate", fps);
        auto *buffer = new QSpinBox; buffer->setRange(1, 3600); buffer->setValue(m_options.bufferSeconds); buffer->setSuffix(" sec"); form->addRow("Replay length", buffer);
        auto *rate = new QSpinBox; rate->setRange(0, 100); rate->setValue(m_options.bitrateMbps); rate->setSpecialValueText("Automatic"); rate->setSuffix(" Mbps"); form->addRow("Bitrate", rate);
        auto *container = new QComboBox; container->addItems({"mp4", "mkv"}); container->setCurrentText(m_options.container); form->addRow("File format", container);
        auto *separate = new QCheckBox("Separate system and microphone tracks"); separate->setChecked(m_options.separateAudioTracks); form->addRow(separate);
        auto *folder = new QLineEdit(m_options.outputDirectory); auto *browse = new QPushButton("Browse…");
        auto *folderRow = new QHBoxLayout; folderRow->addWidget(folder); folderRow->addWidget(browse); form->addRow("Save folder", folderRow);
        connect(browse, &QPushButton::clicked, &dialog, [&] {
            const auto selected = QFileDialog::getExistingDirectory(&dialog, "Save folder", folder->text());
            if (!selected.isEmpty()) folder->setText(selected);
        });
        layout->addLayout(form);
        auto *note = new QLabel("Replay uses RAM. Frame rates above 60 FPS are experimental."); note->setWordWrap(true); note->setProperty("muted", true); layout->addWidget(note);
        auto *validation = new QLabel; validation->setWordWrap(true); validation->setTextFormat(Qt::PlainText); layout->addWidget(validation);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel); layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
            auto candidate = m_options;
            candidate.source = source->currentText(); candidate.codec = codec->currentText(); candidate.frameRate = fps->currentText().toInt();
            candidate.bufferSeconds = buffer->value(); candidate.bitrateMbps = rate->value(); candidate.container = container->currentText();
            candidate.separateAudioTracks = separate->isChecked(); candidate.outputDirectory = folder->text();
            const auto error = candidate.validate(m_capabilities);
            if (!error.isEmpty()) { validation->setText(error); return; }
            m_options = candidate; dialog.accept();
        });
        if (dialog.exec() == QDialog::Accepted) {
            saveSettings(); m_saveSeconds->setMaximum(m_options.bufferSeconds); updateState(); refreshLibrary();
        }
    }

    void refreshLibrary() {
        m_thumbnailQueue.clear();
        while (auto *item = m_clips->takeAt(0)) { delete item->widget(); delete item; }
        const auto files = QDir(m_options.outputDirectory).entryInfoList({"*.mp4", "*.mkv"}, QDir::Files | QDir::NoSymLinks, QDir::Time);
        for (const auto &file : files.mid(0, 20)) {
            if (file.size() == 0) continue;
            auto *row = new QWidget; row->setObjectName("clip"); auto *line = new QHBoxLayout(row); line->setContentsMargins(8, 8, 10, 8);
            auto *thumbnail = new QLabel("▶"); thumbnail->setAlignment(Qt::AlignCenter); thumbnail->setFixedSize(112, 64);
            thumbnail->setStyleSheet("background:#101416; border-radius:6px; color:#63e0bd; font-size:24px;"); line->addWidget(thumbnail);
            auto *labels = new QVBoxLayout;
            auto *name = new QLabel(fontMetrics().elidedText(file.completeBaseName(), Qt::ElideRight, 220)); name->setToolTip(file.fileName()); labels->addWidget(name);
            auto *meta = new QLabel(file.lastModified().toString("dd MMM yyyy HH:mm") + QString(" · %1 MB").arg(file.size() / 1048576.0, 0, 'f', 1)); meta->setProperty("muted", true); labels->addWidget(meta);
            line->addLayout(labels, 1);
            auto *open = new QPushButton("Play"); open->setToolTip("Open in your video player"); line->addWidget(open);
            connect(open, &QPushButton::clicked, row, [path = file.absoluteFilePath()] { QDesktopServices::openUrl(QUrl::fromLocalFile(path)); });
            m_clips->addWidget(row);
            m_thumbnailQueue.enqueue({thumbnail, file.absoluteFilePath()});
        }
        if (m_clips->count() == 0) {
            auto *empty = new QLabel(text("L.Library.Empty")); empty->setWordWrap(true); empty->setProperty("muted", true); m_clips->addWidget(empty);
        }
        if (m_thumbnail.state() == QProcess::NotRunning) nextThumbnail();
    }

    void nextThumbnail() {
        if (m_thumbnailQueue.isEmpty() || QStandardPaths::findExecutable("ffmpeg").isEmpty()) return;
        const auto next = m_thumbnailQueue.dequeue(); m_currentThumbnail = next.first;
        m_thumbnail.start("ffmpeg", {"-v", "error", "-threads", "1", "-ss", "0.5", "-i", next.second,
            "-frames:v", "1", "-vf", "scale=112:64:force_original_aspect_ratio=increase,crop=112:64", "-f", "image2pipe", "-vcodec", "png", "-"});
        m_thumbnailTimeout.start(5000);
    }
};

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("Captail Linux");
    QCoreApplication::setOrganizationName("Captail");
    QString error;
    const auto capabilities = GsrCapabilities::probe(&error);
    CaptailWindow window(capabilities, error);
    window.show();
    return app.exec();
}
