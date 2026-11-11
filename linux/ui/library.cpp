#include "library.h"
#include <QDir>
#include <QDirIterator>
#include <QComboBox>
#include <QSignalBlocker>
#include <algorithm>
#include <QFileInfo>
#include <QFile>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QScrollArea>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QVBoxLayout>
#include <QtConcurrentRun>

namespace Shadowless {
Library::Library(QWidget *parent) : QWidget(parent), m_ffmpeg(QStandardPaths::findExecutable("ffmpeg")) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *header = new QHBoxLayout;
    auto *title = new QLabel("RECENT CLIPS");
    title->setObjectName("sectionTitle");
    header->addWidget(title);
    header->addStretch();
    auto *refreshButton = new QPushButton("↻");
    refreshButton->setObjectName("refreshClips");
    refreshButton->setToolTip("Refresh recent clips");
    refreshButton->setAccessibleName("Refresh recent clips");
    header->addWidget(refreshButton);
    layout->addLayout(header);
    m_collection = new QComboBox;
    m_collection->setObjectName("collectionFilter");
    m_collection->setAccessibleName("Filter clips by game");
    m_collection->addItem("All games", QString());
    layout->addWidget(m_collection);
    connect(m_collection, &QComboBox::currentIndexChanged, this, [this] { render(m_lastScan); });

    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *container = new QWidget;
    m_rows = new QVBoxLayout(container);
    m_rows->setContentsMargins(0, 0, 2, 0);
    m_rows->setAlignment(Qt::AlignTop);
    m_rows->setSpacing(7);
    m_empty = new QLabel("Loading clips…");
    m_empty->setWordWrap(true);
    m_empty->setObjectName("emptyClips");
    m_rows->addWidget(m_empty);
    scroll->setWidget(container);
    layout->addWidget(scroll, 1);

    m_deleteConfirmation = new QWidget;
    auto *confirmation = new QVBoxLayout(m_deleteConfirmation);
    m_deleteName = new QLabel;
    m_deleteName->setTextFormat(Qt::PlainText);
    m_deleteName->setWordWrap(true);
    confirmation->addWidget(m_deleteName);
    auto *actions = new QHBoxLayout;
    auto *cancel = new QPushButton("Cancel");
    cancel->setObjectName("cancelDeleteClip");
    auto *confirm = new QPushButton("Move to Trash");
    confirm->setObjectName("confirmDeleteClip");
    actions->addWidget(cancel); actions->addWidget(confirm);
    confirmation->addLayout(actions);
    layout->addWidget(m_deleteConfirmation);
    m_deleteConfirmation->hide();
    m_deleteError = new QLabel;
    m_deleteError->setObjectName("deleteClipError");
    m_deleteError->setTextFormat(Qt::PlainText);
    m_deleteError->setWordWrap(true);
    layout->addWidget(m_deleteError);
    m_deleteError->hide();
    connect(cancel, &QPushButton::clicked, this, [this] {
        m_pendingDelete.clear(); m_deleteConfirmation->hide();
    });
    connect(confirm, &QPushButton::clicked, this, [this] {
        const auto path = m_pendingDelete;
        m_pendingDelete.clear(); m_deleteConfirmation->hide();
        const QFileInfo file(path);
        const auto root = QFileInfo(m_directory).canonicalFilePath();
        const auto relative = QDir(root).relativeFilePath(file.canonicalFilePath());
        if (root.isEmpty() || !file.isFile() || file.isSymLink() || relative == ".." ||
            relative.startsWith("../") || QDir::isAbsolutePath(relative)) {
            m_deleteError->setText("The clip is missing or is outside the clips folder.");
            m_deleteError->show(); return;
        }
        QFile clip(path);
        if (!clip.moveToTrash()) {
            m_deleteError->setText("Could not move the clip to Trash: " + clip.errorString());
            m_deleteError->show(); return;
        }
        m_deleteError->hide();
        refresh(m_directory);
    });

    connect(refreshButton, &QPushButton::clicked, this, [this] {
        m_retryThumbnails = true;
        refresh(m_directory);
    });
    connect(&m_scan, &QFutureWatcher<Scan>::finished, this, [this] {
        m_scanning = false;
        const auto scan = m_scan.result();
        if (scan.directory == m_directory) { m_lastScan = scan; render(scan); }
        startScan();
    });
    m_timeout.setSingleShot(true);
    m_thumbnail.setStandardErrorFile(QProcess::nullDevice());
    connect(&m_timeout, &QTimer::timeout, &m_thumbnail, &QProcess::kill);
    connect(&m_thumbnail, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        m_timeout.stop();
        const auto bytes = m_thumbnail.readAllStandardOutput();
        if (m_current && code == 0 && status == QProcess::NormalExit) {
            QPixmap image;
            if (image.loadFromData(bytes)) {
                m_current->setPixmap(image);
                m_cache.insert(m_currentClip.path, {m_currentClip, image});
                emit thumbnailReady(m_currentClip.path, image);
            }
        }
        m_current.clear();
        next();
    });
    connect(&m_thumbnail, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart) return;
        m_timeout.stop();
        m_current.clear();
        m_queue.clear();
    });
}

Library::~Library() {
    m_thumbnail.disconnect(this);
    if (m_thumbnail.state() != QProcess::NotRunning) {
        m_thumbnail.kill();
        m_thumbnail.waitForFinished(1000);
    }
}

void Library::refresh(const QString &directory) {
    if (m_directory != directory) {
        m_pendingDelete.clear(); m_deleteConfirmation->hide(); m_deleteError->hide();
        m_directory = directory;
        Scan loading;
        loading.directory = directory;
        loading.emptyText = "Loading clips…";
        m_lastScan = loading;
        render(loading);
    }
    m_scanPending = true;
    startScan();
}

void Library::startScan() {
    if (!isVisible() || !m_scanPending || m_scanning) return;
    m_scanPending = false;
    m_scanning = true;
    m_scan.setFuture(QtConcurrent::run([directory = m_directory] {
        Scan result;
        result.directory = directory;
        if (directory.isEmpty()) {
            result.emptyText = "Choose a save folder in Settings.";
            return result;
        }

        const QDir folder(directory);
        QHash<QString, QList<Clip>> collections;
        const auto newer = [](const Clip &a, const Clip &b) {
            return a.modified != b.modified ? a.modified > b.modified : a.path < b.path;
        };
        QDirIterator files(directory, {"*.mp4", "*.mkv"}, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
        while (files.hasNext()) {
            files.next();
            const auto file = files.fileInfo();
            if (file.size() <= 0) continue;
            const auto collection = folder.relativeFilePath(file.absolutePath());
            auto &recent = collections[collection];
            const Clip clip{file.absoluteFilePath(), file.lastModified(), file.size(), collection};
            const auto position = std::lower_bound(recent.begin(), recent.end(), clip, newer);
            if (position != recent.end() || recent.size() < 20) {
                recent.insert(position, clip);
                if (recent.size() > 20) recent.removeLast();
            }
        }
        for (const auto &recent : collections) result.clips.append(recent);
        std::sort(result.clips.begin(), result.clips.end(), newer);
        result.collections = collections.keys(); result.collections.sort(Qt::CaseInsensitive);
        result.emptyText = "Saved replays and recordings appear here.";
        if (folder.exists() && !folder.isReadable()) result.emptyText = "The clips folder cannot be read.";

        QFileInfo volume(directory);
        while (!volume.exists() && !volume.dir().isRoot()) volume = QFileInfo(volume.absolutePath());
        const QStorageInfo storage(volume.exists() ? volume.absoluteFilePath() : volume.absolutePath());
        if (storage.isValid() && storage.isReady()) {
            result.available = storage.bytesAvailable();
            result.total = storage.bytesTotal();
        }
        return result;
    }));
}

void Library::render(const Scan &scan) {
    const auto selected = m_collection->currentData().toString();
    {
        const QSignalBlocker blocker(m_collection);
        m_collection->clear(); m_collection->addItem("All games", QString());
        for (const auto &collection : scan.collections) m_collection->addItem(collection == "." ? "Desktop / Unidentified" : collection, collection);
        m_collection->setCurrentIndex(qMax(0, m_collection->findData(selected)));
    }
    QList<Clip> visible;
    const auto filter = m_collection->currentData().toString();
    for (const auto &clip : scan.clips) {
        if (!filter.isEmpty() && clip.collection != filter) continue;
        visible.append(clip);
        if (visible.size() == 20) break;
    }
    emit storageChanged(scan.available, scan.total);
    m_empty->setText(scan.emptyText);
    const bool retry = m_retryThumbnails && m_cache.size() < m_clips.size();
    m_retryThumbnails = false;
    if (m_renderedDirectory == scan.directory && m_clips == visible && !retry) return;

    m_renderedDirectory = scan.directory;
    m_clips = visible;
    QStringList paths;
    for (const auto &clip : m_clips) paths.append(clip.path);
    emit clipsChanged(paths);
    m_queue.clear();
    while (auto *item = m_rows->takeAt(1)) {
        delete item->widget();
        delete item;
    }

    QHash<QString, Thumbnail> retained;
    for (const auto &clip : m_clips) {
        auto *row = new QWidget;
        row->setObjectName("clip");
        auto *line = new QHBoxLayout(row);
        line->setContentsMargins(4, 4, 8, 4);
        auto *thumb = new QLabel("▶");
        thumb->setObjectName("thumbnail");
        thumb->setFixedSize(112, 64);
        thumb->setAlignment(Qt::AlignCenter);
        line->addWidget(thumb);
        const auto previous = m_cache.constFind(clip.path);
        if (previous != m_cache.cend() && previous->clip == clip) {
            thumb->setPixmap(previous->pixmap);
            retained.insert(clip.path, *previous);
            emit thumbnailReady(clip.path, previous->pixmap);
        } else {
            m_queue.enqueue({thumb, clip});
        }

        const QFileInfo file(clip.path);
        auto *labels = new QVBoxLayout;
        auto *name = new QLabel;
        name->setObjectName("clipName");
        name->setTextFormat(Qt::PlainText);
        name->setText(name->fontMetrics().elidedText(file.completeBaseName(), Qt::ElideMiddle, 170));
        name->setToolTip(file.fileName());
        labels->addWidget(name);
        auto *meta = new QLabel(QLocale().toString(clip.modified, QLocale::ShortFormat) +
                                " · " + QLocale().formattedDataSize(clip.size, 1));
        meta->setProperty("muted", true);
        meta->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        meta->setToolTip(meta->text());
        labels->addWidget(meta);
        line->addLayout(labels, 1);

        auto *play = new QPushButton("Open");
        play->setAccessibleName("Open " + file.fileName());
        play->setToolTip("Play inside Shadowless");
        play->setIcon(QIcon(":/shadowless/icons/play.svg"));
        line->addWidget(play);
        connect(play, &QPushButton::clicked, this, [this, path = clip.path] {
            emit clipRequested(path);
        });
        auto *remove = new QPushButton;
        remove->setObjectName("deleteClip");
        remove->setIcon(QIcon(":/shadowless/icons/trash.svg"));
        remove->setAccessibleName("Delete " + file.fileName());
        remove->setToolTip("Move clip to Trash");
        remove->setFixedWidth(32);
        line->addWidget(remove);
        connect(remove, &QPushButton::clicked, this, [this, path = clip.path] {
            m_pendingDelete = path;
            m_deleteError->hide();
            m_deleteName->setText("Move " + QFileInfo(path).fileName() + " to Trash?");
            m_deleteConfirmation->show();
            m_deleteConfirmation->findChild<QPushButton *>("cancelDeleteClip")->setFocus();
        });
        m_rows->addWidget(row);
    }
    m_cache.swap(retained);
    m_empty->setVisible(m_clips.isEmpty());
    next();
}

void Library::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    startScan();
    next();
}

void Library::hideEvent(QHideEvent *event) {
    if (!m_previewActive) setPreviewActive(false);
    QWidget::hideEvent(event);
}

void Library::setPreviewActive(bool active) {
    m_previewActive = active;
    if (active || isVisible()) { next(); return; }
    if (m_current) {
        m_queue.prepend({m_current, m_currentClip});
        m_current.clear();
    }
    if (m_thumbnail.state() != QProcess::NotRunning) m_thumbnail.kill();
}

void Library::next() {
    if ((!isVisible() && !m_previewActive) || m_queue.isEmpty() || m_ffmpeg.isEmpty() || m_thumbnail.state() != QProcess::NotRunning) return;
    const auto item = m_queue.dequeue();
    m_current = item.first;
    m_currentClip = item.second;
    m_timeout.start(5000);
    m_thumbnail.start(m_ffmpeg, {"-nostdin", "-v", "error", "-filter_threads", "1", "-threads", "1", "-i", m_currentClip.path,
        "-frames:v", "1", "-vf", "scale=112:64:force_original_aspect_ratio=increase,crop=112:64", "-f", "image2pipe", "-vcodec", "png", "-threads:v", "1", "-"});
}
} // namespace Shadowless
