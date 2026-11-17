#pragma once
#include <QDateTime>
#include <QFutureWatcher>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <atomic>

namespace Shadowless {
// Lossy HEVC re-encode of old H.264 clips, run only while the user is idle (no game, not saving).
// Originals are replaced only after the new file passes ffprobe validation and is smaller.
class IdleCompressor : public QObject {
    Q_OBJECT
public:
    enum class Result { Compressed, Skipped, Failed, Cancelled };
    explicit IdleCompressor(QObject *parent = nullptr);
    ~IdleCompressor() override;
    void setEnabled(bool enabled);
    void setRoot(const QString &root);
    void setBusy(bool busy);      // true: a game is open or capture is saving; cancels the running job
    bool working() const { return m_watcher.isRunning(); }
    static QStringList candidates(const QString &root, const QDateTime &now, int minimumAgeSeconds = 600);
    static QStringList ffmpegArguments(const QString &input, const QString &output);
    static bool validate(const QString &original, const QString &compressed, QString *error);
    static Result compressFile(const QString &path, std::atomic_bool &cancel, QString *detail);
signals:
    void compressed(const QString &path, qint64 savedBytes);
private:
    void next();
    QString m_root;
    bool m_enabled = false, m_busy = true;
    std::atomic_bool m_cancel{false};
    QSet<QString> m_done;
    QString m_current;
    QFutureWatcher<qint64> m_watcher;
};
} // namespace Shadowless
