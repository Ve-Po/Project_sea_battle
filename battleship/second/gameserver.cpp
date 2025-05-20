#include "gameserver.h"
#include <QUuid>
#include <QRandomGenerator>
#include <QJsonDocument>
#include <QDebug>
#include <QPoint>

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
        
        qDebug() << "Received datagram from" << sender.toString() << ":" << senderPort;
        
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(data, &error);
        if (error.error != QJsonParseError::NoError) {
            qDebug() << "JSON parse error:" << error.errorString();
            continue;
        }
        
        QJsonObject json = doc.object();
        QString type = json["type"].toString();
        QString clientId = getClientId(sender, senderPort);
        
        qDebug() << "Processing message of type:" << type << "from client:" << clientId;
        
        if (m_clients.contains(clientId)) {
            m_clients[clientId].lastActive = QDateTime::currentSecsSinceEpoch();
        }
        
        if (type == "login") handleLogin(json, clientId);
        else if (type == "ready") handleReady(json, clientId);
        else if (type == "shot") handleShot(json, clientId);
        else if (type == "ping") handlePing(json, clientId);
        else if (type == "reconnect") handleReconnect(json, clientId);
        else if (type == "board") handleBoard(json, clientId);
        else if (type == "chat_message") handleChatMessage(json, clientId);
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
    qDebug() << "Login attempt from client" << clientId << "with username:" << username;
    
    if (username.isEmpty()) {
        qDebug() << "Login failed: empty username";
        sendError("Username cannot be empty", clientId);
        return;
    }
    
    m_clients[clientId].username = username;
    
    QJsonObject response;
    response["type"] = "login_response";
    response["success"] = true;
    qDebug() << "Login successful for client" << clientId;
    sendJson(response, clientId);
}

void GameServer::handleReady(const QJsonObject &json, const QString &clientId) {
    qDebug() << "[DEBUG] handleReady: Ready request from client" << clientId;
    if (!validateClient(clientId)) {
        qDebug() << "[DEBUG] handleReady: Ready failed: invalid client" << clientId;
        return;
    }
    QJsonArray board = m_clients[clientId].savedBoard;
    if (board.isEmpty()) {
        qDebug() << "[DEBUG] handleReady: Ready failed: no board saved for client" << clientId;
        sendError("Сначала отправьте расстановку кораблей", clientId);
        return;
    }
    if (!validateBoard(board)) {
        qDebug() << "[DEBUG] handleReady: Ready failed: invalid board for client" << clientId;
        sendError("Некорректная расстановка кораблей", clientId);
        return;
    }

    QString foundLobbyId;
    for (auto &lobby : m_lobbies) {
        if (lobby.player2.isEmpty() && lobby.player1 != clientId) {
            foundLobbyId = lobby.id;
            break;
        }
    }

    if (foundLobbyId.isEmpty()) {
        qDebug() << "Creating new lobby for client" << clientId;
        Lobby newLobby;
        newLobby.id = generateLobbyId();
        newLobby.player1 = clientId;
        newLobby.player1Board = board;
        newLobby.player1Ready = true;
        newLobby.lastActivity = QDateTime::currentSecsSinceEpoch();
        m_lobbies[newLobby.id] = newLobby;
        m_clients[clientId].lobbyId = newLobby.id;

        QJsonObject response;
        response["type"] = "lobby_created";
        response["lobby_id"] = newLobby.id;
        sendJson(response, clientId);
    } else {
        qDebug() << "Joining existing lobby" << foundLobbyId << "for client" << clientId;
        Lobby &lobby = m_lobbies[foundLobbyId];
        lobby.player2 = clientId;
        lobby.player2Board = board;
        lobby.player2Ready = true;
        lobby.lastActivity = QDateTime::currentSecsSinceEpoch();
        m_clients[clientId].lobbyId = foundLobbyId;

        QJsonObject startMsg;
        startMsg["type"] = "game_start";
        startMsg["opponent"] = m_clients[lobby.player1].username;
        startMsg["your_turn"] = false;
        sendJson(startMsg, lobby.player2);

        startMsg["opponent"] = m_clients[lobby.player2].username;
        startMsg["your_turn"] = true;
        sendJson(startMsg, lobby.player1);

        qDebug() << "Starting game in lobby" << foundLobbyId;
        startGame(foundLobbyId);
    }
}

void GameServer::handleBoard(const QJsonObject &json, const QString &clientId) {
    qDebug() << "[DEBUG] handleBoard: Board received from client" << clientId;
    if (!validateClient(clientId)) {
        qDebug() << "[DEBUG] handleBoard: Board rejected: invalid client" << clientId;
        sendError("Клиент не авторизован", clientId);
        return;
    }
    if (!json.contains("board")) {
        qDebug() << "[DEBUG] handleBoard: Board rejected: no board data";
        sendError("Не получена расстановка кораблей", clientId);
        return;
    }
    QJsonArray board = json["board"].toArray();
    for (int y = 0; y < board.size(); ++y) {
        QJsonArray row = board[y].toArray();
        for (int x = 0; x < row.size(); ++x) {
            int val = row[x].toInt();
            if (val != 0 && val != 1) {
                qDebug() << "[DEBUG] handleBoard: Board rejected: contains non-ship/non-empty cell at" << x << y << "val=" << val;
                sendError("Доска должна содержать только корабли и пустые клетки", clientId);
                return;
            }
        }
    }
    if (!validateBoard(board)) {
        qDebug() << "[DEBUG] handleBoard: Board rejected: invalid board layout";
        sendError("Некорректная расстановка кораблей", clientId);
        return;
    }
    m_clients[clientId].savedBoard = board;
    qDebug() << "[DEBUG] handleBoard: Board saved for client" << clientId;
}

void GameServer::handleShot(const QJsonObject &json, const QString &clientId) {
    qDebug() << "[DEBUG] handleShot: Shot received from client" << clientId;
    if (!validateClient(clientId)) {
        qDebug() << "[DEBUG] handleShot: Shot rejected: invalid client" << clientId;
        return;
    }
    QString lobbyId = m_clients[clientId].lobbyId;
    if (!m_lobbies.contains(lobbyId)) {
        qDebug() << "[DEBUG] handleShot: Shot rejected: client not in game" << clientId;
        sendError("You are not in a game", clientId);
        return;
    }
    Lobby &lobby = m_lobbies[lobbyId];
    if ((clientId == lobby.player1 && !lobby.player1Turn) ||
        (clientId == lobby.player2 && lobby.player1Turn)) {
        qDebug() << "[DEBUG] handleShot: Shot rejected: not client's turn" << clientId;
        sendError("Not your turn", clientId);
        return;
    }
    int x = json["x"].toInt();
    int y = json["y"].toInt();
    if (x < 0 || x >= 10 || y < 0 || y >= 10) {
        qDebug() << "[DEBUG] handleShot: Shot rejected: invalid coordinates" << x << y;
        sendError("Invalid coordinates", clientId);
        return;
    }
    qDebug() << "[DEBUG] handleShot: Processing shot at" << x << y << "from client" << clientId;
    processShotResult(lobbyId, clientId, x, y);
}

void GameServer::handlePing(const QJsonObject &json, const QString &clientId) {
    qDebug() << "Ping received from client" << clientId;
    
    if (!validateClient(clientId)) {
        qDebug() << "Ping rejected: invalid client" << clientId;
        return;
    }
    
    m_clients[clientId].lastActive = QDateTime::currentSecsSinceEpoch();
    
    QJsonObject pong;
    pong["type"] = "pong";
    sendJson(pong, clientId);
    qDebug() << "Pong sent to client" << clientId;
}

void GameServer::handleReconnect(const QJsonObject &json, const QString &clientId) {
    qDebug() << "Reconnect attempt from client" << clientId;
    
    if (!validateClient(clientId)) {
        qDebug() << "Reconnect rejected: invalid client" << clientId;
        return;
    }

    QString oldClientId = json["old_client_id"].toString();
    if (!m_clients.contains(oldClientId)) {
        qDebug() << "Reconnect rejected: old client not found" << oldClientId;
        sendError("Invalid old client ID", clientId);
        return;
    }

    ClientInfo &oldClient = m_clients[oldClientId];
    ClientInfo &newClient = m_clients[clientId];
    
    newClient.username = oldClient.username;
    newClient.lobbyId = oldClient.lobbyId;
    newClient.savedBoard = oldClient.savedBoard;
    
    m_clients.remove(oldClientId);
    
    QJsonObject response;
    response["type"] = "reconnect_response";
    response["success"] = true;
    sendJson(response, clientId);
    qDebug() << "Reconnect successful for client" << clientId;
}

void GameServer::handleChatMessage(const QJsonObject &json, const QString &clientId) {
    qDebug() << "[DEBUG] handleChatMessage: from client" << clientId;
    if (!validateClient(clientId)) {
        qDebug() << "[DEBUG] handleChatMessage: invalid client" << clientId;
        return;
    }
    QString lobbyId = m_clients[clientId].lobbyId;
    if (!m_lobbies.contains(lobbyId)) {
        qDebug() << "[DEBUG] handleChatMessage: client not in lobby" << clientId;
        return;
    }
    Lobby &lobby = m_lobbies[lobbyId];
    QString otherId = (clientId == lobby.player1) ? lobby.player2 : lobby.player1;
    if (otherId.isEmpty()) {
        qDebug() << "[DEBUG] handleChatMessage: no opponent yet";
        return;
    }
    QJsonObject msg = json;
    // Пересылаем сообщение оппоненту
    sendJson(msg, otherId);
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
    
    // Проверяем количество кораблей
    QMap<int, int> shipCounts = {
        {4, 1}, // 1 корабль на 4 клетки
        {3, 2}, // 2 корабля на 3 клетки
        {2, 3}, // 3 корабля на 2 клетки
        {1, 4}  // 4 корабля на 1 клетку
    };
    
    QVector<QVector<bool>> visited(10, QVector<bool>(10, false));
    
    for (int y = 0; y < 10; ++y) {
        if (board[y].toArray().size() != 10) return false;
        
        for (int x = 0; x < 10; ++x) {
            if (board[y].toArray()[x].toInt() == 1 && !visited[y][x]) {
                int length = 1;
                bool horizontal = false;
                
                // Проверяем направление корабля
                if (x < 9 && board[y].toArray()[x+1].toInt() == 1) {
                    horizontal = true;
                    // Проверяем горизонтальный корабль
                    for (int dx = x+1; dx < 10 && board[y].toArray()[dx].toInt() == 1; ++dx) {
                        length++;
                        visited[y][dx] = true;
                    }
                } else if (y < 9 && board[y+1].toArray()[x].toInt() == 1) {
                    // Проверяем вертикальный корабль
                    for (int dy = y+1; dy < 10 && board[dy].toArray()[x].toInt() == 1; ++dy) {
                        length++;
                        visited[dy][x] = true;
                    }
                }
                
                // Проверяем диагонали (корабли не должны касаться углами)
                if (x > 0 && y > 0 && board[y-1].toArray()[x-1].toInt() == 1) return false;
                if (x < 9 && y > 0 && board[y-1].toArray()[x+1].toInt() == 1) return false;
                if (x > 0 && y < 9 && board[y+1].toArray()[x-1].toInt() == 1) return false;
                if (x < 9 && y < 9 && board[y+1].toArray()[x+1].toInt() == 1) return false;
                
                // Проверяем длину корабля
                if (!shipCounts.contains(length) || shipCounts[length] <= 0) {
                    return false;
                }
                shipCounts[length]--;
            }
            visited[y][x] = true;
        }
    }
    
    // Проверяем, что все корабли расставлены
    for (auto count : shipCounts) {
        if (count != 0) return false;
    }
    
    return true;
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
    lobby.isActive = true;
    lobby.player1Turn = true;
    lobby.lastActivity = QDateTime::currentSecsSinceEpoch();

    if (!m_gameTimer->isActive()) {
        m_gameTimer->start();
    }
}

void GameServer::processShotResult(const QString &lobbyId, const QString &shooterId, int x, int y) {
    qDebug() << "[DEBUG] processShotResult: lobby=" << lobbyId << "shooterId=" << shooterId << "x=" << x << "y=" << y;
    Lobby &lobby = m_lobbies[lobbyId];
    QString targetId = (shooterId == lobby.player1) ? lobby.player2 : lobby.player1;
    QJsonArray targetBoard = (shooterId == lobby.player1) ? lobby.player2Board : lobby.player1Board;
    bool hit = false;
    if (x >= 0 && x < 10 && y >= 0 && y < 10) {
        QJsonArray row = targetBoard[y].toArray();
        hit = row[x].toInt() == 1;
        row[x] = hit ? 2 : 3;
        qDebug() << "[DEBUG] processShotResult: cell before=" << row[x].toInt() << "after=" << (hit ? 2 : 3);
        targetBoard[y] = row;
    }
    qDebug() << "[DEBUG] processShotResult: Shot result:" << (hit ? "hit" : "miss");
    QJsonObject resultMsg;
    resultMsg["type"] = "shot_result";
    resultMsg["x"] = x;
    resultMsg["y"] = y;
    resultMsg["hit"] = hit;
    sendJson(resultMsg, shooterId);
    QJsonObject shotMsg;
    shotMsg["type"] = "shot_received";
    shotMsg["x"] = x;
    shotMsg["y"] = y;
    sendJson(shotMsg, targetId);
    if (hit) {
        bool shipSunk = checkShipSunk(targetBoard, x, y);
        if (shipSunk) {
            QJsonObject sunkMsg;
            sunkMsg["type"] = "ship_sunk";
            sunkMsg["x"] = x;
            sunkMsg["y"] = y;
            sendJson(sunkMsg, shooterId);
            sendJson(sunkMsg, targetId);
            if (checkGameOver(targetBoard)) {
                qDebug() << "[DEBUG] processShotResult: Game over in lobby" << lobbyId;
                // Победителю win, проигравшему lose
                QJsonObject winMsg, loseMsg;
                winMsg["type"] = "game_over";
                winMsg["result"] = "win";
                loseMsg["type"] = "game_over";
                loseMsg["result"] = "lose";
                sendJson(winMsg, shooterId);
                sendJson(loseMsg, targetId);
                cleanupLobby(lobbyId);
                return;
            }
        }
    } else {
        lobby.player1Turn = !lobby.player1Turn;
        QJsonObject turnMsg;
        turnMsg["type"] = "turn_change";
        turnMsg["your_turn"] = lobby.player1Turn;
        sendJson(turnMsg, lobby.player1);
        turnMsg["your_turn"] = !lobby.player1Turn;
        sendJson(turnMsg, lobby.player2);
        qDebug() << "[DEBUG] processShotResult: Turn changed to player" << (lobby.player1Turn ? "1" : "2");
    }
    if (shooterId == lobby.player1) {
        lobby.player2Board = targetBoard;
    } else {
        lobby.player1Board = targetBoard;
    }
}

bool GameServer::checkShipSunk(const QJsonArray &board, int x, int y) {
    // Найти все клетки корабля (влево, вправо, вверх, вниз)
    QVector<QPoint> shipCells;
    shipCells.append(QPoint(x, y));
    const QVector<QPair<int, int>> directions = {
        {-1, 0}, {1, 0}, {0, -1}, {0, 1}
    };
    for (const auto& dir : directions) {
        int nx = x + dir.first;
        int ny = y + dir.second;
        while (nx >= 0 && nx < 10 && ny >= 0 && ny < 10) {
            int val = board[ny].toArray()[nx].toInt();
            if (val == 1 || val == 2) {
                shipCells.append(QPoint(nx, ny));
                nx += dir.first;
                ny += dir.second;
            } else {
                break;
            }
        }
    }
    // Проверить, что все клетки корабля подбиты (2)
    for (const QPoint &p : shipCells) {
        if (board[p.y()].toArray()[p.x()].toInt() != 2) {
            return false;
        }
    }
    return true;
}

bool GameServer::checkGameOver(const QJsonArray &board) {
    // Проверяем, остались ли неповрежденные корабли
    for (int y = 0; y < 10; y++) {
        QJsonArray row = board[y].toArray();
        for (int x = 0; x < 10; x++) {
            if (row[x].toInt() == 1) {
                return false; // Нашли неповрежденный корабль
            }
        }
    }
    return true; // Все корабли уничтожены
}

void GameServer::endGame(const QString &lobbyId, const QString &winnerId) {
    Lobby &lobby = m_lobbies[lobbyId];
    QString loserId = (winnerId == lobby.player1) ? lobby.player2 : lobby.player1;
    
    QJsonObject winMsg, loseMsg;
    winMsg["type"] = "game_over";
    winMsg["result"] = "win";
    loseMsg["type"] = "game_over";
    loseMsg["result"] = "lose";
    
    sendJson(winMsg, winnerId);
    sendJson(loseMsg, loserId);
    
    m_clients[lobby.player1].lobbyId = "";
    m_clients[lobby.player2].lobbyId = "";
    m_lobbies.remove(lobbyId);
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