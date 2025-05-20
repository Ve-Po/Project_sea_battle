// gameserver.h
#pragma once

#include <QObject>
#include <QUdpSocket>
#include <QNetworkDatagram> // Добавлено для QNetworkDatagram
#include <QMap>
#include <QTimer>
#include <QJsonArray>
#include <QJsonObject>
#include <QDateTime>

struct ClientInfo {
    QString id;
    QString username;
    QHostAddress address;
    quint16 port;
    qint64 lastActive;
    QString lobbyId;
    bool isConnected;
    QJsonArray savedBoard;
    
    ClientInfo() : port(0), lastActive(0), isConnected(true) {} // Конструктор по умолчанию
};

struct Lobby {
    QString id; 
    QString player1;
    QString player2;
    bool player1Ready;
    bool player2Ready;
    QJsonArray player1Board;
    QJsonArray player2Board;
    bool isActive = false;      // Статус игры
    bool player1Turn = false;   // Чей ход
    qint64 lastActivity= 0;
    
    Lobby() : player1Ready(false), player2Ready(false), lastActivity(0) {} // Конструктор по умолчанию
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
    void onGameTimeout();
    void onSessionTimeout();
    void onPingTimerTimeout();

private:
    QMap<QString, ClientInfo> m_clients;
    QMap<QString, Lobby> m_lobbies;
    QMap<QString, QString> m_clientAddressToId;

    QUdpSocket *m_socket;
    QTimer *m_gameTimer;
    QTimer *m_sessionTimer;
    QTimer *m_pingTimer;

    const int GAME_TIMEOUT_MS = 120000;
    const int SESSION_TIMEOUT_S = 60;
    const int PING_INTERVAL_MS = 5000;

    void handleLogin(const QJsonObject &json, const QString &clientId);
    void handleReady(const QJsonObject &json, const QString &clientId);
    void handleShot(const QJsonObject &json, const QString &clientId);
    void handlePing(const QJsonObject &json, const QString &clientId);
    void handleReconnect(const QJsonObject &json, const QString &clientId);

    QString getClientId(const QHostAddress &address, quint16 port);
    QString generateLobbyId() const;
    bool validateBoard(const QJsonArray &board);
    void cleanupLobby(const QString &lobbyId);
    void startGame(const QString &lobbyId);
    void sendJson(const QJsonObject &json, const QString &clientId);
    void sendError(const QString &message, const QString &clientId);
    void handleBoard(const QJsonObject &json, const QString &clientId);
    bool validateClient(const QString &clientId);
    void onError(QAbstractSocket::SocketError socketError);
};