#ifndef NETWORKCLIENT_H
#define NETWORKCLIENT_H

#include <QObject>
#include <QUdpSocket>
#include <QTimer>
#include <QVector>
#include <QJsonObject>
#include <QNetworkDatagram>

class NetworkClient : public QObject
{
    Q_OBJECT

public:
    explicit NetworkClient(QObject *parent = nullptr);

    // Основные методы подключения
    void connectToServer(const QString &host, quint16 port);
    void disconnectFromServer();
    bool isConnected() const;
    void reconnect();  // Новый метод для переподключения

    // Методы авторизации
    void login(const QString &username);
    QString username() const;

    // Методы игрового процесса
    void createGame();
    void findGame();
    void joinGame(const QString &gameId = QString());
    void sendReady();
    void sendReadyWithBoard(const QVector<QVector<int>> &board);  // Новый метод
    void sendShot(int x, int y);
    void sendBoard(const QVector<QVector<int>> &board);
    void sendShotResult(int x, int y, bool hit);
    void sendShipSunk(int x, int y);
    void sendChatMessage(const QString &message);

    // Состояние игры
    bool isYourTurn() const;
    QString gameId() const;
    QString opponent() const;
    QVector<QVector<int>> getBoard() const;  // Новый метод для получения текущей доски

signals:
    // Сигналы состояния соединения
    void connected();
    void disconnected();
    void error(const QString &message);
    void reconnecting();  // Новый сигнал

    // Сигналы авторизации
    void loginResponse(bool success);

    // Сигналы игровых событий
    void gameCreated(const QString &gameId);
    void gameFound(const QString &opponent, const QString &gameId);
    void waitingForOpponent();
    void gameStart(bool yourTurn);
    void gameStarted(bool yourTurn, const QString &opponent);  // Новый сигнал
    void gameStartConfirmed();
    void turnChanged(bool yourTurn);
    void shotReceived(int x, int y);
    void shotResult(int x, int y, bool hit);
    void gameOver(bool youWin);
    void opponentDisconnected();
    void opponentReady(const QString &username);  // Новый сигнал
    void gameStateReceived(const QJsonObject &state);  // Новый сигнал
    void gameStateChanged(const QJsonObject &state);   // Новый сигнал

    // Сигналы чата
    void chatMessageReceived(const QString &sender, const QString &message);

private slots:
    void onReadyRead();
    void onError(QAbstractSocket::SocketError socketError);
    void tryReconnect();
    void sendPing();
    void handleGameState(const QJsonObject &json);  // Новый слот
    void handleGameStateChanged(const QJsonObject &json);  // Новый слот

private:
    // Вспомогательные методы
    bool validateConnection();
    void sendJson(const QJsonObject &json);
    void handleReconnectResponse(const QJsonObject &json);  // Новый метод
    void updateGameState(const QJsonObject &state);  // Новый метод

    // Компоненты
    QUdpSocket *m_socket;
    QTimer *m_reconnectTimer;
    QTimer *m_pingTimer;

    // Состояние
    QHostAddress m_serverAddress;
    QString m_host;
    quint16 m_port;
    QString m_username;
    QString m_gameId;
    QString m_opponent;
    bool m_isConnected;
    bool m_isYourTurn;
    QVector<QVector<int>> m_board;  // Новое поле для хранения доски
    int m_reconnectAttempts;  // Новое поле для отслеживания попыток переподключения
};

#endif // NETWORKCLIENT_H