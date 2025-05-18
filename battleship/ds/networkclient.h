#ifndef NETWORKCLIENT_H
#define NETWORKCLIENT_H

#include <QObject>
#include <QUdpSocket>
#include <QTimer>
#include <QString>
#include <QVector>
#include <QHostAddress>
#include <QJsonObject>

class NetworkClient : public QObject {
    Q_OBJECT
public:
    explicit NetworkClient(QObject *parent = nullptr);
    
    void connectToServer(const QString &host, quint16 port);
    void login(const QString &username);
    void findGame();
    void sendShot(int x, int y);
    void sendReady();
    void sendBoard(const QVector<QVector<int>> &board);
    void sendChatMessage(const QString &message);
    void sendShotResult(int x, int y, bool hit);
    void sendShipSunkMessage(int x, int y);
    QString username() const { return m_username; }

signals:
    void connected();
    void disconnected();
    void error(const QString &error);
    void loginResponse(bool success);
    void gameFound(const QString &opponent);
    void waitingForOpponent();
    void shotResult(int x, int y, bool hit);
    void gameOver(const QString &winner);
    void chatMessageReceived(const QString &sender, const QString &message);
    void opponentReady();
    void shotReceived(int x, int y);
    void turnChanged(bool yourTurn);
    void gameStartConfirmed();

private slots:
    void onReadyRead();
    void onError(QAbstractSocket::SocketError socketError);
    void tryReconnect();
    void sendPing();

private:
    void sendJson(const QJsonObject &json);
    
    QUdpSocket *m_socket;
    QTimer *m_reconnectTimer;
    QTimer *m_pingTimer;
    QString m_username;
    QString m_host;
    quint16 m_port;
    QHostAddress m_serverAddress;
};

#endif // NETWORKCLIENT_H 