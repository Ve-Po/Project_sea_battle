#include "gameserver.h"
#include <QUuid>
#include <QRandomGenerator>
#include <QJsonDocument>
#include <QDebug>

GameServer::GameServer(QObject *parent) : QObject(parent),
    m_socket(new QUdpSocket(this)),
    m_gameTimer(new QTimer(this)),
    m_sessionTimer(new QTimer(this)),
    m_pingTimer(new QTimer(this))
{
    connect(m_socket, &QUdpSocket::readyRead, this, &GameServer::onReadyRead);
    connect(m_socket, &QUdpSocket::errorOccurred, this, &GameServer::onError);
    
    m_gameTimer->setInterval(GAME_TIMEOUT_MS);
    m_sessionTimer->setInterval(SESSION_TIMEOUT_S * 1000);
    m_pingTimer->setInterval(PING_INTERVAL_MS);
    
    connect(m_gameTimer, &QTimer::timeout, this, &GameServer::onGameTimeout);
    connect(m_sessionTimer, &QTimer::timeout, this, &GameServer::onSessionTimeout);
    connect(m_pingTimer, &QTimer::timeout, this, &GameServer::onPingTimerTimeout);
}

GameServer::~GameServer() { 
    stop(); 
}

bool GameServer::start(quint16 port) {
    if (m_socket->bind(QHostAddress::Any, port)) {
        qDebug() << "Server started on port" << port;
        m_sessionTimer->start();
        m_pingTimer->start();
        return true;
    }
    qDebug() << "Failed to start server:" << m_socket->errorString();
    return false;
}

void GameServer::stop() {
    m_socket->close();
    m_gameTimer->stop();
    m_sessionTimer->stop();
    m_pingTimer->stop();
    m_clients.clear();
    m_lobbies.clear();
    m_clientAddressToId.clear();
}

void GameServer::onReadyRead() {
    while (m_socket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_socket->receiveDatagram();
        QByteArray data = datagram.data();
        QHostAddress sender = datagram.senderAddress();
        quint16 senderPort = datagram.senderPort();
        
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(data, &error);
        if (error.error != QJsonParseError::NoError) {
            qDebug() << "JSON parse error:" << error.errorString();
            continue;
        }
        
        QJsonObject json = doc.object();
        QString type = json["type"].toString();
        QString clientId = getClientId(sender, senderPort);
        
        if (m_clients.contains(clientId)) {
            m_clients[clientId].lastActive = QDateTime::currentSecsSinceEpoch();
        }
        
        if (type == "login") handleLogin(json, clientId);
        else if (type == "ready") handleReady(json, clientId);
        else if (type == "shot") handleShot(json, clientId);
        else if (type == "ping") handlePing(json, clientId);
        else if (type == "reconnect") handleReconnect(json, clientId);
        else if (type == "board") handleBoard(json, clientId);
        else qDebug() << "Unknown message type:" << type;
    }
}

void GameServer::onError(QAbstractSocket::SocketError socketError) {
    qDebug() << "Socket error occurred:" << m_socket->errorString();
}

void GameServer::onGameTimeout() {
    qint64 currentTime = QDateTime::currentSecsSinceEpoch();
    for (auto it = m_lobbies.begin(); it != m_lobbies.end(); ) {
        if (currentTime - it->lastActivity > GAME_TIMEOUT_MS / 1000) {
            cleanupLobby(it.key());
            it = m_lobbies.erase(it);
        } else {
            ++it;
        }
    }
}

void GameServer::onSessionTimeout() {
    qint64 currentTime = QDateTime::currentSecsSinceEpoch();
    for (auto it = m_clients.begin(); it != m_clients.end(); ) {
        if (currentTime - it->lastActive > SESSION_TIMEOUT_S) {
            QString addrKey = it->address.toString() + ":" + QString::number(it->port);
            m_clientAddressToId.remove(addrKey);
            it = m_clients.erase(it);
        } else {
            ++it;
        }
    }
}

void GameServer::onPingTimerTimeout() {
    QJsonObject pingMsg;
    pingMsg["type"] = "ping";
    for (const auto& client : m_clients) {
        sendJson(pingMsg, client.id);
    }
}

void GameServer::handleLogin(const QJsonObject &json, const QString &clientId) {
    QString username = json["username"].toString();
    if (username.isEmpty()) {
        sendError("Username cannot be empty", clientId);
        return;
    }
    
    m_clients[clientId].username = username;
    
    QJsonObject response;
    response["type"] = "login_response";
    response["success"] = true;
    sendJson(response, clientId);
}

void GameServer::handleReady(const QJsonObject &json, const QString &clientId) 
{
     qDebug() << "ready" ;
    if (!validateClient(clientId)) {
          qDebug() << "not ok with client" ;
        return;
    }


    // Получаем сохраненную доску из handleBoard
    QJsonArray board = m_clients[clientId].savedBoard;
    if (board.isEmpty()) {
        sendError("Сначала отправьте расстановку кораблей", clientId);
          qDebug() << "not rasstn korabl" ;
        return;
    }

    // Проверяем валидность доски
    if (!validateBoard(board)) {
        sendError("Некорректная расстановка кораблей", clientId);
          qDebug() << "not good rasst korab" ;
        return;
    }

    // Далее оригинальная логика handleReady...
    QString foundLobbyId;
    for (auto &lobby : m_lobbies) {
        if (lobby.player2.isEmpty() && lobby.player1 != clientId) {
            foundLobbyId = lobby.id;
            break;
        }
    }

    if (foundLobbyId.isEmpty()) {
          qDebug() << "pusto" ;
        // Создаем новое лобби
        Lobby newLobby;
        newLobby.id = generateLobbyId();
        newLobby.player1 = clientId;
        newLobby.player1Board = board;  // Используем сохраненную доску
        newLobby.player1Ready = true;
        newLobby.lastActivity = QDateTime::currentSecsSinceEpoch();
        m_lobbies[newLobby.id] = newLobby;
        m_clients[clientId].lobbyId = newLobby.id;

        QJsonObject response;
        response["type"] = "lobby_created";
        response["lobby_id"] = newLobby.id;
        sendJson(response, clientId);
    } else {
        // Присоединяемся к существующему лобби
        Lobby &lobby = m_lobbies[foundLobbyId];
        lobby.player2 = clientId;
        lobby.player2Board = board;  // Используем сохраненную доску
        lobby.player2Ready = true;
        lobby.lastActivity = QDateTime::currentSecsSinceEpoch();
        m_clients[clientId].lobbyId = foundLobbyId;

        // Уведомляем игроков
        QJsonObject startMsg;
        startMsg["type"] = "game_start";
        startMsg["opponent"] = m_clients[lobby.player1].username;
        startMsg["your_turn"] = false;
        sendJson(startMsg, lobby.player2);

        startMsg["opponent"] = m_clients[lobby.player2].username;
        startMsg["your_turn"] = true;
        sendJson(startMsg, lobby.player1);

        startGame(foundLobbyId);
    }
}
void GameServer::handleBoard(const QJsonObject &json, const QString &clientId)
{
    qDebug() << "board" ;
    if (!validateClient(clientId)) {
        sendError("Клиент не авторизован", clientId);
        return;
    }

    if (!json.contains("board")) {
        sendError("Не получена расстановка кораблей", clientId);
        return;
    }

    QJsonArray board = json["board"].toArray();
    if (!validateBoard(board)) {
        sendError("Некорректная расстановка кораблей", clientId);
        return;
    }

    // Сохраняем доску в данных клиента
    m_clients[clientId].savedBoard = board;
    
    // Отправляем подтверждение
   // QJsonObject response;
    //response["type"] = "board_saved";
   // response["status"] = "success";
   // sendJson(response, clientId);
}

void GameServer::handleShot(const QJsonObject &json, const QString &clientId) {
    if (!validateClient(clientId)) return;
    
    QString lobbyId = m_clients[clientId].lobbyId;
    if (!m_lobbies.contains(lobbyId)) {
        sendError("You are not in a game", clientId);
        return;
    }

    Lobby &lobby = m_lobbies[lobbyId];
    QString opponentId = (clientId == lobby.player1) ? lobby.player2 : lobby.player1;
    
    QJsonObject shotMsg = json;
    shotMsg["type"] = "shot";
    sendJson(shotMsg, opponentId);
}

void GameServer::handlePing(const QJsonObject &json, const QString &clientId) {
    if (!validateClient(clientId)) return;
    m_clients[clientId].lastActive = QDateTime::currentSecsSinceEpoch();
}

void GameServer::handleReconnect(const QJsonObject &json, const QString &clientId) {
    if (!validateClient(clientId)) {
        sendError("Invalid client ID", clientId);
        return;
    }

    QString lobbyId = m_clients[clientId].lobbyId;
    if (lobbyId.isEmpty() || !m_lobbies.contains(lobbyId)) {
        sendError("Game not found", clientId);
        return;
    }

    m_clients[clientId].isConnected = true;
    m_clients[clientId].lastActive = QDateTime::currentSecsSinceEpoch();

    // Send game state to reconnected client
    Lobby &lobby = m_lobbies[lobbyId];
    QJsonObject state;
    state["type"] = "game_state";
    state["lobby_id"] = lobbyId;
    state["opponent"] = m_clients[(clientId == lobby.player1) ? lobby.player2 : lobby.player1].username;
    state["your_turn"] = (clientId == lobby.player1) ? lobby.player1Ready : lobby.player2Ready;
    sendJson(state, clientId);
}

QString GameServer::getClientId(const QHostAddress &address, quint16 port) {
    QString key = address.toString() + ":" + QString::number(port);
    if (!m_clientAddressToId.contains(key)) {
        QString newId = QUuid::createUuid().toString();
        m_clientAddressToId[key] = newId;
        
        ClientInfo client;
        client.id = newId;
        client.address = address;
        client.port = port;
        client.lastActive = QDateTime::currentSecsSinceEpoch();
        client.lobbyId = "";
        client.isConnected = true;
        
        m_clients[newId] = client;
    }
    return m_clientAddressToId[key];
}

QString GameServer::generateLobbyId() const {
    const QString chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    QString id;
    for (int i = 0; i < 6; ++i) {
        id.append(chars.at(QRandomGenerator::global()->bounded(chars.length())));
    }
    return id;
}

bool GameServer::validateBoard(const QJsonArray &board) {
    if (board.size() != 10) return false;
    int shipCells = 0;
    for (const auto &row : board) {
        if (row.toArray().size() != 10) return false;
        for (const auto &cell : row.toArray()) {
            if (cell.toInt() == 1) shipCells++;
        }
    }
    return shipCells == 20;
}

void GameServer::cleanupLobby(const QString &lobbyId) {
    if (!m_lobbies.contains(lobbyId)) return;

    Lobby &lobby = m_lobbies[lobbyId];
    QJsonObject msg;
    msg["type"] = "lobby_timeout";
    
    if (!lobby.player1.isEmpty()) {
        sendJson(msg, lobby.player1);
        m_clients[lobby.player1].lobbyId.clear();
    }
    if (!lobby.player2.isEmpty()) {
        sendJson(msg, lobby.player2);
        m_clients[lobby.player2].lobbyId.clear();
    }
}

void GameServer::startGame(const QString &lobbyId) {
    if (!m_lobbies.contains(lobbyId)) {
        qDebug() << "Лобби не найдено:" << lobbyId;
        return;
    }

    Lobby &lobby = m_lobbies[lobbyId];
    
    // Помечаем лобби как активную игру
    lobby.isActive = true;
    lobby.player1Turn = true;
    lobby.lastActivity = QDateTime::currentSecsSinceEpoch();

    // Отправляем уведомления игрокам
    QJsonObject startMsg;
    startMsg["type"] = "game_start";
    startMsg["lobby_id"] = lobbyId;  // Используем тот же ID
    
    // Игроку 1
    startMsg["your_turn"] = true;
    startMsg["opponent"] = m_clients[lobby.player2].username;
    sendJson(startMsg, lobby.player1);
    
    // Игроку 2
    startMsg["your_turn"] = false;
    startMsg["opponent"] = m_clients[lobby.player1].username;
    sendJson(startMsg, lobby.player2);

    // Запускаем таймер игры
    if (!m_gameTimer->isActive()) {
        m_gameTimer->start();
    }
    
    qDebug() << "Игра началась в лобби:" << lobbyId 
             << "Игрок 1:" << lobby.player1 
             << "Игрок 2:" << lobby.player2;
}

void GameServer::sendJson(const QJsonObject &json, const QString &clientId) {
    if (!m_clients.contains(clientId)) return;
    const ClientInfo &client = m_clients[clientId];
    m_socket->writeDatagram(QJsonDocument(json).toJson(), client.address, client.port);
}

void GameServer::sendError(const QString &message, const QString &clientId) {
    QJsonObject error;
    error["type"] = "error";
    error["message"] = message;
    sendJson(error, clientId);
}

bool GameServer::validateClient(const QString &clientId) {
    if (!m_clients.contains(clientId)) {
        sendError("Client not registered", clientId);
        return false;
    }
    return true;
}