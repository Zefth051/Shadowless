#pragma once
#include <QJsonArray>
#include <QUrl>
#include <optional>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QNetworkReply>
#include <QTemporaryFile>
#include <memory>

namespace Shadowless {
struct LinuxRelease {
    QString version;
    QUrl url;
    QByteArray sha256;
    qint64 size = 0;
};
std::optional<LinuxRelease> newestLinuxRelease(const QJsonArray &releases, const QString &current);
QString installAppImage(const QString &destination, const QString &download, const QByteArray &digest);
class UpdateService : public QObject {
    Q_OBJECT
public:
    explicit UpdateService(QObject *parent = nullptr);
    static bool enabled();
    void check(bool interactive);
    void download(const LinuxRelease &release);
signals:
    void available(const Shadowless::LinuxRelease &release);
    void status(const QString &message);
private:
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
    std::optional<LinuxRelease> m_newest;
    void page(int number, bool interactive);
};
}
