#ifndef GAMESERVER_H
#define GAMESERVER_H

#include <QObject>
#include <QUdpSocket>
#include <QTimer>
#include <QNetworkDatagram>
#include <QMap>
#include <QPair>
#include <QJsonObject>
#include <QJsonArray>

class GameServer : public QObject
{
    Q_OBJECT

public:
    explicit GameServer(QObject *parent = nullptr);
    ~GameServer();

    bool start(quint16 port);
    void stop();

private:
    struct ClientInfo {
        QString id;
        QString username;
        QString gameId;
        QHostAddress address;
        quint16 port;
        qint64 lastActive;
        bool isConnected = true;
    };

struct Lobby {
        QString id;
        QString player1;
        QString player2;
        bool player1Ready = false;
        bool player2Ready = false;
        QJsonArray player1Board;
        QJsonArray player2Board;
    };
    QMap<QString, Lobby> m_lobbies; 

    QUdpSocket *m_socket;
    QTimer *m_gameTimer;
    QTimer *m_sessionTimer;
    QTimer *m_pingTimer;

    QMap<QString, ClientInfo> m_clients;
 
    QMap<QString, QString> m_clientAddressToId;

    static const int GAME_TIMEOUT_MS = 300000;    // 5 минут
    static const int SESSION_TIMEOUT_S = 60;      // 1 минута
    static const int PING_INTERVAL_MS = 30000;    // 30 секунд

private slots:
    void onReadyRead();
    void onError(QAbstractSocket::SocketError socketError);
    void onGameTimeout();
    void onSessionTimeout();
    void onPingTimerTimeout();

private:

    void notifyPlayers(const QString &lobbyId);
    void handleLogin(const QJsonObject &json, const QString &clientId);
    void handleNewGame(const QJsonObject &json, const QString &clientId);
    void handleJoinGame(const QJsonObject &json, const QString &clientId);
    void handleReady(const QJsonObject &json, const QString &clientId);
    void handleShot(const QJsonObject &json, const QString &clientId);
    void handleShotResult(const QJsonObject &json, const QString &clientId);
    void handleBoard(const QJsonObject &json, const QString &clientId);
    void handleChatMessage(const QJsonObject &json, const QString &clientId);
    void handlePing(const QJsonObject &json, const QString &clientId);
    void handleReconnect(const QJsonObject &json, const QString &clientId);
    bool validateGameState(const QString &gameId);
    void syncGameState(const QString &gameId);
    void handleDisconnect(const QString &clientId);
    bool validateBoard(const QJsonArray &board);
    void notifyGameStateChange(const QString &gameId);
    void cleanupGame(const QString &gameId);
    QString getOpponentId(const QString &gameId, const QString &clientId);
    void sendGameState(const QString &gameId, const QString &clientId);

    QString getClientId(const QHostAddress &address, quint16 port);
    QString generateGameId() const;
    void sendJson(const QJsonObject &json, const QString &clientId);
    void sendError(const QString &message, const QString &clientId);
    bool validateClient(const QString &clientId);
    bool validateGame(const QString &gameId, const QString &clientId);
    void startGame(const QString &gameId);
};

#endif // GAMESERVER_H
