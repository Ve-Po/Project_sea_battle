#ifndef GAMESERVER_H
#define GAMESERVER_H

#include <QObject>
#include <QUdpSocket>
#include <QTimer>
#include <QMap>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkDatagram>


struct ClientInfo {
    QString id;
    QHostAddress address;
    quint16 port;
    qint64 lastActive;
    QString username;
    QString lobbyId;
    bool isConnected;
    QJsonArray savedBoard;
};

struct Lobby {
    QString id;
    QString player1;
    QString player2;
    QJsonArray player1Board;
    QJsonArray player2Board;
    bool player1Ready;
    bool player2Ready;
    bool isActive;
    bool player1Turn;
    qint64 lastActivity;
};

class GameServer : public QObject
{
    Q_OBJECT
public:
    explicit GameServer(QObject *parent = nullptr);
    ~GameServer();

    bool start(quint16 port);
    void stop();

private slots:
    void onReadyRead();
    void onError(QAbstractSocket::SocketError socketError);
    void onGameTimeout();
    void onSessionTimeout();
    void onPingTimerTimeout();

private:
    // Основные функции
    void handleLogin(const QJsonObject &json, const QString &clientId);
    void handleReady(const QJsonObject &json, const QString &clientId);
    void handleShot(const QJsonObject &json, const QString &clientId);
    void handlePing(const QJsonObject &json, const QString &clientId);
    void handleReconnect(const QJsonObject &json, const QString &clientId);
    void handleBoard(const QJsonObject &json, const QString &clientId);
    void handleChatMessage(const QJsonObject &json, const QString &clientId);
    
    // Вспомогательные функции
    QString getClientId(const QHostAddress &address, quint16 port);
    QString generateLobbyId() const;
    bool validateBoard(const QJsonArray &board);
    void cleanupLobby(const QString &lobbyId);
    void startGame(const QString &lobbyId);
    void processShotResult(const QString &lobbyId, const QString &shooterId, int x, int y);
    bool checkWinCondition(const QJsonArray &board);
    void endGame(const QString &lobbyId, const QString &winnerId);
    void sendJson(const QJsonObject &json, const QString &clientId);
    void sendError(const QString &message, const QString &clientId);
    bool validateClient(const QString &clientId);
    bool checkShipSunk(const QJsonArray &board, int x, int y);

    bool checkGameOver(const QJsonArray &board);

    // Константы
    static constexpr int GAME_TIMEOUT_MS = 1800000; // 30 минут
    static constexpr int SESSION_TIMEOUT_S = 300;   // 5 минут
    static constexpr int PING_INTERVAL_MS = 10000;  // 10 секунд

    // Члены класса
    QUdpSocket *m_socket;
    QTimer *m_gameTimer;
    QTimer *m_sessionTimer;
    QTimer *m_pingTimer;
    QMap<QString, ClientInfo> m_clients;
    QMap<QString, Lobby> m_lobbies;
    QMap<QString, QString> m_clientAddressToId;
};

#endif // GAMESERVER_H