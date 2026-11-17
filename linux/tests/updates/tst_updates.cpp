#include "../../app/update_service.h"
#include <QCryptographicHash>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>
#include <QTimer>
#include <QScopeGuard>
#include <algorithm>

using namespace Shadowless;

static QJsonArray fixture() { return QJsonDocument::fromJson("[\n          {\"tag_name\":\"v9.0.0\",\"assets\":[]},\n          {\"tag_name\":\"linux-v0.2.5\",\"draft\":false,\"assets\":[\n            {\"name\":\"Shadowless-0.2.5-linux-x86_64.AppImage\",\"size\":100,\n             \"browser_download_url\":\"https://github.com/Zefth051/Shadowless/releases/download/linux-v0.2.5/Shadowless-0.2.5-linux-x86_64.AppImage\",\n             \"digest\":\"sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"}]}\n        ]").array();
}

class FixtureReply : public QNetworkReply {
    QByteArray m_body;
    qint64 m_position = 0;
public:
    FixtureReply(const QNetworkRequest &request, int code, NetworkError error, const QByteArray &body, QObject *parent) : QNetworkReply(parent), m_body(body) {
        setRequest(request); setUrl(request.url());
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, code);
        if (error != NoError) setError(error, "fixture transport failure");
        open(QIODevice::ReadOnly);
        QTimer::singleShot(0, this, [this] { emit readyRead(); setFinished(true); emit finished(); });
    }
    void abort() override {}
    qint64 bytesAvailable() const override { return m_body.size() - m_position + QNetworkReply::bytesAvailable(); }
protected:
    qint64 readData(char *data, qint64 maximum) override {
        const auto size = qMin(maximum, m_body.size() - m_position);
        if (size <= 0) return -1;
        std::copy_n(m_body.constData() + m_position, size, data); m_position += size;
        return size;
    }
};
class FixtureNetwork : public QNetworkAccessManager {
public:
    QByteArray body = "[]";
    int code = 403;
    QNetworkReply::NetworkError failure = QNetworkReply::ContentAccessDenied;
protected:
    QNetworkReply *createRequest(Operation, const QNetworkRequest &request, QIODevice *) override {
        return new FixtureReply(request, code, failure, body, this);
    }
};

class UpdateTests : public QObject {
    Q_OBJECT
private slots:
    void checksReleaseListAndRejectsInvalidJson() {
        QCoreApplication::setApplicationVersion("0.2.4");
        FixtureNetwork network; network.code = 200; network.failure = QNetworkReply::NoError;
        UpdateService service(nullptr, &network);
        QSignalSpy messages(&service, &UpdateService::status);
        QSignalSpy releases(&service, &UpdateService::available);
        service.check(true);
        QTRY_COMPARE(messages.size(), 1);
        QCOMPARE(messages.takeFirst().at(0).toString(), "Shadowless is up to date.");
        network.body = QJsonDocument(fixture()).toJson();
        service.check(true);
        QTRY_COMPARE(releases.size(), 1);
        QVERIFY(messages.isEmpty());
        network.body = "not JSON";
        service.check(true);
        QTRY_COMPARE(messages.size(), 1);
        QVERIFY(messages.takeFirst().at(0).toString().contains("invalid release list"));
        QCOMPARE(releases.size(), 1);
    }
    void reportsHttpAndTlsFailures() {
        FixtureNetwork network;
        UpdateService service(nullptr, &network);
        QSignalSpy messages(&service, &UpdateService::status);
        service.check(true);
        QTRY_COMPARE(messages.size(), 1);
        QVERIFY(messages.takeFirst().at(0).toString().contains("403"));
        network.code = 0; network.failure = QNetworkReply::SslHandshakeFailedError;
        service.check(true);
        QTRY_COMPARE(messages.size(), 1);
        QVERIFY(messages.takeFirst().at(0).toString().contains("TLS"));
        network.code = 429; network.failure = QNetworkReply::UnknownContentError;
        service.check(true);
        QTRY_COMPARE(messages.size(), 1);
        QVERIFY(messages.takeFirst().at(0).toString().contains("429"));
    }
    void selectsLinuxOnly() {
        auto releases = fixture();
        auto result = newestLinuxRelease(releases, "0.2.4");
        QVERIFY(result.has_value());
        QCOMPARE(result->version, "0.2.5");
        QVERIFY(!newestLinuxRelease(releases, "0.2.5"));
        QVERIFY(!newestLinuxRelease(releases, "0.2.6"));
        for (const auto &bad : {"https://github.com/attacker/Shadowless/releases/download/linux-v0.2.5/file", "http://github.com/Zefth051/Shadowless/releases/download/linux-v0.2.5/file", "https://user@github.com/Zefth051/Shadowless/releases/download/linux-v0.2.5/file"}) {
            auto release = releases[1].toObject(); auto assets = release["assets"].toArray();
            auto asset = assets[0].toObject(); asset["browser_download_url"] = bad;
            assets[0] = asset; release["assets"] = assets;
            QVERIFY(!newestLinuxRelease(QJsonArray{release}, "0.2.4"));
        }
        auto draft = releases[1].toObject(); draft["draft"] = true;
        QVERIFY(!newestLinuxRelease(QJsonArray{draft}, "0.2.4"));
        auto prerelease = releases[1].toObject(); prerelease["prerelease"] = true;
        QVERIFY(!newestLinuxRelease(QJsonArray{prerelease}, "0.2.4"));
        auto noDigest = releases[1].toObject(); auto assets = noDigest["assets"].toArray();
        auto asset = assets[0].toObject(); asset.remove("digest"); assets[0] = asset; noDigest["assets"] = assets;
        QVERIFY(!newestLinuxRelease(QJsonArray{noDigest}, "0.2.4"));
    }
    void publicRuntimeUpdateRoundTrip() {
        if (!qEnvironmentVariableIsSet("SHADOWLESS_TEST_PUBLIC_UPDATES"))
            QSKIP("Opt-in HTTPS/download test: set SHADOWLESS_TEST_PUBLIC_UPDATES=1.");
        QTemporaryDir directory; QVERIFY(directory.isValid());
        const auto destination = directory.filePath("isolated.AppImage");
        QFile original(destination); QVERIFY(original.open(QIODevice::WriteOnly));
        original.write("original fixture"); original.close();
        const auto previousPath = qgetenv("APPIMAGE");
        const bool hadPath = qEnvironmentVariableIsSet("APPIMAGE");
        const auto previousVersion = QCoreApplication::applicationVersion();
        const auto restore = qScopeGuard([&] {
            if (hadPath) qputenv("APPIMAGE", previousPath); else qunsetenv("APPIMAGE");
            QCoreApplication::setApplicationVersion(previousVersion);
        });
        qputenv("APPIMAGE", destination.toUtf8());
        QCoreApplication::setApplicationVersion("0.0.0");
        UpdateService service;
        QByteArray expected; QString completion;
        connect(&service, &UpdateService::available, this, [&](const LinuxRelease &release) {
            expected = release.sha256; service.download(release);
        });
        connect(&service, &UpdateService::status, this, [&](const QString &message) {
            if (!message.startsWith("Downloading")) completion = message;
        });
        service.check(true);
        QTRY_VERIFY_WITH_TIMEOUT(!completion.isEmpty(), 120000);
        QVERIFY2(completion.startsWith("Update installed."), qPrintable(completion));
        QVERIFY(original.open(QIODevice::ReadOnly));
        QCryptographicHash hash(QCryptographicHash::Sha256);
        QVERIFY(hash.addData(&original)); QCOMPARE(hash.result().toHex(), expected);
        QVERIFY(original.permissions() & QFileDevice::ExeOwner);
    }
    void preservesOriginalOnBadDigest() {
        QTemporaryDir directory;
        const auto destination = directory.filePath("installed.AppImage");
        const auto download = directory.filePath("download");
        QFile old(destination); QVERIFY(old.open(QIODevice::WriteOnly)); old.write("original"); old.close();
        QFile next(download); QVERIFY(next.open(QIODevice::WriteOnly)); next.write("replacement"); next.close();
        QVERIFY(!installAppImage(destination, download, QByteArray(64, 'a')).isEmpty());
        QVERIFY(old.open(QIODevice::ReadOnly)); QCOMPARE(old.readAll(), QByteArray("original")); old.close();
        const auto digest = QCryptographicHash::hash("replacement", QCryptographicHash::Sha256).toHex();
        QVERIFY2(installAppImage(destination, download, digest).isEmpty(), "Verified update should replace original atomically");
        QVERIFY(old.open(QIODevice::ReadOnly)); QCOMPARE(old.readAll(), QByteArray("replacement"));
        QVERIFY(old.permissions() & QFileDevice::ExeOwner);
        const auto link = directory.filePath("link.AppImage"); QVERIFY(QFile::link(destination, link));
        QVERIFY(!installAppImage(link, download, digest).isEmpty());
    }
};
QTEST_GUILESS_MAIN(UpdateTests)
#include "tst_updates.moc"
