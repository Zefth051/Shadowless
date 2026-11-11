#include "../../app/update_service.h"
#include <QCryptographicHash>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>
using namespace Shadowless;

static QJsonArray fixture() { return QJsonDocument::fromJson("[\n          {\"tag_name\":\"v9.0.0\",\"assets\":[]},\n          {\"tag_name\":\"linux-v0.2.5\",\"draft\":false,\"assets\":[\n            {\"name\":\"Shadowless-0.2.5-linux-x86_64.AppImage\",\"size\":100,\n             \"browser_download_url\":\"https://github.com/Zefth051/Shadowless/releases/download/linux-v0.2.5/Shadowless-0.2.5-linux-x86_64.AppImage\",\n             \"digest\":\"sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"}]}\n        ]").array();
}

class UpdateTests : public QObject {
    Q_OBJECT
private slots:
    void selectsLinuxOnly() {
        auto releases = fixture();
        auto result = newestLinuxRelease(releases, "0.2.4");
        QVERIFY(result.has_value());
        QCOMPARE(result->version, "0.2.5");
        QVERIFY(!newestLinuxRelease(releases, "0.2.5"));
        for (const auto &bad : {"https://github.com/attacker/Shadowless/releases/download/linux-v0.2.5/file", "http://github.com/Zefth051/Shadowless/releases/download/linux-v0.2.5/file", "https://user@github.com/Zefth051/Shadowless/releases/download/linux-v0.2.5/file"}) {
            auto release = releases[1].toObject(); auto assets = release["assets"].toArray();
            auto asset = assets[0].toObject(); asset["browser_download_url"] = bad;
            assets[0] = asset; release["assets"] = assets;
            QVERIFY(!newestLinuxRelease(QJsonArray{release}, "0.2.4"));
        }
        auto draft = releases[1].toObject(); draft["draft"] = true;
        QVERIFY(!newestLinuxRelease(QJsonArray{draft}, "0.2.4"));
        auto noDigest = releases[1].toObject(); auto assets = noDigest["assets"].toArray();
        auto asset = assets[0].toObject(); asset.remove("digest"); assets[0] = asset; noDigest["assets"] = assets;
        QVERIFY(!newestLinuxRelease(QJsonArray{noDigest}, "0.2.4"));
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
