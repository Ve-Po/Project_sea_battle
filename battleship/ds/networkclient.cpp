#include "networkclient.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

NetworkClient::NetworkClient(QObject *parent) : QObject(parent), // Инициализация клиента
    m_socket(new QUdpSocket(this)),
    m_reconnectTimer(new QTimer(this)),
    m_pingTimer(new QTimer(this)),
    m_isConnected(false),
    m_isYourTurn(false)
{
    // Настройка интервалов таймеров
    m_reconnectTimer->setInterval(5000);    // 5 секунд между попытками переподключения
    m_pingTimer->setInterval(10000);       // Пинг каждые 10 секунд
    
    // Соединение сигналов и слотов
    connect(m_socket, &QUdpSocket::readyRead, this, &NetworkClient::onReadyRead);
    connect(m_socket, &QUdpSocket::errorOccurred, this, &NetworkClient::onError);
    connect(m_reconnectTimer, &QTimer::timeout, this, &NetworkClient::tryReconnect);
    connect(m_pingTimer, &QTimer::timeout, this, &NetworkClient::sendPing);
}

void NetworkClient::connectToServer(const QString &host, quint16 port) //Подключение к серверу
{
    qDebug() << "Подключение к серверу:" << host << ":" << port;
    
    // Сохраняем параметры подключения
    m_host = host;
    m_port = port;
    
    // Пытаемся преобразовать адрес
    if (!m_serverAddress.setAddress(host)) {
        qDebug() << "Неверный адрес сервера";
        emit error("Неверный адрес сервера");
        return;
    }
    
    // Закрываем сокет, если он уже был привязан
    if (m_socket->state() == QAbstractSocket::BoundState) {
        m_socket->close();
    }
    
    // Пытаемся привязать сокет к случайному порту
    if (m_socket->bind(QHostAddress::Any, 0, QUdpSocket::DefaultForPlatform)) {
        qDebug() << "UDP сокет привязан к порту:" << m_socket->localPort();
        m_isConnected = true;
        m_pingTimer->start();
        emit connected();
    } else {
        qDebug() << "Ошибка привязки сокета:" << m_socket->errorString();
        m_reconnectTimer->start();
        emit error("Ошибка подключения к серверу");
    }
}

void NetworkClient::disconnectFromServer()
{
    // Отправляем сообщение о выходе, если подключены
    if (m_isConnected && !m_username.isEmpty()) {
        QJsonObject json;
        json["type"] = "disconnect";
        json["username"] = m_username;
        sendJson(json);
    }
    
    // Останавливаем все соединения и таймеры
    m_socket->close();
    m_pingTimer->stop();
    m_reconnectTimer->stop();
    m_isConnected = false;
    
    emit disconnected();
}

void NetworkClient::tryReconnect()
{
    if (!m_isConnected) {
        qDebug() << "Попытка переподключения...";
        connectToServer(m_host, m_port);
    }
}


void NetworkClient::findGame() {
    if (!validateConnection()) return;
    
    qDebug() << "Поиск доступной игры";
    QJsonObject json;
    json["type"] = "ready";
    json["username"] = m_username;
    sendJson(json);
}

void NetworkClient::login(const QString &username) //Логин ---> сервер обрабатывает в handleLogin
{
    if (!m_isConnected) {
        emit error("Нет подключения к серверу");
        return;
    }
    
    if (username.isEmpty()) {
        emit error("Имя пользователя не может быть пустым");
        return;
    }
    
    qDebug() << "Авторизация пользователя:" << username;
    m_username = username;
    
    QJsonObject json;
    json["type"] = "login";
    json["username"] = username;
    sendJson(json);
}

void NetworkClient::createGame() //создание новой игры -----> server: handleNewGame
{
    if (!validateConnection()) return;
    
    qDebug() << "Создание новой игры";
    QJsonObject json;
    json["type"] = "new_game";
    json["username"] = m_username;
    sendJson(json);
}

void NetworkClient::joinGame(const QString &gameId) // присоединение к игре -----> server handljoingame
{
    if (!validateConnection()) return;
    
    qDebug() << "Присоединение к игре:" << gameId;
    QJsonObject json;
    json["type"] = "join_game";
    json["username"] = m_username;
    
    if (!gameId.isEmpty()) {
        json["game_id"] = gameId;
        m_gameId = gameId;
    }
    
    sendJson(json);
}

void NetworkClient::sendReady()
{
    if (!validateConnection()) return;
    
    qDebug() << "Подтверждение готовности";
    QJsonObject json;
    json["type"] = "ready";
    json["username"] = m_username;
    sendJson(json);
}

void NetworkClient::sendShot(int x, int y)
{
    if (!validateConnection()) return;
    if (!m_isYourTurn) {
        emit error("Сейчас не ваш ход");
        return;
    }
    
    qDebug() << "Отправка выстрела в (" << x << "," << y << ")";
    QJsonObject json;
    json["type"] = "shot";
    json["username"] = m_username;
    json["x"] = x;
    json["y"] = y;
    sendJson(json);
}

void NetworkClient::sendBoard(const QVector<QVector<int>> &board)
{
    if (!validateConnection()) return;
    
    qDebug() << "Отправка расстановки кораблей";
    QJsonObject json;
    json["type"] = "board";
    json["username"] = m_username;
    
    QJsonArray boardArray;
    for (const auto &row : board) {
        QJsonArray rowArray;
        for (int cell : row) {
            rowArray.append(cell);
        }
        boardArray.append(rowArray);
    }
    
    json["board"] = boardArray;
    sendJson(json);
}

void NetworkClient::sendShotResult(int x, int y, bool hit)
{
    if (!validateConnection()) return;
    
    qDebug() << "Отправка результата выстрела:" << x << y << hit;
    QJsonObject json;
    json["type"] = "shot_result";
    json["username"] = m_username;
    json["x"] = x;
    json["y"] = y;
    json["hit"] = hit;
    sendJson(json);
}

void NetworkClient::sendShipSunk(int x, int y)
{
    if (!validateConnection()) return;
    
    qDebug() << "Отправка сообщения о потоплении корабля:" << x << y;
    QJsonObject json;
    json["type"] = "ship_sunk";
    json["username"] = m_username;
    json["x"] = x;
    json["y"] = y;
    sendJson(json);
}

void NetworkClient::sendChatMessage(const QString &message)
{
    if (!validateConnection()) return;
    
    qDebug() << "Отправка сообщения в чат:" << message;
    QJsonObject json;
    json["type"] = "chat";
    json["username"] = m_username;
    json["message"] = message;
    sendJson(json);
}

void NetworkClient::sendPing()
{
    if (!m_isConnected) return;
    
    QJsonObject json;
    json["type"] = "ping";
    json["username"] = m_username;
    sendJson(json);
}

void NetworkClient::onReadyRead()
{
    while (m_socket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_socket->receiveDatagram();
        QByteArray data = datagram.data();
        
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            emit error("Ошибка разбора JSON: " + parseError.errorString());
            continue;
        }

        QJsonObject json = doc.object();
        QString type = json["type"].toString();

        if (type == "error") {
            emit error(json["message"].toString());
        } else if (type == "login_response") {
            emit loginResponse(json["success"].toBool());
        } else if (type == "game_created") {
            m_gameId = json["game_id"].toString();
            emit gameCreated(m_gameId);
        } else if (type == "game_found") {
            m_opponent = json["opponent"].toString();
            m_gameId = json["gameId"].toString();
            emit gameFound(m_opponent, m_gameId);
        } else if (type == "waiting_for_opponent") {
            emit waitingForOpponent();
        } else if (type == "game_stazzzzrt") {
            m_isYourTurn = json["yourTurn"].toBool();
            m_opponent = json["opponent"].toString();
            emit gameStarted(m_isYourTurn, m_opponent);
        } else if (type == "game_start") {
            emit gameStartConfirmed();
        } else if (type == "turn_changed") {
            m_isYourTurn = json["yourTurn"].toBool();
            emit turnChanged(m_isYourTurn);
        } else if (type == "shot") {
            emit shotReceived(json["x"].toInt(), json["y"].toInt());
        } else if (type == "shot_result") {
            emit shotResult(json["x"].toInt(), json["y"].toInt(), json["hit"].toBool());
        } else if (type == "game_over") {
            emit gameOver(json["youWin"].toBool());
        } else if (type == "opponent_disconnected") {
            emit opponentDisconnected();
        } else if (type == "chat") {
            emit chatMessageReceived(json["sender"].toString(), json["message"].toString());
        } else if (type == "game_state") {
            handleGameState(json);
        } else if (type == "game_state_changed") {
            handleGameStateChanged(json);
        } else if (type == "player_ready") {
            emit opponentReady(json["username"].toString());
        } else if (type == "reconnect_response") {
            handleReconnectResponse(json);
        }
    }
}

void NetworkClient::onError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);
    qDebug() << "Ошибка сокета:" << m_socket->errorString();
    m_isConnected = false;
    emit error(m_socket->errorString());
    m_reconnectTimer->start();
}

bool NetworkClient::validateConnection()
{
    if (!m_isConnected) {
        emit error("Нет подключения к серверу");
        return false;
    }
    return true;
}

void NetworkClient::sendJson(const QJsonObject &json)
{
    if (!m_isConnected) {
        emit error("Нет подключения к серверу");
        return;
    }
    
    QJsonDocument doc(json);
    QByteArray data = doc.toJson(QJsonDocument::Compact);
    
    qint64 bytesSent = m_socket->writeDatagram(data, m_serverAddress, m_port);
    if (bytesSent == -1) {
        qDebug() << "Ошибка отправки данных:" << m_socket->errorString();
        emit error("Ошибка отправки данных");
    }
}

void NetworkClient::reconnect()
{
    if (m_isConnected) {
        return;
    }

    m_reconnectAttempts++;
    emit reconnecting();

    QJsonObject json;
    json["type"] = "reconnect";
    json["username"] = m_username;
    json["gameId"] = m_gameId;
    sendJson(json);
}

void NetworkClient::handleReconnectResponse(const QJsonObject &json)
{
    if (json["success"].toBool()) {
        m_isConnected = true;
        m_reconnectAttempts = 0;
        emit connected();
        
        // Обновляем состояние игры
        if (json.contains("gameState")) {
            updateGameState(json["gameState"].toObject());
        }
    } else {
        emit error(json["message"].toString());
    }
}

void NetworkClient::handleGameState(const QJsonObject &json)
{
    updateGameState(json);
    emit gameStateReceived(json);
}

void NetworkClient::handleGameStateChanged(const QJsonObject &json)
{
    updateGameState(json);
    emit gameStateChanged(json);
}

void NetworkClient::updateGameState(const QJsonObject &state)
{
    if (state.contains("isYourTurn")) {
        m_isYourTurn = state["isYourTurn"].toBool();
        emit turnChanged(m_isYourTurn);
    }

    if (state.contains("yourBoard")) {
        QJsonArray boardArray = state["yourBoard"].toArray();
        m_board.clear();
        m_board.resize(10);
        for (int i = 0; i < 10; i++) {
            m_board[i].resize(10);
            QJsonArray row = boardArray[i].toArray();
            for (int j = 0; j < 10; j++) {
                m_board[i][j] = row[j].toInt();
            }
        }
    }

    if (state.contains("lastShotX") && state.contains("lastShotY")) {
        int x = state["lastShotX"].toInt();
        int y = state["lastShotY"].toInt();
        bool hit = state["lastShotHit"].toBool();
        emit shotResult(x, y, hit);
    }
}

QVector<QVector<int>> NetworkClient::getBoard() const
{
    return m_board;
}


void NetworkClient::sendReadyWithBoard(const QVector<QVector<int>> &board) { //отправляем готовность с доской ---> server handleready
    if (!validateConnection()) return;
    
    QJsonObject json;
    json["type"] = "ready";
    json["username"] = m_username;
    
    // Конвертируем доску в JSON
    QJsonArray boardArray;
    for (const auto &row : board) {
        QJsonArray rowArray;
        for (int cell : row) {
            rowArray.append(cell);
        }
        boardArray.append(rowArray);
    }
    json["board"] = boardArray;
    
    sendJson(json);
}