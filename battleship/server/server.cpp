#include <QCoreApplication>
#include <QUdpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QVector>
#include <QTimer>
#include <QDebug>
#include <QDateTime>
#include <QPair>


inline bool operator<(const QHostAddress &a1, const QHostAddress &a2) {
    return a1.toString() < a2.toString();
}

class GameServer : public QObject {
    Q_OBJECT
public:
    explicit GameServer(QObject *parent = nullptr) : QObject(parent) {
        server = new QUdpSocket(this);
        connect(server, &QUdpSocket::readyRead, this, &GameServer::handleClientData);
        
        if (!server->bind(QHostAddress::Any, 12345)) {
            qDebug() << "Сервер не может запуститься!";
            return;
        }
        
        qDebug() << "Сервер запущен на порту 12345";
        
        // Таймер для очистки неактивных клиентов
        cleanupTimer = new QTimer(this);
        connect(cleanupTimer, &QTimer::timeout, this, &GameServer::cleanupInactiveClients);
        cleanupTimer->start(30000); // Проверка каждые 30 секунд
    }

private slots:
    void handleClientData() {
        QByteArray datagram;
        datagram.resize(server->pendingDatagramSize());
        QHostAddress sender;
        quint16 senderPort;

        server->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);
        
        QPair<QHostAddress, quint16> clientKey(sender, senderPort);
        qDebug() << "Получены данные от клиента:" << datagram;
        
        QJsonDocument doc = QJsonDocument::fromJson(datagram);
        if (doc.isNull()) {
            qDebug() << "Ошибка при разборе JSON";
            return;
        }
        
        QJsonObject request = doc.object();
        QString type = request["type"].toString();
        qDebug() << "Тип сообщения:" << type << "от" << sender.toString() << ":" << senderPort;
        
        // Получаем или создаем состояние клиента
        if (!clients.contains(clientKey)) {
            clients[clientKey] = ClientState();
        }
        ClientState &clientState = clients[clientKey];
        clientState.lastActivity = QDateTime::currentDateTime();
        clientState.address = sender;
        clientState.port = senderPort;
        
        if (type == "login") {
            handleLogin(clientKey, request);
        }
        else if (type == "find_game") {
            handleFindGame(clientKey);
        }
        else if (type == "shot") {
            handleShot(clientKey, request);
        }
        else if (type == "board") {
            handleBoard(clientKey, request);
        }
        else if (type == "ping") {
            handlePing(clientKey);
        }
        else if (type == "chat") {
            handleChat(clientKey, request);
        }
        else if (type == "ready") {
            handleReady(clientKey, request);
        }
        else {
            qDebug() << "Неизвестный тип сообщения:" << type;
        }
    }

    void cleanupInactiveClients() {
        QDateTime now = QDateTime::currentDateTime();
        QList<QPair<QHostAddress, quint16>> toRemove;
        
        for (auto it = clients.begin(); it != clients.end(); ++it) {
            if (it.value().lastActivity.secsTo(now) > 60) { // 1 минута без активности
                qDebug() << "Удаление неактивного клиента:" << it.value().username;
                toRemove.append(it.key());
            }
        }
        
        for (const auto &clientKey : toRemove) {
            // Если клиент был в игре, уведомляем противника
            if (clients[clientKey].inGame && clients.contains(clients[clientKey].opponentKey)) {
                QJsonObject response;
                response["type"] = "game_over";
                response["winner"] = clients[clients[clientKey].opponentKey].username;
                sendJson(clientKey, response);
            }
            
            // Удаляем клиента из всех списков
            removeFromWaitingList(clientKey);
            removeFromActiveGames(clientKey);
            clients.remove(clientKey);
            // Выводим количество клиентов после удаления
            qDebug() << "Сейчас подключено клиентов:" << clients.size();
        }
    }

private:
    struct ClientState {
        QString username;
        bool inGame = false;
        QPair<QHostAddress, quint16> opponentKey;
        QVector<QVector<int>> board;
        QDateTime lastActivity;
        QHostAddress address;
        quint16 port;
    };

    struct Game {
        QPair<QHostAddress, quint16> player1Key;
        QPair<QHostAddress, quint16> player2Key;
        bool player1Turn = true;
        bool player1Ready = false;
        bool player2Ready = false;
        
        bool operator==(const Game &other) const {
            return player1Key == other.player1Key && player2Key == other.player2Key;
        }
    };

    QUdpSocket *server;
    QTimer *cleanupTimer;
    QMap<QPair<QHostAddress, quint16>, ClientState> clients;
    QVector<QPair<QHostAddress, quint16>> waitingList;
    QVector<Game> activeGames;

    void handleLogin(const QPair<QHostAddress, quint16> &clientKey, const QJsonObject &request) {
        QString username = request["username"].toString();
        if (username.isEmpty()) {
            qDebug() << "Пустое имя пользователя";
            return;
        }
        
        // Проверяем, не занято ли имя
        for (const auto &client : clients) {
            if (client.username == username) {
                QJsonObject response;
                response["type"] = "login_response";
                response["success"] = false;
                response["error"] = "Имя пользователя уже занято";
                sendJson(clientKey, response);
                return;
            }
        }
        
        clients[clientKey].username = username;
        qDebug() << "Пользователь вошел:" << username;
        
        QJsonObject response;
        response["type"] = "login_response";
        response["success"] = true;
        
        sendJson(clientKey, response);
        // Выводим количество клиентов
        qDebug() << "Сейчас подключено клиентов:" << clients.size();
    }

    void handlePing(const QPair<QHostAddress, quint16> &clientKey) {
        QJsonObject response;
        response["type"] = "pong";
        sendJson(clientKey, response);
    }

    void handleBoard(const QPair<QHostAddress, quint16> &clientKey, const QJsonObject &request) {
        if (!clients[clientKey].inGame) {
            qDebug() << "Клиент не в игре:" << clients[clientKey].username;
            return;
        }

        QPair<QHostAddress, quint16> opponentKey = clients[clientKey].opponentKey;
        if (!clients.contains(opponentKey)) {
            qDebug() << "Противник не найден";
            return;
        }

        // Пересылаем доску противнику
        QJsonObject boardMessage;
        boardMessage["type"] = "board";
        boardMessage["board"] = request["board"];
        sendJson(opponentKey, boardMessage);
    }

    void handleFindGame(const QPair<QHostAddress, quint16> &clientKey) {
        qDebug() << "handleFindGame: начало метода для клиента" << clients[clientKey].username;

        if (clients[clientKey].inGame) {
            qDebug() << "handleFindGame: клиент уже в игре:" << clients[clientKey].username;
            return;
        }

        // Если клиент уже в списке ожидания, не делаем ничего
        if (waitingList.contains(clientKey)) {
            qDebug() << "handleFindGame: клиент уже в списке ожидания:" << clients[clientKey].username;
            return;
        }

        // Если есть ожидающие игроки
        if (!waitingList.isEmpty()) {
            QPair<QHostAddress, quint16> opponentKey = waitingList.takeFirst();
            if (opponentKey == clientKey) {
                // Если это тот же клиент, добавляем его обратно в список ожидания
                waitingList.append(clientKey);
                QJsonObject response;
                response["type"] = "waiting";
                sendJson(clientKey, response);
                return;
            }

            // Создаём новую игру
            Game game;
            game.player1Key = clientKey;
            game.player2Key = opponentKey;
            game.player1Ready = false;
            game.player2Ready = false;
            activeGames.append(game);

            // Обновляем состояние клиентов
            clients[clientKey].inGame = true;
            clients[clientKey].opponentKey = opponentKey;
            clients[opponentKey].inGame = true;
            clients[opponentKey].opponentKey = clientKey;

            // Уведомляем игроков о начале игры
            QJsonObject response1;
            response1["type"] = "game_start";
            response1["opponent"] = clients[opponentKey].username;
            response1["waiting_for_ready"] = true;
            sendJson(clientKey, response1);

            QJsonObject response2;
            response2["type"] = "game_start";
            response2["opponent"] = clients[clientKey].username;
            response2["waiting_for_ready"] = true;
            sendJson(opponentKey, response2);

            qDebug() << "handleFindGame: игра создана между" << clients[clientKey].username << "и" << clients[opponentKey].username;
            return;
        }

        // Если нет ожидающих игроков, добавляем в очередь
        waitingList.append(clientKey);
        QJsonObject response;
        response["type"] = "waiting";
        sendJson(clientKey, response);
        qDebug() << "handleFindGame: клиент добавлен в список ожидания:" << clients[clientKey].username;
    }

    void handleShot(const QPair<QHostAddress, quint16> &clientKey, const QJsonObject &request) {
        if (!clients[clientKey].inGame) {
            qDebug() << "Клиент не в игре:" << clients[clientKey].username;
            return;
        }

        QPair<QHostAddress, quint16> opponentKey = clients[clientKey].opponentKey;
        if (!clients.contains(opponentKey)) {
            qDebug() << "Противник не найден";
            return;
        }

        // Находим игру
        Game* currentGame = nullptr;
        for (auto& game : activeGames) {
            if ((game.player1Key == clientKey && game.player2Key == opponentKey) ||
                (game.player1Key == opponentKey && game.player2Key == clientKey)) {
                currentGame = &game;
                break;
            }
        }

        if (!currentGame) {
            qDebug() << "Игра не найдена";
            return;
        }

        // Проверяем, чей сейчас ход
        bool isPlayer1Turn = (currentGame->player1Key == clientKey);
        if (isPlayer1Turn != currentGame->player1Turn) {
            qDebug() << "Не ваш ход";
            QJsonObject errorResponse;
            errorResponse["type"] = "error";
            errorResponse["message"] = "Не ваш ход";
            sendJson(clientKey, errorResponse);
            return;
        }

        // Получаем координаты выстрела
        int x = request["x"].toInt();
        int y = request["y"].toInt();

        // Пересылаем выстрел противнику
        QJsonObject shotMessage;
        shotMessage["type"] = "shot";
        shotMessage["x"] = x;
        shotMessage["y"] = y;
        sendJson(opponentKey, shotMessage);

        // Меняем очередь хода
        currentGame->player1Turn = !currentGame->player1Turn;

        // Отправляем сообщение о смене хода обоим игрокам
        QJsonObject turnMessage;
        turnMessage["type"] = "turn_change";
        turnMessage["your_turn"] = !isPlayer1Turn;
        sendJson(clientKey, turnMessage);
        
        QJsonObject opponentTurnMessage;
        opponentTurnMessage["type"] = "turn_change";
        opponentTurnMessage["your_turn"] = isPlayer1Turn;
        sendJson(opponentKey, opponentTurnMessage);
    }

    void removeFromWaitingList(const QPair<QHostAddress, quint16> &clientKey) {
        waitingList.removeOne(clientKey);
    }

    void removeFromActiveGames(const QPair<QHostAddress, quint16> &clientKey) {
        for (int i = 0; i < activeGames.size(); ++i) {
            const Game &game = activeGames[i];
            if ((game.player1Key == clientKey) || (game.player2Key == clientKey)) {
                
                QPair<QHostAddress, quint16> opponentKey = (game.player1Key == clientKey) ? game.player2Key : game.player1Key;
                if (clients.contains(opponentKey)) {
                    clients[opponentKey].inGame = false;
                }
                
                activeGames.removeAt(i);
                break;
            }
        }
    }

    void sendJson(const QPair<QHostAddress, quint16> &clientKey, const QJsonObject &json) {
        QJsonDocument doc(json);
        QByteArray data = doc.toJson();
        server->writeDatagram(data, clientKey.first, clientKey.second);
    }

    void handleChat(const QPair<QHostAddress, quint16> &clientKey, const QJsonObject &request) {
        if (!clients[clientKey].inGame) {
            qDebug() << "Клиент не в игре:" << clients[clientKey].username;
            return;
        }

        QPair<QHostAddress, quint16> opponentKey = clients[clientKey].opponentKey;
        if (!clients.contains(opponentKey)) {
            qDebug() << "Противник не найден";
            return;
        }

        // Пересылаем сообщение противнику
        QJsonObject chatMessage;
        chatMessage["type"] = "chat";
        chatMessage["sender"] = clients[clientKey].username;
        chatMessage["message"] = request["message"].toString();
        sendJson(opponentKey, chatMessage);
    }

    void handleReady(const QPair<QHostAddress, quint16> &clientKey, const QJsonObject &request) {
        QString username = request["username"].toString();
        if (!clients.contains(clientKey)) {
            return;
        }

        // Находим игру, в которой участвует клиент
        for (auto &game : activeGames) {
            if (game.player1Key == clientKey) {
                game.player1Ready = true;
                if (game.player2Ready) {
                    // Оба игрока готовы, начинаем игру
                    QJsonObject gameStartMsg;
                    gameStartMsg["type"] = "game_start";
                    gameStartMsg["opponent"] = clients[game.player2Key].username;
                    gameStartMsg["waiting_for_ready"] = false;
                    sendJson(game.player1Key, gameStartMsg);

                    gameStartMsg["opponent"] = clients[game.player1Key].username;
                    sendJson(game.player2Key, gameStartMsg);

                    // Отправляем сообщение о первом ходе
                    QJsonObject turnMsg;
                    turnMsg["type"] = "turn_change";
                    turnMsg["your_turn"] = true;
                    sendJson(game.player1Key, turnMsg);
                    
                    turnMsg["your_turn"] = false;
                    sendJson(game.player2Key, turnMsg);
                }
                break;
            }
            else if (game.player2Key == clientKey) {
                game.player2Ready = true;
                if (game.player1Ready) {
                    // Оба игрока готовы, начинаем игру
                    QJsonObject gameStartMsg;
                    gameStartMsg["type"] = "game_start";
                    gameStartMsg["opponent"] = clients[game.player1Key].username;
                    gameStartMsg["waiting_for_ready"] = false;
                    sendJson(game.player2Key, gameStartMsg);

                    gameStartMsg["opponent"] = clients[game.player2Key].username;
                    sendJson(game.player1Key, gameStartMsg);

                    // Отправляем сообщение о первом ходе
                    QJsonObject turnMsg;
                    turnMsg["type"] = "turn_change";
                    turnMsg["your_turn"] = true;
                    sendJson(game.player1Key, turnMsg);
                    
                    turnMsg["your_turn"] = false;
                    sendJson(game.player2Key, turnMsg);
                }
                break;
            }
        }
    }
};

int main(int argc, char *argv[]) {
    QCoreApplication a(argc, argv);
    GameServer server;
    return a.exec();
} 