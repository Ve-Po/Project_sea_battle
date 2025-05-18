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

struct Player {
    QString username;
    QHostAddress address;
    quint16 port;
    bool isReady;
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
    void onPingTimeout(Player* player);
    void onGameTimeout(Game* game);

private:
    QUdpSocket* m_socket;
    QMap<QString, Player*> m_players;
    QMap<QString, QString> m_gamePairs;
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
    void checkGameOver(Game* game);
    bool checkBoard(const QVector<QVector<int>>& board);
    void removeGame(Game* game);
};

#endif // GAMESERVER_H 