#pragma once
#include <QLocalSocket>
#include <QTimer>
#include <QStringList>
#include <QHash>
#include <QNetworkAccessManager>

namespace Shadowless {
class DiscordPresenceTests;
class DiscordPresence : public QObject {
public:
    enum State { Idle, InstantClip, Recording, Paused, Unknown };
    explicit DiscordPresence(QObject *parent = nullptr);
    ~DiscordPresence() override;
    void start();
    void stop();
    void setState(State state, const QString &game = {});
private:
    friend class DiscordPresenceTests;
    QLocalSocket m_socket;
    QTimer m_retry, m_deadline, m_update;
    QStringList m_paths;
    int m_path = 0;
    QByteArray m_input;
    QString m_nonce;
    State m_desired = Idle, m_sent = Idle;
    QString m_game, m_sentGame, m_sentIcon;
    QNetworkAccessManager m_network;
    QHash<QString, QString> m_gameIcons;
    bool m_iconsRequested = false;
    static QHash<QString, QString> gameIconsFromJson(const QByteArray &json);
    void requestGameIcons();
    bool m_started = false, m_ready = false, m_hasSent = false;
    static QByteArray frame(quint32 opcode, const QByteArray &payload);
    static QString stateText(State state);
    void connectNext();
    void handshake();
    void disconnected();
    void read();
    void publish();
    bool write(quint32 opcode, const QByteArray &payload);
};
}
