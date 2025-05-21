#ifndef NETWORKCLIENT_H
#define NETWORKCLIENT_H

#include <QObject>
#include <QUdpSocket>
#include <QVector>

class NetworkClient : public QObject
{
    Q_OBJECT
public:
    explicit NetworkClient(QObject *parent = nullptr);
    ~NetworkClient();

    void connectToServer(const QString &address, quint16 port);
    void disconnect();
    void login(const QString &username);
    void createGame();
    void joinGame();
    void sendReadyWithBoard(const QVector<QVector<int>> &board);
    void sendShot(int x, int y);
    void sendShotResult(int x, int y, bool hit);
    void sendShipSunk(int x, int y);
    void sendChatMessage(const QString &message);
    void gameOver(bool youWin);
    bool isYourTurn() const;

signals:
    void connected();
    void disconnected();
    void error(const QString &error);
    void loginResponse(bool success);
    void gameFound(const QString &opponent);
    void waitingForOpponent();
    void shotResult(int x, int y, bool hit);
    void gameOverSignal(bool youWin);
    void chatMessageReceived(const QString &sender, const QString &message);
    void shotReceived(int x, int y);
    void turnChanged(bool yourTurn);
    void gameStartConfirmed();
    void lobbyCreated(const QString &lobbyId);
    void shipSunk(int x, int y);


private slots:
    void onReadyRead();
    void onError(QAbstractSocket::SocketError socketError);

private:
    void sendJson(const QJsonObject &jsonObject);
    void processMessage(const QJsonObject &jsonObject);

    QUdpSocket *m_socket;
    bool m_isYourTurn;
    QString m_username;
    QHostAddress m_serverAddress;
    quint16 m_serverPort;
};

#endif // NETWORKCLIENT_H
