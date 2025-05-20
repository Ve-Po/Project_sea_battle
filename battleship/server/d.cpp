#include "gameserver.h"
#include <QUuid>
#include <QDateTime>
#include <QRandomGenerator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

// Константы таймаутов


GameServer::GameServer(QObject *parent) : QObject(parent),
    m_socket(new QUdpSocket(this)),       // Создаем ОДИН UDP сокет для сервера
    m_gameTimer(new QTimer(this)),
    m_sessionTimer(new QTimer(this)),
    m_pingTimer(new QTimer(this))
{
    // Настройка соединений для сокета
    connect(m_socket, &QUdpSocket::readyRead, this, &GameServer::onReadyRead);
    connect(m_socket, &QUdpSocket::errorOccurred, this, &GameServer::onError);
    
    // Настройка интервалов таймеров
    m_gameTimer->setInterval(GAME_TIMEOUT_MS / 2); // Проверка таймаута игры каждые 60 секунд
    m_sessionTimer->setInterval(SESSION_TIMEOUT_S * 1000); // Проверка неактивных сессий
    m_pingTimer->setInterval(PING_INTERVAL_MS);    // Отправка пингов
    
    // Соединение таймеров с обработчиками
    connect(m_gameTimer, &QTimer::timeout, this, &GameServer::onGameTimeout);
    connect(m_sessionTimer, &QTimer::timeout, this, &GameServer::onSessionTimeout);
    connect(m_pingTimer, &QTimer::timeout, this, &GameServer::onPingTimerTimeout);
}

GameServer::~GameServer()
{
    // Остановка сервера и освобождение ресурсов
    stop();
}

bool GameServer::start(quint16 port)
{
    // Попытка привязать сокет к указанному порту
    if (m_socket->bind(QHostAddress::Any, port)) {
        qDebug() << "Сервер запущен на порту" << port;
        m_sessionTimer->start();
        m_pingTimer->start();
        return true;
    }
    qDebug() << "Ошибка запуска сервера:" << m_socket->errorString();
    return false;
}

void GameServer::stop()
{
    // Остановка всех компонентов сервера
    m_socket->close();
    m_gameTimer->stop();
    m_sessionTimer->stop();
    m_pingTimer->stop();
    m_clients.clear();
    m_games.clear();
    m_clientAddressToId.clear();
}

// Основной метод обработки входящих сообщений
void GameServer::onReadyRead()
{
    // Обработка всех ожидающих датаграмм
    while (m_socket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_socket->receiveDatagram();
        QByteArray data = datagram.data();
        QHostAddress sender = datagram.senderAddress();
        quint16 senderPort = datagram.senderPort();
        
        // Парсинг JSON сообщения
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(data, &error);
        if (error.error != QJsonParseError::NoError) {
            qDebug() << "Ошибка разбора JSON:" << error.errorString();
            continue;
        }
        
        QJsonObject json = doc.object();
        QString type = json["type"].toString();
        QString clientId = getClientId(sender, senderPort);
        
        // Обновляем время последней активности клиента
        if (m_clients.contains(clientId)) {
            m_clients[clientId].lastActive = QDateTime::currentSecsSinceEpoch();
        }
        
        // Маршрутизация сообщений по их типу
        if (type == "login") {
            handleLogin(json, clientId);
        } else if (type == "new_game") {
            handleNewGame(json, clientId);
        } else if (type == "join_game") {
            handleJoinGame(json, clientId);
        } else if (type == "ready") {
            qDebug() << "ready:" << sender << senderPort;
            handleReady(json, clientId);
        } else if (type == "shot") {
            handleShot(json, clientId);
        } else if (type == "shot_result") {
            handleShotResult(json, clientId);
        } else if (type == "board") {
            handleBoard(json, clientId);
        } else if (type == "chat") {
            handleChatMessage(json, clientId);
        } else if (type == "ping") {
            handlePing(json, clientId);
        } else if (type == "reconnect") {
            handleReconnect(json, clientId);
        } else {
            qDebug() << "Неизвестный тип сообщения:" << type;
        }
    }
}

// Обработка сообщения входа
void GameServer::handleLogin(const QJsonObject &json, const QString &clientId)
{
    QString username = json["username"].toString();
    if (username.isEmpty()) {
        sendError("Имя пользователя не может быть пустым", clientId);
        return;
    }
    
    // Сохраняем имя пользователя
    m_clients[clientId].username = username;
    
    // Отправляем подтверждение входа
    QJsonObject response;
    response["type"] = "login_response";
    response["success"] = true;
    sendJson(response, clientId);
}

// Обработка создания новой игры
void GameServer::handleNewGame(const QJsonObject &json, const QString &clientId)
{
    if (!validateClient(clientId)) return;
    
    // Генерируем уникальный ID для игры
    QString gameId = generateGameId();
    GameInfo newGame;
    newGame.id = gameId;
    newGame.player1 = clientId;
    m_games[gameId] = newGame;
    
    // Привязываем игру к клиенту
    m_clients[clientId].gameId = gameId;
    
    // Отправляем клиенту ID созданной игры
    QJsonObject response;
    response["type"] = "game_created";
    response["game_id"] = gameId;
    sendJson(response, clientId);
}

// Обработка присоединения к игре
void GameServer::handleJoinGame(const QJsonObject &json, const QString &clientId)
{
    if (!validateClient(clientId)) return;
    
    QString gameId = json["game_id"].toString();
    if (!m_games.contains(gameId)) {
        sendError("Игра не найдена", clientId);
        return;
    }
    
    GameInfo &game = m_games[gameId];
    if (!game.player2.isEmpty()) {
        sendError("В игре уже есть два игрока", clientId);
        return;
    }
    
    // Добавляем второго игрока
    game.player2 = clientId;
    m_clients[clientId].gameId = gameId;
    
    // Уведомляем обоих игроков о соединении
    QJsonObject response1;
    response1["type"] = "game_found";
    response1["game_id"] = gameId;
    response1["opponent"] = m_clients[game.player1].username;
    sendJson(response1, game.player1);
    
    QJsonObject response2;
    response2["type"] = "game_found";
    response2["game_id"] = gameId;
    response2["opponent"] = m_clients[game.player1].username;
    sendJson(response2, clientId);
}

// Обработка готовности игрока
void GameServer::handleReady(const QJsonObject &json, const QString &clientId) {
    if (!validateClient(clientId)) return; 
    QString gameId = m_clients[clientId].gameId;
    qDebug() << "gameid" << gameId;
    if (gameId.isEmpty() || !m_games.contains(gameId)) {
        sendError("Вы не в игре", clientId);
        return;
    }
    
    GameInfo &game = m_games[gameId];
    
    // Проверяем и сохраняем доску
    if (!json.contains("board")) {
        sendError("Не получена расстановка кораблей", clientId);
        return;
    }
    
    QJsonArray board = json["board"].toArray();
    if (!validateBoard(board)) {
        sendError("Некорректная расстановка кораблей", clientId);
        return;
    }
    
    // Сохраняем готовность и доску
    if (game.player1 == clientId) {
        game.player1Ready = true;
        game.player1Board = board;
    } else if (game.player2 == clientId) {
        game.player2Ready = true;
        game.player2Board = board;
    }
    
    // Уведомляем противника
    QString opponentId = (game.player1 == clientId) ? game.player2 : game.player1;
    if (!opponentId.isEmpty()) {
        QJsonObject notice;
        notice["type"] = "player_ready";
        notice["username"] = m_clients[clientId].username;
        sendJson(notice, opponentId);
    }
    
    // Если оба готовы - начинаем игру
    if (game.player1Ready && game.player2Ready && 
        !game.player1Board.isEmpty() && !game.player2Board.isEmpty()) {
        startGame(gameId);
    }
}

// Обработка выстрела
void GameServer::handleShot(const QJsonObject &json, const QString &clientId)
{
    if (!validateClient(clientId)) return;
    
    QString gameId = m_clients[clientId].gameId;
    if (!validateGame(gameId, clientId)) return;
    
    GameInfo &game = m_games[gameId];
    
    // Проверяем, что ход делает правильный игрок
    if ((game.player1 == clientId && !game.player1Turn) || 
        (game.player2 == clientId && game.player1Turn)) {
        sendError("Сейчас не ваш ход", clientId);
        return;
    }
    
    // Отправляем выстрел противнику
    QString opponentId = (game.player1 == clientId) ? game.player2 : game.player1;
    QJsonObject shotMsg = json;
    shotMsg["type"] = "shot";
    sendJson(shotMsg, opponentId);
    
    // Меняем очередь хода и обновляем время последнего действия
    game.player1Turn = !game.player1Turn;
    game.lastTurnTime = QDateTime::currentSecsSinceEpoch();
}

// Остальные методы остаются аналогичными, но с русскими комментариями...

// Получение ID клиента по адресу и порту
QString GameServer::getClientId(const QHostAddress &address, quint16 port)
{
    QString key = address.toString() + ":" + QString::number(port);
    
    if (!m_clientAddressToId.contains(key)) {
        QString newId = QUuid::createUuid().toString();
        m_clientAddressToId[key] = newId;
        
        ClientInfo newClient;
        newClient.id = newId;
        newClient.address = address;
        newClient.port = port;
        newClient.lastActive = QDateTime::currentSecsSinceEpoch();
        m_clients[newId] = newClient;
    }
    return m_clientAddressToId[key];
}
// Генерация ID игры
QString GameServer::generateGameId() const
{
    const QString chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    QString id;
    for (int i = 0; i < 6; ++i) {
        id.append(chars.at(QRandomGenerator::global()->bounded(chars.length())));
    }
    return id;
}

// Отправка JSON сообщения клиенту
void GameServer::sendJson(const QJsonObject &json, const QString &clientId)
{
    if (!m_clients.contains(clientId)) return;
    
    QJsonDocument doc(json);
    m_socket->writeDatagram(doc.toJson(), 
                           m_clients[clientId].address, 
                           m_clients[clientId].port);
}

// Отправка сообщения об ошибке
void GameServer::sendError(const QString &message, const QString &clientId)
{
    QJsonObject error;
    error["type"] = "error";
    error["message"] = message;
    sendJson(error, clientId);
}

// Проверка валидности клиента
bool GameServer::validateClient(const QString &clientId)
{
    if (!m_clients.contains(clientId)) {
        sendError("Клиент не зарегистрирован", clientId);
        return false;
    }
    return true;
}

// Проверка валидности игры
bool GameServer::validateGame(const QString &gameId, const QString &clientId)
{
    if (!m_games.contains(gameId)) {
        sendError("Игра не найдена", clientId);
        return false;
    }
    
    if (!m_games[gameId].isActive) {
        sendError("Игра не активна", clientId);
        return false;
    }
    
    return true;
}

// Error handler
void GameServer::onError(QAbstractSocket::SocketError socketError)
{
    qDebug() << "Socket error occurred:" << m_socket->errorString();
}

// Game timeout handler
void GameServer::onGameTimeout()
{
    qint64 currentTime = QDateTime::currentSecsSinceEpoch();
    
    // Check all active games for timeout
    for (auto it = m_games.begin(); it != m_games.end(); ) {
        if (currentTime - it->lastTurnTime > GAME_TIMEOUT_MS / 1000) {
            // Notify players about timeout
            QJsonObject timeoutMsg;
            timeoutMsg["type"] = "game_timeout";
            sendJson(timeoutMsg, it->player1);
            if (!it->player2.isEmpty()) {
                sendJson(timeoutMsg, it->player2);
            }
            
            // Remove the game
            it = m_games.erase(it);
        } else {
            ++it;
        }
    }
}

// Session timeout handler
void GameServer::onSessionTimeout()
{
    qint64 currentTime = QDateTime::currentSecsSinceEpoch();
    
    for (auto it = m_clients.begin(); it != m_clients.end(); ) {
        if (currentTime - it->lastActive > SESSION_TIMEOUT_S) {
            // Create string key for removal
            QString addrKey = it->address.toString() + ":" + QString::number(it->port);
            m_clientAddressToId.remove(addrKey);
            
            it = m_clients.erase(it);
        } else {
            ++it;
        }
    }
}
// Ping timer handler
void GameServer::onPingTimerTimeout()
{
    QJsonObject pingMsg;
    pingMsg["type"] = "ping";
    
    // Send ping to all connected clients
    for (const auto& client : m_clients) {
        sendJson(pingMsg, client.id);
    }
}

// Start game handler
void GameServer::startGame(const QString &gameId) {
    if (!m_games.contains(gameId)) return;
    
    GameInfo &game = m_games[gameId];
    game.isActive = true;
    game.player1Turn = true; // Первый ход у игрока 1
    game.lastTurnTime = QDateTime::currentSecsSinceEpoch();
    
    // Отправляем состояние игры обоим игрокам
    QJsonObject startMsg;
    startMsg["type"] = "game_start";
    
    // Игроку 1
    startMsg["your_turn"] = true;
    startMsg["opponent"] = m_clients[game.player2].username;
    sendJson(startMsg, game.player1);
    
    // Игроку 2
    startMsg["your_turn"] = false;
    startMsg["opponent"] = m_clients[game.player1].username;
    sendJson(startMsg, game.player2);
    
    // Запускаем таймер игры, если не запущен
    if (!m_gameTimer->isActive()) {
        m_gameTimer->start();
    }
}

// Shot result handler
void GameServer::handleShotResult(const QJsonObject &json, const QString &clientId)
{
    if (!validateClient(clientId)) return;
    
    QString gameId = m_clients[clientId].gameId;
    if (!validateGame(gameId, clientId)) return;
    
    // Forward the result to the opponent
    GameInfo& game = m_games[gameId];
    QString opponentId = (game.player1 == clientId) ? game.player2 : game.player1;
    sendJson(json, opponentId);
}

// Board handler
void GameServer::handleBoard(const QJsonObject &json, const QString &clientId)
{
    if (!validateClient(clientId)) return;
    
    QString gameId = m_clients[clientId].gameId;
    if (!validateGame(gameId, clientId)) return;
    
    // Store the board configuration
    if (json.contains("board")) {
        GameInfo& game = m_games[gameId];
        if (game.player1 == clientId) {
            game.player1Board = json["board"].toArray();
        } else {
            game.player2Board = json["board"].toArray();
        }
    }
}

// Chat message handler
void GameServer::handleChatMessage(const QJsonObject &json, const QString &clientId)
{
    if (!validateClient(clientId)) return;
    
    QString gameId = m_clients[clientId].gameId;
    if (!validateGame(gameId, clientId)) return;
    
    // Forward the message to the opponent
    GameInfo& game = m_games[gameId];
    QString opponentId = (game.player1 == clientId) ? game.player2 : game.player1;
    sendJson(json, opponentId);
}

// Ping handler
void GameServer::handlePing(const QJsonObject &json, const QString &clientId)
{
    if (!validateClient(clientId)) return;
    
    // Simply update last active time
    m_clients[clientId].lastActive = QDateTime::currentSecsSinceEpoch();
}

void GameServer::handleReconnect(const QJsonObject &json, const QString &clientId)
{
    if (!validateClient(clientId)) {
        sendError("Неверный ID клиента", clientId);
        return;
    }

    QString gameId = m_clients[clientId].gameId;
    if (gameId.isEmpty() || !m_games.contains(gameId)) {
        sendError("Игра не найдена", clientId);
        return;
    }

    GameInfo &game = m_games[gameId];
    m_clients[clientId].isConnected = true;
    m_clients[clientId].lastActive = QDateTime::currentSecsSinceEpoch();

    // Отправляем текущее состояние игры
    sendGameState(gameId, clientId);
}

bool GameServer::validateGameState(const QString &gameId)
{
    if (!m_games.contains(gameId)) {
        return false;
    }

    GameInfo &game = m_games[gameId];
    
    // Проверяем, что оба игрока подключены
    if (!m_clients.contains(game.player1) || !m_clients.contains(game.player2)) {
        return false;
    }

    // Проверяем, что оба игрока готовы
    if (!game.player1Ready || !game.player2Ready) {
        return false;
    }

    // Проверяем, что доски установлены
    if (game.player1Board.isEmpty() || game.player2Board.isEmpty()) {
        return false;
    }

    return true;
}

void GameServer::syncGameState(const QString &gameId)
{
    if (!m_games.contains(gameId)) {
        return;
    }

    GameInfo &game = m_games[gameId];
    
    // Отправляем состояние обоим игрокам
    sendGameState(gameId, game.player1);
    sendGameState(gameId, game.player2);
}

void GameServer::handleDisconnect(const QString &clientId)
{
    if (!m_clients.contains(clientId)) {
        return;
    }

    m_clients[clientId].isConnected = false;
    QString gameId = m_clients[clientId].gameId;

    if (!gameId.isEmpty() && m_games.contains(gameId)) {
        GameInfo &game = m_games[gameId];
        QString opponentId = getOpponentId(gameId, clientId);

        if (!opponentId.isEmpty()) {
            QJsonObject json;
            json["type"] = "opponent_disconnected";
            sendJson(json, opponentId);
        }

        // Даем 30 секунд на переподключение
        QTimer::singleShot(30000, this, [this, gameId, clientId]() {
            if (m_clients.contains(clientId) && !m_clients[clientId].isConnected) {
                cleanupGame(gameId);
            }
        });
    }
}

bool GameServer::validateBoard(const QJsonArray &board)
{
    if (board.size() != 10) {
        return false;
    }

    int shipCount = 0;
    for (int i = 0; i < 10; i++) {
        if (board[i].toArray().size() != 10) {
            return false;
        }
        for (int j = 0; j < 10; j++) {
            if (board[i].toArray()[j].toInt() == 1) {
                shipCount++;
            }
        }
    }

    // Проверяем количество клеток с кораблями (должно быть 20)
    return shipCount == 20;
}

void GameServer::notifyGameStateChange(const QString &gameId)
{
    if (!m_games.contains(gameId)) {
        return;
    }

    GameInfo &game = m_games[gameId];
    QJsonObject json;
    json["type"] = "game_state_changed";
    json["gameId"] = gameId;
    json["player1Turn"] = game.player1Turn;
    json["isActive"] = game.isActive;

    if (!game.player1.isEmpty()) {
        sendJson(json, game.player1);
    }
    if (!game.player2.isEmpty()) {
        sendJson(json, game.player2);
    }
}

void GameServer::cleanupGame(const QString &gameId)
{
    if (!m_games.contains(gameId)) {
        return;
    }

    GameInfo &game = m_games[gameId];
    
    // Уведомляем игроков о завершении игры
    QJsonObject json;
    json["type"] = "game_ended";
    json["reason"] = "opponent_disconnected";

    if (!game.player1.isEmpty() && m_clients.contains(game.player1)) {
        sendJson(json, game.player1);
        m_clients[game.player1].gameId.clear();
    }
    if (!game.player2.isEmpty() && m_clients.contains(game.player2)) {
        sendJson(json, game.player2);
        m_clients[game.player2].gameId.clear();
    }

    m_games.remove(gameId);
}

QString GameServer::getOpponentId(const QString &gameId, const QString &clientId)
{
    if (!m_games.contains(gameId)) {
        return QString();
    }

    GameInfo &game = m_games[gameId];
    if (game.player1 == clientId) {
        return game.player2;
    } else if (game.player2 == clientId) {
        return game.player1;
    }
    return QString();
}

void GameServer::sendGameState(const QString &gameId, const QString &clientId)
{
    if (!m_games.contains(gameId) || !m_clients.contains(clientId)) {
        return;
    }

    GameInfo &game = m_games[gameId];
    QJsonObject json;
    json["type"] = "game_state";
    json["gameId"] = gameId;
    json["isYourTurn"] = (game.player1 == clientId) ? game.player1Turn : !game.player1Turn;
    json["isActive"] = game.isActive;
    
    // Отправляем только свою доску
    if (game.player1 == clientId) {
        json["yourBoard"] = game.player1Board;
    } else {
        json["yourBoard"] = game.player2Board;
    }

    // Добавляем информацию о последнем выстреле
    if (game.lastShotX != -1 && game.lastShotY != -1) {
        json["lastShotX"] = game.lastShotX;
        json["lastShotY"] = game.lastShotY;
        json["lastShotHit"] = game.lastShotHit;
    }

    sendJson(json, clientId);
}