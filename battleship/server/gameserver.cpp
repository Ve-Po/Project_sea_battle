#include "gameserver.h"

GameServer::GameServer(QObject *parent) : QObject(parent) {
    m_socket = new QUdpSocket(this);
    m_gameTimer = new QTimer(this);
    m_gameTimer->setInterval(GAME_TIMEOUT);
    connect(m_gameTimer, &QTimer::timeout, this, &GameServer::onGameTimeout);
    connect(m_socket, &QUdpSocket::readyRead, this, &GameServer::onReadyRead);
    qDebug() << "Сервер инициализирован и готов к запуску на порту 12345.";
}

bool GameServer::start(quint16 port) {
    if (!m_socket->bind(QHostAddress::Any, port)) {
        qDebug() << "Сервер не может запуститься на порту" << port << "— порт занят.";
        return false;
    }
    quint16 actualPort = m_socket->localPort();
    qDebug() << "Сервер успешно запущен на порту" << actualPort;
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
        
        qDebug() << "[onReadyRead] Получены данные:" << datagram;
        
        QJsonDocument doc = QJsonDocument::fromJson(datagram);
        if (doc.isNull()) {
            qDebug() << "[onReadyRead] Ошибка при разборе JSON";
            continue;
        }
        
        QJsonObject json = doc.object();
        QString type = json["type"].toString();
        qDebug() << "[onReadyRead] Тип сообщения:" << type;
        
        if (type == "login") {
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
            qDebug() << "[onReadyRead] Неизвестный тип сообщения:" << type;
        }
    }
}

void GameServer::handleLogin(const QHostAddress& sender, quint16 senderPort, const QJsonObject& json) {
    QString username = json["username"].toString();
    qDebug() << "[handleLogin] Попытка входа пользователя:" << username << sender << senderPort;
    if (m_players.contains(username)) {
        qDebug() << "[handleLogin] Имя уже занято:" << username;
        QJsonObject response;
        response["type"] = "login_response";
        response["success"] = false;
        response["message"] = "Игрок уже существует";
        sendJson(sender, senderPort, response);
        return;
    }
    
    // Создаем нового игрока
    Player* player = new Player;
    player->username = username;
    player->address = sender;
    player->port = senderPort;
    player->boardReceived = false;
    player->isReady = false;
    
    m_players[username] = player;
    
    // Отправляем ответ
    QJsonObject response;
    response["type"] = "login_response";
    response["success"] = true;
    response["message"] = "Успешный вход";
    sendJson(sender, senderPort, response);
    qDebug() << "[handleLogin] Успешный вход пользователя:" << username;
    qDebug() << "КЛИЕНТ ПОДКЛЮЧИЛСЯ:" << username << sender.toString() << ":" << senderPort;
}

void GameServer::handleFindGame(const QHostAddress& sender, quint16 senderPort, const QJsonObject& json) {
    QString username = json["username"].toString();
    qDebug() << "[handleFindGame] Запрос на поиск игры от:" << username;
    if (!m_players.contains(username)) {
        qDebug() << "[handleFindGame] Игрок не найден:" << username;
        QJsonObject response;
        response["type"] = "find_game_response";
        response["success"] = false;
        response["message"] = "Игрок не найден";
        sendJson(sender, senderPort, response);
        return;
    }
    Player* player = m_players[username];
    if (m_gamePairs.contains(username)) {
        qDebug() << "[handleFindGame] Игрок уже в игре:" << username;
        QJsonObject response;
        response["type"] = "find_game_response";
        response["success"] = false;
        response["message"] = "Игрок уже в игре";
        sendJson(sender, senderPort, response);
        return;
    }
    m_waitingQueue.append(username);
    qDebug() << "[handleFindGame] Игрок добавлен в очередь ожидания:" << username;
    QJsonObject response;
    response["type"] = "find_game_response";
    response["success"] = true;
    response["message"] = "Поиск игры начат";
    sendJson(sender, senderPort, response);
    processWaitingQueue();
}

void GameServer::processWaitingQueue() {
    qDebug() << "[processWaitingQueue] Размер очереди:" << m_waitingQueue.size();
    while (m_waitingQueue.size() >= 2) {
        QString player1 = m_waitingQueue.takeFirst();
        QString player2 = m_waitingQueue.takeFirst();
        m_gamePairs[player1] = player2;
        m_gamePairs[player2] = player1;
        qDebug() << "[processWaitingQueue] Пара создана:" << player1 << "и" << player2;
        QJsonObject gameFound;
        gameFound["type"] = "game_found";
        gameFound["opponent"] = player2;
        sendJson(m_players[player1]->address, m_players[player1]->port, gameFound);
        gameFound["opponent"] = player1;
        sendJson(m_players[player2]->address, m_players[player2]->port, gameFound);
    }
}

void GameServer::handleBoard(const QHostAddress& sender, quint16 senderPort, const QJsonObject& json) {
    QString username = json["username"].toString();
    qDebug() << "[handleBoard] Получена доска от:" << username;
    QJsonArray boardArray = json["board"].toArray();
    if (!m_players.contains(username)) {
        qDebug() << "[handleBoard] Игрок не найден:" << username;
        QJsonObject response;
        response["type"] = "board_response";
        response["success"] = false;
        response["message"] = "Игрок не найден";
        sendJson(sender, senderPort, response);
        return;
    }
    if (boardArray.size() != BOARD_SIZE) {
        qDebug() << "[handleBoard] Некорректный размер доски:" << boardArray.size();
        QJsonObject response;
        response["type"] = "board_response";
        response["success"] = false;
        response["message"] = "Некорректный размер доски";
        sendJson(sender, senderPort, response);
        return;
    }
    Player* player = m_players[username];
    QVector<QVector<int>> board;
    for (int i = 0; i < boardArray.size(); ++i) {
        QJsonArray row = boardArray[i].toArray();
        if (row.size() != BOARD_SIZE) {
            qDebug() << "[handleBoard] Некорректный размер строки доски:" << row.size();
            QJsonObject response;
            response["type"] = "board_response";
            response["success"] = false;
            response["message"] = "Некорректный размер строки доски";
            sendJson(sender, senderPort, response);
            return;
        }
        QVector<int> boardRow;
        for (int j = 0; j < row.size(); ++j) {
            boardRow.append(row[j].toInt());
        }
        board.append(boardRow);
    }
    if (!checkBoard(board)) {
        qDebug() << "[handleBoard] Некорректная доска (валидация не пройдена)";
        QJsonObject response;
        response["type"] = "board_response";
        response["success"] = false;
        response["message"] = "Некорректная доска";
        sendJson(sender, senderPort, response);
        return;
    }
    player->board = board;
    player->boardReceived = true;
    qDebug() << "[handleBoard] Доска успешно принята от:" << username;
    QJsonObject response;
    response["type"] = "board_response";
    response["success"] = true;
    response["message"] = "Доска принята";
    sendJson(sender, senderPort, response);
    if (m_gamePairs.contains(username)) {
        tryStartGame(username);
    }
}

bool GameServer::checkBoard(const QVector<QVector<int>>& board) {
    if (board.size() != BOARD_SIZE) return false;
    for (const auto& row : board) {
        if (row.size() != BOARD_SIZE) return false;
        for (int cell : row) {
            if (cell != 0 && cell != 1) return false;
        }
    }
    return true;
}

void GameServer::handleShot(const QHostAddress& sender, quint16 senderPort, const QJsonObject& json) {
    QString username = json["username"].toString();
    qDebug() << "[handleShot] Выстрел от:" << username;
    if (!m_players.contains(username) || !m_gamePairs.contains(username)) {
        qDebug() << "[handleShot] Игрок не найден или не в игре:" << username;
        QJsonObject response;
        response["type"] = "shot_response";
        response["success"] = false;
        response["message"] = "Игрок не найден или не в игре";
        sendJson(sender, senderPort, response);
        return;
    }
    Player* player = m_players[username];
    player->address = sender;
    player->port = senderPort;
    QString opponent = m_gamePairs[username];
    int x = json["x"].toInt();
    int y = json["y"].toInt();
    qDebug() << "[handleShot] Координаты:" << x << y;
    if (x < 0 || x >= BOARD_SIZE || y < 0 || y >= BOARD_SIZE) {
        qDebug() << "[handleShot] Некорректные координаты:" << x << y;
        QJsonObject response;
        response["type"] = "shot_response";
        response["success"] = false;
        response["message"] = "Некорректные координаты";
        sendJson(sender, senderPort, response);
        return;
    }
    bool hit = m_players[opponent]->board[y][x] == 1;
    bool sunk = false;
    if (hit) {
        m_players[opponent]->board[y][x] = 2;
        sunk = checkShipSunk(m_players[opponent]->board, x, y);
        if (sunk && areAllShipsSunk(m_players[opponent]->board)) {
            qDebug() << "[handleShot] Победа игрока:" << username;
            handleGameOver(username, opponent);
            return;
        }
    }
    m_gameTimer->stop();
    m_gameTimer->start();
    QJsonObject shotResult;
    shotResult["type"] = "shot_result";
    shotResult["hit"] = hit;
    shotResult["sunk"] = sunk;
    sendJson(sender, senderPort, shotResult);
    QJsonObject opponentNotification;
    opponentNotification["type"] = "opponent_shot";
    opponentNotification["x"] = x;
    opponentNotification["y"] = y;
    opponentNotification["hit"] = hit;
    opponentNotification["sunk"] = sunk;
    sendJson(m_players[opponent]->address, m_players[opponent]->port, opponentNotification);
}

bool GameServer::checkShipSunk(const QVector<QVector<int>>& board, int x, int y) {
    // Проверяем все клетки корабля
    QVector<QPoint> shipCells;
    QVector<QPoint> toCheck;
    toCheck.append(QPoint(x, y));

    while (!toCheck.isEmpty()) {
        QPoint current = toCheck.takeFirst();
        if (shipCells.contains(current)) {
            continue;
        }

        if (board[current.y()][current.x()] == 1 || board[current.y()][current.x()] == 2) {
            shipCells.append(current);

            // Проверяем соседние клетки
            for (int dx = -1; dx <= 1; dx++) {
                for (int dy = -1; dy <= 1; dy++) {
                    if (dx == 0 && dy == 0) continue;
                    if (dx != 0 && dy != 0) continue; // Только горизонтально и вертикально

                    int newX = current.x() + dx;
                    int newY = current.y() + dy;

                    if (newX >= 0 && newX < 10 && newY >= 0 && newY < 10) {
                        toCheck.append(QPoint(newX, newY));
                    }
                }
            }
        }
    }

    // Проверяем, все ли клетки корабля поражены
    for (const QPoint& cell : shipCells) {
        if (board[cell.y()][cell.x()] != 2) {
            return false;
        }
    }

    return !shipCells.isEmpty();
}

bool GameServer::areAllShipsSunk(const QVector<QVector<int>>& board) {
    for (int i = 0; i < BOARD_SIZE; ++i) {
        for (int j = 0; j < BOARD_SIZE; ++j) {
            if (board[i][j] == 1) { // Если остались неподбитые корабли
                return false;
            }
        }
    }
    return true;
}

void GameServer::handleGameOver(const QString& winner, const QString& loser) {
    qDebug() << "[handleGameOver] Победитель:" << winner << "Проигравший:" << loser;
    QJsonObject winnerNotification;
    winnerNotification["type"] = "game_over";
    winnerNotification["result"] = "win";
    sendJson(m_players[winner]->address, m_players[winner]->port, winnerNotification);
    QJsonObject loserNotification;
    loserNotification["type"] = "game_over";
    loserNotification["result"] = "lose";
    sendJson(m_players[loser]->address, m_players[loser]->port, loserNotification);
    m_gamePairs.remove(winner);
    m_gamePairs.remove(loser);
    m_gameTimer->stop();
}

void GameServer::handleReady(const QHostAddress& sender, quint16 senderPort, const QJsonObject& json) {
    QString username = json["username"].toString();
    qDebug() << "[handleReady] Готовность от игрока:" << username;
    if (!m_players.contains(username)) {
        qDebug() << "[handleReady] Игрок не найден:" << username;
        QJsonObject response;
        response["type"] = "ready_response";
        response["success"] = false;
        response["message"] = "Игрок не найден";
        sendJson(sender, senderPort, response);
        return;
    }
    Player* player = m_players[username];
    player->isReady = true;
    QJsonObject response;
    response["type"] = "ready_response";
    response["success"] = true;
    response["message"] = "Готовность подтверждена";
    sendJson(sender, senderPort, response);
    if (m_gamePairs.contains(username)) {
        tryStartGame(username);
    }
}

void GameServer::handleChat(const QHostAddress& sender, quint16 senderPort, const QJsonObject& json) {
    QString username = json["username"].toString();
    qDebug() << "[handleChat] Сообщение от игрока:" << username;
    QString message = json["message"].toString();
    if (!m_players.contains(username) || !m_gamePairs.contains(username)) {
        qDebug() << "[handleChat] Игрок не найден или не в игре:" << username;
        return;
    }
    Player* player = m_players[username];
    QString opponent = m_gamePairs[username];
    Player* opponentPlayer = m_players[opponent];
    QJsonObject chatJson;
    chatJson["type"] = "chat";
    chatJson["username"] = username;
    chatJson["message"] = message;
    sendJson(sender, senderPort, chatJson);
    sendJson(opponentPlayer->address, opponentPlayer->port, chatJson);
}

void GameServer::onGameTimeout() {
    qDebug() << "[onGameTimeout] Сработал таймер игры";
    checkGameTimeout();
}

void GameServer::checkGameTimeout() {
    qDebug() << "[checkGameTimeout] Проверка на таймаут игры";
    for (auto it = m_gamePairs.begin(); it != m_gamePairs.end();) {
        QString player1 = it.key();
        QString player2 = it.value();
        if (!m_players.contains(player1) || !m_players.contains(player2)) {
            it = m_gamePairs.erase(it);
            continue;
        }
        if (!m_players[player1]->isReady || !m_players[player2]->isReady) {
            qDebug() << "[checkGameTimeout] Игра отменена по таймауту между:" << player1 << player2;
            QJsonObject timeout1;
            timeout1["type"] = "game_timeout";
            timeout1["message"] = "Игра отменена из-за таймаута";
            sendJson(m_players[player1]->address, m_players[player1]->port, timeout1);
            QJsonObject timeout2;
            timeout2["type"] = "game_timeout";
            timeout2["message"] = "Игра отменена из-за таймаута";
            sendJson(m_players[player2]->address, m_players[player2]->port, timeout2);
            it = m_gamePairs.erase(it);
            continue;
        }
        ++it;
    }
}

void GameServer::tryStartGame(const QString& username) {
    QString opponent = m_gamePairs[username];
    Player* player1 = m_players[username];
    Player* player2 = m_players[opponent];
    qDebug() << "[tryStartGame] Проверка старта игры для пары:" << username << opponent;
    if (player1->boardReceived && player2->boardReceived && player1->isReady && player2->isReady) {
        QJsonObject gameStart;
        gameStart["type"] = "game_start";
        gameStart["opponent"] = opponent;
        sendJson(player1->address, player1->port, gameStart);
        gameStart["opponent"] = username;
        sendJson(player2->address, player2->port, gameStart);
        m_gameTimer->start();
        qDebug() << "[tryStartGame] Игра началась между" << username << "и" << opponent;
    }
}

void GameServer::removePlayer(const QString& username) {
    qDebug() << "[removePlayer] Удаление игрока:" << username;
    if (!m_players.contains(username)) {
        return;
    }
    if (m_gamePairs.contains(username)) {
        QString opponent = m_gamePairs[username];
        if (m_players.contains(opponent)) {
            QJsonObject response;
            response["type"] = "opponent_disconnected";
            response["message"] = "Противник отключился";
            sendJson(m_players[opponent]->address, m_players[opponent]->port, response);
        }
        m_gamePairs.remove(username);
        m_gamePairs.remove(opponent);
    }
    delete m_players[username];
    m_players.remove(username);
}

void GameServer::sendJson(const QHostAddress& address, quint16 port, const QJsonObject& json) {
    QByteArray data = QJsonDocument(json).toJson();
    m_socket->writeDatagram(data, address, port);
} 