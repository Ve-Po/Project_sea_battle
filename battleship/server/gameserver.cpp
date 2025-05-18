#include "gameserver.h"

GameServer::GameServer(QObject *parent) : QObject(parent) {
    m_socket = new QUdpSocket(this);
    m_gameTimer = new QTimer(this);
    m_gameTimer->setInterval(300000); // 5 минут на игру
    connect(m_gameTimer, &QTimer::timeout, this, &GameServer::onGameTimeout);
    connect(m_socket, &QUdpSocket::readyRead, this, &GameServer::onReadyRead);
}

bool GameServer::start(quint16 port) {
    if (!m_socket->bind(QHostAddress::Any, port)) {
        qDebug() << "Не удалось запустить сервер на порту" << port;
        return false;
    }
    qDebug() << "Сервер запущен на порту" << port;
    return true;
}

void GameServer::stop() {
    m_socket->close();
    qDebug() << "Сервер остановлен";
}

void GameServer::onReadyRead() {
    while (m_socket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(m_socket->pendingDatagramSize());
        QHostAddress sender;
        quint16 senderPort;

        m_socket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);
        
        qDebug() << "Получены данные:" << datagram;
        
        QJsonDocument doc = QJsonDocument::fromJson(datagram);
        if (doc.isNull()) {
            qDebug() << "Ошибка при разборе JSON";
            continue;
        }
        
        QJsonObject json = doc.object();
        QString type = json["type"].toString();
        qDebug() << "Тип сообщения:" << type;
        
        if (type == "ping") {
            handlePing(sender, senderPort, json);
        }
        else if (type == "login") {
            handleLogin(sender, senderPort, json);
        }
        else if (type == "find_game") {
            handleFindGame(sender, senderPort, json);
        }
        else if (type == "board") {
            handleBoard(sender, senderPort, json);
        }
        else if (type == "shot") {
            handleShot(sender, senderPort, json);
        }
        else if (type == "ready") {
            handleReady(sender, senderPort, json);
        }
        else if (type == "chat") {
            handleChat(sender, senderPort, json);
        }
        else {
            qDebug() << "Неизвестный тип сообщения:" << type;
        }
    }
}

void GameServer::handlePing(const QHostAddress& sender, quint16 senderPort, const QJsonObject& json) {
    QString username = json["username"].toString();
    if (m_players.contains(username)) {
        Player* player = m_players[username];
        player->address = sender;
        player->port = senderPort;
        player->missedPings = 0;
        
        // Отправляем pong
        QJsonObject response;
        response["type"] = "pong";
        sendJson(response, sender, senderPort);
    }
}

void GameServer::handleLogin(const QHostAddress& sender, quint16 senderPort, const QJsonObject& json) {
    QString username = json["username"].toString();
    
    if (m_players.contains(username)) {
        QJsonObject response;
        response["type"] = "login_response";
        response["success"] = false;
        response["message"] = "Имя пользователя уже занято";
        sendJson(response, sender, senderPort);
        return;
    }

    Player* player = new Player;
    player->username = username;
    player->address = sender;
    player->port = senderPort;
    player->isReady = false;
    player->missedPings = 0;
    
    player->pingTimer = new QTimer(this);
    player->pingTimer->setInterval(5000); // Проверка каждые 5 секунд
    connect(player->pingTimer, &QTimer::timeout, this, [this, player]() {
        onPingTimeout(player);
    });
    player->pingTimer->start();

    m_players[username] = player;

    QJsonObject response;
    response["type"] = "login_response";
    response["success"] = true;
    sendJson(response, sender, senderPort);
}

void GameServer::handleFindGame(const QHostAddress& sender, quint16 senderPort, const QJsonObject& json) {
    QString username = json["username"].toString();
    if (!m_players.contains(username)) {
        return;
    }

    Player* player = m_players[username];
    player->address = sender;
    player->port = senderPort;

    // Ищем свободного игрока
    QString opponent;
    for (auto it = m_players.begin(); it != m_players.end(); ++it) {
        if (it.key() != username && !m_gamePairs.contains(it.key())) {
            opponent = it.key();
            break;
        }
    }

    if (opponent.isEmpty()) {
        QJsonObject response;
        response["type"] = "waiting";
        sendJson(response, sender, senderPort);
        return;
    }

    startGame(username, opponent);
}

void GameServer::handleBoard(const QHostAddress& sender, quint16 senderPort, const QJsonObject& json) {
    QString username = json["username"].toString();
    if (!m_players.contains(username)) {
        return;
    }

    Player* player = m_players[username];
    player->address = sender;
    player->port = senderPort;

    QJsonArray boardArray = json["board"].toArray();
    player->board.clear();
    for (const QJsonValue& rowValue : boardArray) {
        QVector<int> row;
        QJsonArray rowArray = rowValue.toArray();
        for (const QJsonValue& cellValue : rowArray) {
            row.append(cellValue.toInt());
        }
        player->board.append(row);
    }

    player->isReady = true;

    // Проверяем, готовы ли оба игрока
    QString opponent = m_gamePairs[username];
    if (m_players[opponent]->isReady) {
        QJsonObject response1;
        response1["type"] = "game_start";
        response1["opponent"] = opponent;
        sendJson(response1, player->address, player->port);

        QJsonObject response2;
        response2["type"] = "game_start";
        response2["opponent"] = username;
        sendJson(response2, m_players[opponent]->address, m_players[opponent]->port);

        m_gameTimer->start();
    }
}

void GameServer::handleShot(const QHostAddress& sender, quint16 senderPort, const QJsonObject& json) {
    QString username = json["username"].toString();
    if (!m_players.contains(username) || !m_gamePairs.contains(username)) {
        return;
    }

    Player* player = m_players[username];
    player->address = sender;
    player->port = senderPort;

    QString opponent = m_gamePairs[username];
    int x = json["x"].toInt();
    int y = json["y"].toInt();

    bool hit = false;
    if (x >= 0 && x < 10 && y >= 0 && y < 10) {
        hit = m_players[opponent]->board[y][x] == 1;
        if (hit) {
            m_players[opponent]->board[y][x] = 2; // Помечаем как подбитый
        }
    }

    QJsonObject response;
    response["type"] = "shot_result";
    response["x"] = x;
    response["y"] = y;
    response["hit"] = hit;
    sendJson(response, player->address, player->port);

    // Проверяем победу
    bool gameOver = true;
    for (const auto& row : m_players[opponent]->board) {
        for (int cell : row) {
            if (cell == 1) { // Если остались неподбитые корабли
                gameOver = false;
                break;
            }
        }
        if (!gameOver) break;
    }

    if (gameOver) {
        QJsonObject gameOver1;
        gameOver1["type"] = "game_over";
        gameOver1["winner"] = username;
        sendJson(gameOver1, player->address, player->port);

        QJsonObject gameOver2;
        gameOver2["type"] = "game_over";
        gameOver2["winner"] = username;
        sendJson(gameOver2, m_players[opponent]->address, m_players[opponent]->port);

        m_gamePairs.remove(username);
        m_gamePairs.remove(opponent);
        m_gameTimer->stop();
    }
}

void GameServer::handleReady(const QHostAddress& sender, quint16 senderPort, const QJsonObject& json) {
    QString username = json["username"].toString();
    if (!m_players.contains(username)) {
        return;
    }

    Player* player = m_players[username];
    player->address = sender;
    player->port = senderPort;
    player->isReady = true;

    if (m_gamePairs.contains(username)) {
        QString opponent = m_gamePairs[username];
        if (m_players[opponent]->isReady) {
            QJsonObject response1;
            response1["type"] = "game_start";
            response1["opponent"] = opponent;
            sendJson(response1, player->address, player->port);

            QJsonObject response2;
            response2["type"] = "game_start";
            response2["opponent"] = username;
            sendJson(response2, m_players[opponent]->address, m_players[opponent]->port);

            m_gameTimer->start();
        }
    }
}

void GameServer::handleChat(const QHostAddress &sender, quint16 senderPort, const QJsonObject &json) {
    QString username = json["username"].toString();
    QString message = json["message"].toString();
    
    // Находим игрока
    auto it = std::find_if(m_players.begin(), m_players.end(),
        [&](const Player &p) { return p.username == username; });
    
    if (it != m_players.end()) {
        // Находим игру, в которой участвует игрок
        auto gameIt = std::find_if(m_games.begin(), m_games.end(),
            [&](const Game &g) { return g.player1 == username || g.player2 == username; });
        
        if (gameIt != m_games.end()) {
            // Отправляем сообщение обоим игрокам
            QString opponent = (gameIt->player1 == username) ? gameIt->player2 : gameIt->player1;
            auto opponentIt = std::find_if(m_players.begin(), m_players.end(),
                [&](const Player &p) { return p.username == opponent; });
            
            if (opponentIt != m_players.end()) {
                QJsonObject chatJson;
                chatJson["type"] = "chat";
                chatJson["username"] = username;
                chatJson["message"] = message;
                
                // Отправляем сообщение отправителю
                sendJson(sender, senderPort, chatJson);
                
                // Отправляем сообщение получателю
                sendJson(opponentIt->address, opponentIt->port, chatJson);
            }
        }
    }
}

void GameServer::onPingTimeout(Player* player) {
    player->missedPings++;
    if (player->missedPings >= 3) { // Если пропущено 3 пинга подряд
        removePlayer(player->username);
    }
}

void GameServer::onGameTimeout() {
    checkGameTimeout();
}

void GameServer::checkGameTimeout() {
    for (auto it = m_gamePairs.begin(); it != m_gamePairs.end();) {
        QString player1 = it.key();
        QString player2 = it.value();
        
        if (!m_players.contains(player1) || !m_players.contains(player2)) {
            it = m_gamePairs.erase(it);
            continue;
        }

        QJsonObject timeout1;
        timeout1["type"] = "game_over";
        timeout1["winner"] = "timeout";
        sendJson(timeout1, m_players[player1]->address, m_players[player1]->port);

        QJsonObject timeout2;
        timeout2["type"] = "game_over";
        timeout2["winner"] = "timeout";
        sendJson(timeout2, m_players[player2]->address, m_players[player2]->port);

        it = m_gamePairs.erase(it);
    }
}

void GameServer::startGame(const QString& player1, const QString& player2) {
    m_gamePairs[player1] = player2;
    m_gamePairs[player2] = player1;

    QJsonObject response1;
    response1["type"] = "game_start";
    response1["opponent"] = player2;
    sendJson(response1, m_players[player1]->address, m_players[player1]->port);

    QJsonObject response2;
    response2["type"] = "game_start";
    response2["opponent"] = player1;
    sendJson(response2, m_players[player2]->address, m_players[player2]->port);
}

void GameServer::removePlayer(const QString& username) {
    if (m_players.contains(username)) {
        Player* player = m_players[username];
        if (player->pingTimer) {
            player->pingTimer->stop();
            delete player->pingTimer;
        }
        delete player;
        m_players.remove(username);

        if (m_gamePairs.contains(username)) {
            QString opponent = m_gamePairs[username];
            m_gamePairs.remove(username);
            m_gamePairs.remove(opponent);

            if (m_players.contains(opponent)) {
                QJsonObject response;
                response["type"] = "game_over";
                response["winner"] = "disconnect";
                sendJson(response, m_players[opponent]->address, m_players[opponent]->port);
            }
        }
    }
}

void GameServer::sendJson(const QJsonObject& json, const QHostAddress& address, quint16 port) {
    QJsonDocument doc(json);
    QByteArray data = doc.toJson();
    m_socket->writeDatagram(data, address, port);
} 