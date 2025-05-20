#include "NetworkClient.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

NetworkClient::NetworkClient(QObject *parent) : QObject(parent),
    m_socket(new QUdpSocket(this)),
    m_isYourTurn(false)
{
    connect(m_socket, &QUdpSocket::readyRead, this, &NetworkClient::onReadyRead);
    connect(m_socket, &QUdpSocket::errorOccurred, this, &NetworkClient::onError);
}

NetworkClient::~NetworkClient()
{
    if (m_socket->state() == QAbstractSocket::BoundState) {
        m_socket->close();
    }
    delete m_socket;
}

void NetworkClient::connectToServer(const QString &address, quint16 port)
{
    m_serverAddress = QHostAddress(address);
    m_serverPort = port;
    
    if (m_socket->state() == QAbstractSocket::BoundState) {
        m_socket->close();
    }
    
    if (m_socket->bind(0)) {
        qDebug() << "Socket bound successfully on port" << m_socket->localPort();
        emit connected();
    } else {
        QString errorMsg = "Failed to bind socket: " + m_socket->errorString();
        qDebug() << errorMsg;
        emit error(errorMsg);
    }
}

void NetworkClient::disconnect()
{
    m_socket->close();
    emit disconnected();
}

void NetworkClient::login(const QString &username)
{
    m_username = username;
    QJsonObject loginMsg;
    loginMsg["type"] = "login";
    loginMsg["username"] = username;
    qDebug() << "Sending login request for user:" << username;
    sendJson(loginMsg);
}

void NetworkClient::createGame()
{
    QJsonObject msg;
    msg["type"] = "create_game";
    qDebug() << "Sending create game request";
    sendJson(msg);
}

void NetworkClient::joinGame()
{
    QJsonObject msg;
    msg["type"] = "join_game";
    qDebug() << "Sending join game request";
    sendJson(msg);
}

void NetworkClient::sendReadyWithBoard(const QVector<QVector<int>> &board)
{
    // Сначала отправляем доску
    QJsonObject boardMsg;
    boardMsg["type"] = "board";
    
    QJsonArray boardArray;
    for (const auto &row : board) {
        QJsonArray jsonRow;
        for (int cell : row) {
            jsonRow.append(cell);
        }
        boardArray.append(jsonRow);
    }
    boardMsg["board"] = boardArray;
    
    qDebug() << "Sending board";
    sendJson(boardMsg);
    
    // Затем отправляем сигнал готовности
    QJsonObject readyMsg;
    readyMsg["type"] = "ready";
    
    qDebug() << "Sending ready message";
    sendJson(readyMsg);
}

void NetworkClient::sendShot(int x, int y)
{
    QJsonObject msg;
    msg["type"] = "shot";
    msg["x"] = x;
    msg["y"] = y;
    qDebug() << "Sending shot at (" << x << "," << y << ")";
    sendJson(msg);
}

void NetworkClient::sendShotResult(int x, int y, bool hit)
{
    QJsonObject msg;
    msg["type"] = "shot_result";
    msg["x"] = x;
    msg["y"] = y;
    msg["hit"] = hit;
    qDebug() << "Sending shot result at (" << x << "," << y << "):" << (hit ? "hit" : "miss");
    sendJson(msg);
}

void NetworkClient::sendShipSunk(int x, int y)
{
    QJsonObject msg;
    msg["type"] = "ship_sunk";
    msg["x"] = x;
    msg["y"] = y;
    qDebug() << "Sending ship sunk at (" << x << "," << y << ")";
    sendJson(msg);
}

void NetworkClient::sendChatMessage(const QString &message)
{
    QJsonObject msg;
    msg["type"] = "chat_message";
    msg["message"] = message;
    msg["sender"] = m_username;
    qDebug() << "Sending chat message:" << message;
    sendJson(msg);
}

void NetworkClient::gameOver(bool youWin)
{
    QJsonObject msg;
    msg["type"] = "game_over";
    msg["result"] = youWin ? "win" : "loss";
    qDebug() << "Sending game over message:" << (youWin ? "win" : "loss");
    sendJson(msg);
}

bool NetworkClient::isYourTurn() const
{
    return m_isYourTurn;
}

void NetworkClient::onReadyRead()
{
    while (m_socket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(m_socket->pendingDatagramSize());
        QHostAddress sender;
        quint16 senderPort;

        m_socket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);

        QJsonDocument doc = QJsonDocument::fromJson(datagram);
        if (doc.isObject()) {
            processMessage(doc.object());
        }
    }
}

void NetworkClient::onError(QAbstractSocket::SocketError socketError)
{
    QString errorString = m_socket->errorString();
    qDebug() << "Socket error:" << errorString;
    emit error(errorString);
}

void NetworkClient::sendJson(const QJsonObject &jsonObject)
{
    if (m_socket->state() != QAbstractSocket::BoundState) {
        qDebug() << "Socket is not bound, cannot send data";
        emit error("Socket is not bound");
        return;
    }

    QJsonDocument doc(jsonObject);
    QByteArray data = doc.toJson();
    qint64 bytesWritten = m_socket->writeDatagram(data, m_serverAddress, m_serverPort);
    
    if (bytesWritten == -1) {
        QString errorMsg = "Failed to send data: " + m_socket->errorString();
        qDebug() << errorMsg;
        emit error(errorMsg);
    } else {
        qDebug() << "Sent" << bytesWritten << "bytes to" << m_serverAddress.toString() << ":" << m_serverPort;
    }
}

void NetworkClient::processMessage(const QJsonObject &jsonObject)
{
    QString type = jsonObject["type"].toString();
    qDebug() << "Received message of type:" << type;
    
    if (type == "login_response") {
        bool success = jsonObject["success"].toBool();
        qDebug() << "Login response:" << (success ? "success" : "failed");
        emit loginResponse(success);
    }
    else if (type == "lobby_created") {
        QString lobbyId = jsonObject["lobby_id"].toString();
        qDebug() << "Lobby created with ID:" << lobbyId;
        emit lobbyCreated(lobbyId);
    }
    else if (type == "game_found") {
        QString opponent = jsonObject["opponent"].toString();
        qDebug() << "Game found with opponent:" << opponent;
        emit gameFound(opponent);
    }
    else if (type == "waiting_for_opponent") {
        qDebug() << "Waiting for opponent to join";
        emit waitingForOpponent();
    }
    else if (type == "shot_result") {
        int x = jsonObject["x"].toInt();
        int y = jsonObject["y"].toInt();
        bool hit = jsonObject["hit"].toBool();
        qDebug() << "Shot result at (" << x << "," << y << "):" << (hit ? "hit" : "miss");
        emit shotResult(x, y, hit);
    }
    else if (type == "game_over") {
        bool youWin = jsonObject["result"].toString() == "win";
        qDebug() << "Game over:" << (youWin ? "you won" : "you lost");
        emit gameOverSignal(youWin);
    }
    else if (type == "chat_message") {
        QString sender = jsonObject["sender"].toString();
        QString message = jsonObject["message"].toString();
        qDebug() << "Chat message received from" << sender << ":" << message;
        qDebug() << "Full chat message:" << jsonObject;
        emit chatMessageReceived(sender, message);
    }
    else if (type == "shot_received") {
        int x = jsonObject["x"].toInt();
        int y = jsonObject["y"].toInt();
        qDebug() << "Received shot at (" << x << "," << y << ")";
        emit shotReceived(x, y);
    }
    else if (type == "turn_change") {
        m_isYourTurn = jsonObject["your_turn"].toBool();
        qDebug() << "Turn changed:" << (m_isYourTurn ? "your turn" : "opponent's turn");
        emit turnChanged(m_isYourTurn);
    }
    else if (type == "game_start") {
        qDebug() << "Game started!";
        // При старте игры первый ход должен быть у создателя лобби
        m_isYourTurn = jsonObject["your_turn"].toBool();
        qDebug() << "Initial turn state:" << (m_isYourTurn ? "your turn" : "opponent's turn");
        emit turnChanged(m_isYourTurn);
        emit gameStartConfirmed();
    }
    else if (type == "ping") {
        return;
    }
    else if (type == "ship_sunk") {
        int x = jsonObject["x"].toInt();
        int y = jsonObject["y"].toInt();
        qDebug() << "Ship sunk at (" << x << "," << y << ")";
        emit shipSunk(x, y);
    }
    else {
        qDebug() << "Unknown message type:" << type;
    }
}
