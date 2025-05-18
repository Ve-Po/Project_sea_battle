#include "networkclient.h"
#include <QDebug>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTimer>

NetworkClient::NetworkClient(QObject *parent) : QObject(parent) {
    m_socket = new QUdpSocket(this);
    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setInterval(5000); // 5 секунд между попытками переподключения
    
    m_pingTimer = new QTimer(this);
    m_pingTimer->setInterval(1000); // Пинг каждую секунду
    
    connect(m_socket, &QUdpSocket::readyRead, this, &NetworkClient::onReadyRead);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QUdpSocket::error),
            this, &NetworkClient::onError);
    connect(m_reconnectTimer, &QTimer::timeout, this, &NetworkClient::tryReconnect);
    connect(m_pingTimer, &QTimer::timeout, this, &NetworkClient::sendPing);
}

void NetworkClient::connectToServer(const QString &host, quint16 port) {
    qDebug() << "Подключение к серверу:" << host << ":" << port;
    m_host = host;
    m_port = port;
    m_serverAddress = QHostAddress(host);
    
    if (m_socket->state() == QAbstractSocket::BoundState) {
        m_socket->close();
    }
    
    if (m_socket->bind(QHostAddress::Any, 0, QUdpSocket::DefaultForPlatform)) {  // Используем случайный порт
        qDebug() << "UDP сокет успешно привязан на порту:" << m_socket->localPort();
        m_pingTimer->start();
        emit connected();
    } else {
        qDebug() << "Не удалось привязать UDP сокет";
        m_reconnectTimer->start();
    }
}

void NetworkClient::tryReconnect() {
    if (m_socket->state() != QAbstractSocket::BoundState) {
        qDebug() << "Попытка переподключения к серверу:" << m_host << ":" << m_port;
        if (m_socket->bind()) {
            m_pingTimer->start();
            emit connected();
        }
    } else {
        m_reconnectTimer->stop();
    }
}

void NetworkClient::onError(QAbstractSocket::SocketError socketError) {
    qDebug() << "Ошибка сокета:" << m_socket->errorString();
    emit error(m_socket->errorString());
    
    if (socketError != QAbstractSocket::RemoteHostClosedError) {
        m_reconnectTimer->start();
    }
}

void NetworkClient::sendPing() {
    QJsonObject json;
    json["type"] = "ping";
    json["username"] = m_username;
    sendJson(json);
}

void NetworkClient::login(const QString &username) {
    if (m_socket->state() != QAbstractSocket::BoundState) {
        qDebug() << "Нет подключения к серверу при попытке входа";
        emit error("Нет подключения к серверу");
        return;
    }
    
    qDebug() << "Отправка логина:" << username;
    m_username = username;
    QJsonObject json;
    json["type"] = "login";
    json["username"] = username;
    sendJson(json);
}

void NetworkClient::findGame() {
    if (m_socket->state() != QAbstractSocket::BoundState) {
        qDebug() << "Нет подключения к серверу при поиске игры";
        emit error("Нет подключения к серверу");
        return;
    }
    
    qDebug() << "Поиск игры";
    QJsonObject json;
    json["type"] = "find_game";
    json["username"] = m_username;
    sendJson(json);
}

void NetworkClient::sendShot(int x, int y) {
    if (m_socket->state() != QAbstractSocket::BoundState) {
        qDebug() << "Нет подключения к серверу при отправке выстрела";
        emit error("Нет подключения к серверу");
        return;
    }
    
    qDebug() << "Отправка выстрела:" << x << y;
    QJsonObject json;
    json["type"] = "shot";
    json["username"] = m_username;
    json["x"] = x;
    json["y"] = y;
    sendJson(json);
}

void NetworkClient::sendReady() {
    qDebug() << "sendReady: начало метода";
    if (m_socket->state() != QAbstractSocket::BoundState) {
        qDebug() << "sendReady: нет подключения к серверу";
        emit error("Нет подключения к серверу");
        return;
    }
    
    qDebug() << "sendReady: отправка сообщения о готовности";
    QJsonObject json;
    json["type"] = "ready";
    json["username"] = m_username;
    sendJson(json);
    qDebug() << "sendReady: сообщение отправлено";
}

void NetworkClient::sendBoard(const QVector<QVector<int>> &board) {
    if (m_socket->state() != QAbstractSocket::BoundState) {
        qDebug() << "Нет подключения к серверу при отправке доски";
        emit error("Нет подключения к серверу");
        return;
    }
    
    qDebug() << "Отправка доски";
    QJsonObject request;
    request["type"] = "board";
    request["username"] = m_username;
    
    QJsonArray boardArray;
    for (const auto &row : board) {
        QJsonArray rowArray;
        for (int cell : row) {
            rowArray.append(cell);
        }
        boardArray.append(rowArray);
    }
    
    request["board"] = boardArray;
    sendJson(request);
}

void NetworkClient::sendChatMessage(const QString &message) {
    if (m_socket->state() != QAbstractSocket::BoundState) {
        qDebug() << "Нет подключения к серверу при отправке сообщения";
        emit error("Нет подключения к серверу");
        return;
    }
    
    qDebug() << "Отправка сообщения в чат:" << message;
    QJsonObject json;
    json["type"] = "chat";
    json["username"] = m_username;
    json["message"] = message;
    sendJson(json);
}

void NetworkClient::sendShotResult(int x, int y, bool hit) {
    if (m_socket->state() != QAbstractSocket::BoundState) {
        qDebug() << "Нет подключения к серверу при отправке результата выстрела";
        emit error("Нет подключения к серверу");
        return;
    }
    QJsonObject response;
    response["type"] = "shot_result";
    response["x"] = x;
    response["y"] = y;
    response["hit"] = hit;
    sendJson(response);
}

void NetworkClient::onReadyRead() {
    QByteArray datagram;
    datagram.resize(m_socket->pendingDatagramSize());
    QHostAddress sender;
    quint16 senderPort;

    m_socket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);
    
    QJsonDocument doc = QJsonDocument::fromJson(datagram);
    if (doc.isNull()) {
        qDebug() << "Ошибка при разборе JSON";
        return;
    }
    
    QJsonObject response = doc.object();
    QString type = response["type"].toString();
    qDebug() << "Получено сообщение типа:" << type;
    
    if (type == "game_found") {
        QString opponent = response["opponent"].toString();
        qDebug() << "game_found: получено сообщение game_found";
        qDebug() << "game_found: opponent=" << opponent;
        emit gameFound(opponent);
        emit waitingForOpponent();
    }
    else if (type == "game_start") {
        QString opponent = response["opponent"].toString();
        bool waitingForReady = response["waiting_for_ready"].toBool();
        qDebug() << "game_start: opponent=" << opponent << "waitingForReady=" << waitingForReady;
        emit gameFound(opponent);
        if (waitingForReady) {
            sendReady();
            emit waitingForOpponent();
        } else {
            emit gameStartConfirmed();
        }
    }
    else if (type == "turn_change") {
        bool yourTurn = response["your_turn"].toBool();
        qDebug() << "turn_change: получено сообщение turn_change";
        qDebug() << "turn_change: yourTurn=" << yourTurn;
        qDebug() << "turn_change: отправляем сигнал turnChanged с параметром" << yourTurn;
        emit turnChanged(yourTurn);
        qDebug() << "turn_change: сигнал отправлен";
    }
    else if (type == "pong") {
        // Игнорируем ответы на пинг
        return;
    }
    else if (type == "login_response") {
        bool success = response["success"].toBool();
        qDebug() << "Ответ на логин:" << success;
        emit loginResponse(success);
    }
    else if (type == "waiting") {
        qDebug() << "Ожидание противника";
        emit waitingForOpponent();
        // Не повторяем поиск игры автоматически
    }
    else if (type == "error") {
        QString message = response["message"].toString();
        qDebug() << "Получена ошибка:" << message;
        emit error(message);
    }
    else if (type == "shot") {
        int x = response["x"].toInt();
        int y = response["y"].toInt();
        qDebug() << "Получен выстрел от противника:" << x << y;
        emit shotReceived(x, y);
    }
    else if (type == "shot_result") {
        int x = response["x"].toInt();
        int y = response["y"].toInt();
        bool hit = response["hit"].toBool();
        qDebug() << "Результат выстрела:" << x << y << hit;
        emit shotResult(x, y, hit);
    }
    else if (type == "game_over") {
        QString winner = response["winner"].toString();
        qDebug() << "Игра окончена, победитель:" << winner;
        emit gameOver(winner);
    }
    else if (type == "chat") {
        QString sender = response["sender"].toString();
        QString message = response["message"].toString();
        qDebug() << "Получено сообщение от" << sender << ":" << message;
        emit chatMessageReceived(sender, message);
    }
    else if (type == "ready") {
        QString opponent = response["opponent"].toString();
        qDebug() << "Противник готов:" << opponent;
        emit opponentReady();
    }
    else {
        qDebug() << "Неизвестный тип сообщения:" << type;
    }
}

void NetworkClient::sendJson(const QJsonObject &json) {
    if (m_socket->state() != QAbstractSocket::BoundState) {
        qDebug() << "Нет подключения к серверу";
        emit error("Нет подключения к серверу");
        return;
    }
    
    QJsonDocument doc(json);
    QByteArray data = doc.toJson();
    qDebug() << "Отправка данных:" << data;
    m_socket->writeDatagram(data, m_serverAddress, m_port);
}

void NetworkClient::sendShipSunkMessage(int x, int y) {
    QJsonObject json;
    json["type"] = "ship_sunk";
    json["x"] = x;
    json["y"] = y;
    sendJson(json);
} 