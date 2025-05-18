#ifndef GAMESERVER_H
#define GAMESERVER_H

#include <QObject>
#include <QUdpSocket>
#include <QMap>
#include <QTimer>
#include <QHostAddress>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>
#include <QQueue>
#include <QVector>
#include <QPoint>

struct Player {
    QString username;
    QHostAddress address;
    quint16 port;
    bool isReady;
    bool boardReceived;
    QVector<QVector<int>> board;
    QTimer* pingTimer;
    int missedPings;
};

class GameServer : public QObject {
    Q_OBJECT
public:
    explicit GameServer(QObject *parent = nullptr);
    bool start(quint16 port = 12345);
    void stop();

private slots:
    void onReadyRead();
    void onGameTimeout();

private:
    static const int PING_INTERVAL = 5000;  // 5 секунд
    static const int MAX_MISSED_PINGS = 3;
    static const int GAME_TIMEOUT = 300000;  // 5 минут
    static const int BOARD_SIZE = 10;        // Размер игрового поля

    QUdpSocket* m_socket;
    QMap<QString, Player*> m_players;
    QMap<QString, QString> m_gamePairs;
    QQueue<QString> m_waitingQueue;
    QTimer* m_gameTimer;
    
    void handlePing(const QHostAddress& sender, quint16 senderPort, const QJsonObject& json);
    void handleLogin(const QHostAddress& sender, quint16 senderPort, const QJsonObject& json);
    void handleFindGame(const QHostAddress& sender, quint16 senderPort, const QJsonObject& json);
    void handleBoard(const QHostAddress& sender, quint16 senderPort, const QJsonObject& json);
    void handleShot(const QHostAddress& sender, quint16 senderPort, const QJsonObject& json);
    void handleReady(const QHostAddress& sender, quint16 senderPort, const QJsonObject& json);
    void handleChat(const QHostAddress& sender, quint16 senderPort, const QJsonObject& json);
    void sendJson(const QHostAddress& address, quint16 port, const QJsonObject& json);
    void startGame(const QString& player1, const QString& player2);
    void checkGameTimeout();
    bool checkBoard(const QVector<QVector<int>>& board);
    void removePlayer(const QString& username);
    void processWaitingQueue();
    void tryStartGame(const QString& username);
    bool checkShipSunk(const QVector<QVector<int>>& board, int x, int y);
    bool areAllShipsSunk(const QVector<QVector<int>>& board);
    void handleGameOver(const QString& winner, const QString& loser);
};

#endif // GAMESERVER_H 