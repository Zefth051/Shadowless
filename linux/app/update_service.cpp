#include "update_service.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QVersionNumber>

namespace Shadowless {
static constexpr qint64 maximumAsset = 512LL * 1024 * 1024;
static bool trustedDownload(const QUrl &url) {
    return url.scheme() == "https" && url.userInfo().isEmpty() && url.port(-1) == -1
        && url.query().isEmpty() && url.fragment().isEmpty()
        && url.host() == "github.com";
}
std::optional<LinuxRelease> newestLinuxRelease(const QJsonArray &releases, const QString &current) {
    std::optional<LinuxRelease> newest;
    const QRegularExpression tagPattern("^linux-v([0-9]+\\.[0-9]+\\.[0-9]+)$");
    const QRegularExpression digestPattern("^sha256:([a-fA-F0-9]{64})$");
    for (const auto &value : releases) {
        const auto release = value.toObject();
        const auto tag = release["tag_name"].toString();
        const auto match = tagPattern.match(tag);
        if (!match.hasMatch() || release["draft"].toBool()
            || (release["prerelease"].toBool() && !current.startsWith("0."))) continue;
        const auto version = match.captured(1);
        if (QVersionNumber::fromString(version) <= QVersionNumber::fromString(current)
            || (newest && QVersionNumber::fromString(version) <= QVersionNumber::fromString(newest->version))) continue;
        const auto name = "Shadowless-" + version + "-linux-x86_64.AppImage";
        for (const auto &item : release["assets"].toArray()) {
            const auto asset = item.toObject();
            const QUrl url(asset["browser_download_url"].toString());
            const auto digest = digestPattern.match(asset["digest"].toString());
            const auto size = asset["size"].toInteger();
            if (asset["name"].toString() != name || !trustedDownload(url)
                || url.path() != "/Zefth051/Shadowless/releases/download/" + tag + "/" + name
                || !digest.hasMatch() || size <= 0 || size > maximumAsset) continue;
            newest = LinuxRelease{version, url, digest.captured(1).toLatin1().toLower(), size};
        }
    }
    return newest;
}
QString installAppImage(const QString &destination, const QString &download, const QByteArray &digest) {
    const QFileInfo original(destination);
    if (!original.isAbsolute() || !original.isFile() || original.isSymLink() || !original.isWritable())
        return "The installed AppImage must be a writable regular file, not a symbolic link.";
    QFile source(download);
    if (!source.open(QIODevice::ReadOnly) || source.size() <= 0 || source.size() > maximumAsset)
        return "Cannot read the downloaded AppImage.";
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&source) || hash.result().toHex() != digest)
        return "Update SHA-256 verification failed; the installed AppImage was preserved.";
    if (!source.seek(0)) return "Cannot read the downloaded AppImage.";
    QSaveFile target(destination);
    target.setDirectWriteFallback(false);
    if (!target.open(QIODevice::WriteOnly)) return target.errorString();
    if (!target.setPermissions(original.permissions() | QFileDevice::ExeOwner)) return target.errorString();
    while (!source.atEnd()) {
        const auto chunk = source.read(128 * 1024);
        if (chunk.isEmpty() || target.write(chunk) != chunk.size()) return "Cannot write the update; the installed AppImage was preserved.";
    }
    if (!target.commit()) return target.errorString();
    return {};
}
UpdateService::UpdateService(QObject *parent) : QObject(parent) {}
bool UpdateService::enabled() {
#ifdef SHADOWLESS_PUBLIC_UPDATES
    return true;
#else
    return false;
#endif
}
void UpdateService::check(bool interactive) {
    if (!enabled()) {
        if (interactive) emit status("In-app updates require a public release build. Download private previews through GitHub.");
        return;
    }
    if (m_reply) { if (interactive) emit status("An update request is already running."); return; }
    m_newest.reset(); page(1, interactive);
}
void UpdateService::page(int number, bool interactive) {
    QNetworkRequest request(QUrl(QString("https://api.github.com/repos/Zefth051/Shadowless/releases?per_page=100&page=%1").arg(number)));
    request.setRawHeader("User-Agent", "Shadowless/" + QCoreApplication::applicationVersion().toLatin1());
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    request.setTransferTimeout(15000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    auto *reply = m_network.get(request); m_reply = reply;
    reply->setReadBufferSize(128 * 1024);
    auto data = std::make_shared<QByteArray>();
    connect(reply, &QNetworkReply::readyRead, this, [reply, data] {
        data->append(reply->readAll()); if (data->size() > 2 * 1024 * 1024) reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, data, number, interactive] {
        m_reply.clear(); reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError || reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
            if (interactive) emit status("Cannot check GitHub releases. Check your connection and try again.");
            return;
        }
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(*data, &error);
        if (error.error != QJsonParseError::NoError || !document.isArray()) {
            if (interactive) emit status("GitHub returned an invalid release list.");
            return;
        }
        const auto candidate = newestLinuxRelease(document.array(), m_newest ? m_newest->version : QCoreApplication::applicationVersion());
        if (candidate) m_newest = candidate;
        if (reply->rawHeader("Link").contains("rel=\"next\"")) {
            if (number < 20) { page(number + 1, interactive); return; }
            if (interactive) emit status("Too many release pages; open GitHub Releases to check manually.");
            return;
        }
        if (m_newest) emit available(*m_newest);
        else if (interactive) emit status("Shadowless is up to date.");
    });
}
void UpdateService::download(const LinuxRelease &release) {
    if (!enabled() || m_reply) return;
    // Validate the caller's selection against the same repository policy as discovery.
    const auto name = "Shadowless-" + release.version + "-linux-x86_64.AppImage";
    if (!trustedDownload(release.url) || release.url.path() != "/Zefth051/Shadowless/releases/download/linux-v" + release.version + "/" + name
        || release.size <= 0 || release.size > maximumAsset) { emit status("Invalid update selection."); return; }
    const auto destination = qEnvironmentVariable("APPIMAGE");
    if (destination.isEmpty()) { emit status("Automatic installation requires running the AppImage from its permanent writable location."); return; }
    auto file = std::make_shared<QTemporaryFile>();
    if (!file->open()) { emit status("Cannot create a temporary update download."); return; }
    QNetworkRequest request(release.url);
    request.setTransferTimeout(60000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::UserVerifiedRedirectPolicy);
    auto *reply = m_network.get(request); m_reply = reply;
    reply->setReadBufferSize(128 * 1024);
    connect(reply, &QNetworkReply::redirected, this, [reply](const QUrl &url) {
        if (url.scheme() == "https" && url.userInfo().isEmpty() && url.port(-1) == -1
            && (url.host() == "github.com" || url.host() == "release-assets.githubusercontent.com" || url.host() == "objects.githubusercontent.com")) reply->redirectAllowed();
        else reply->abort();
    });
    connect(reply, &QNetworkReply::readyRead, this, [reply, file, release] {
        const auto data = reply->readAll();
        if (file->size() + data.size() > release.size || file->write(data) != data.size()) reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, file, release, destination] {
        m_reply.clear(); reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError || file->size() != release.size || !file->flush()) {
            emit status("Update download failed; the installed AppImage was preserved."); return;
        }
        const auto error = installAppImage(destination, file->fileName(), release.sha256);
        emit status(error.isEmpty() ? "Update installed. Quit and reopen Shadowless when recording is finished to use the new version." : error);
    });
    emit status("Downloading the update. Recording can continue.");
}
}
