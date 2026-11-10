#pragma once
#include <QDateTime>
#include <QFutureWatcher>
#include <QHash>
#include <QPixmap>
#include <QPointer>
#include <QProcess>
#include <QQueue>
#include <QTimer>
#include <QWidget>

class QLabel;
class QVBoxLayout;

namespace Shadowless {
class Library : public QWidget {
    Q_OBJECT
public:
    explicit Library(QWidget *parent = nullptr);
    ~Library() override;
    void refresh(const QString &directory);
signals:
    void openingClip();
    void openFailed(const QString &message);
    void storageChanged(qint64 available, qint64 total);
protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
private:
    struct Clip {
        QString path;
        QDateTime modified;
        qint64 size;
        bool operator==(const Clip &other) const {
            return path == other.path && modified == other.modified && size == other.size;
        }
    };
    struct Scan {
        QString directory;
        QList<Clip> clips;
        qint64 available = 0;
        qint64 total = 0;
        QString emptyText;
    };
    struct Thumbnail {
        Clip clip;
        QPixmap pixmap;
    };

    QVBoxLayout *m_rows;
    QLabel *m_empty;
    QFutureWatcher<Scan> m_scan;
    QProcess m_thumbnail;
    QTimer m_timeout;
    QQueue<QPair<QPointer<QLabel>, Clip>> m_queue;
    QHash<QString, Thumbnail> m_cache;
    QPointer<QLabel> m_current;
    Clip m_currentClip{};
    QList<Clip> m_clips;
    QString m_directory, m_renderedDirectory;
    QString m_ffmpeg;
    bool m_scanPending = false;
    bool m_scanning = false;
    bool m_retryThumbnails = false;

    void startScan();
    void render(const Scan &scan);
    void next();
};
} // namespace Shadowless
