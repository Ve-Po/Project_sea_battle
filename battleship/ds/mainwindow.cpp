#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QDateTime>
#include <QTimer>
#include <QRandomGenerator>
#include "ui_mainwindow.h"
#include <QInputDialog>
#include <QDebug>
#include <QJsonObject>
#include <QJsonDocument>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    m_myTurn(false),
    m_gameActive(false),
    m_currentShipType(GameBoard::ShipSize::BATTLESHIP),
    m_isHorizontal(true),
    m_aiFoundShip(false),
    m_placementBoard(nullptr),
    m_opponentBoard(nullptr),
    m_ownBoard(nullptr),
    m_isGameStarted(false),
    m_isConnected(false),
    m_networkMode(false),
    m_isMyTurn(false),
    m_pendingUsername(""),
    m_placementMode(true),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    
    // Диалог выбора режима
    QStringList modes;
    modes << "Играть по сети" << "Играть с компьютером";
    bool ok;
    QString mode = QInputDialog::getItem(this, "Выбор режима", "Выберите режим игры:", modes, 0, false, &ok);
    if (!ok) {
        QApplication::quit();
        return;
    }
    m_networkMode = (mode == "Играть по сети");

    m_networkClient = new NetworkClient(this);
    connect(m_networkClient, &NetworkClient::connected, this, &MainWindow::onConnected);
    connect(m_networkClient, &NetworkClient::disconnected, this, &MainWindow::onDisconnected);
    connect(m_networkClient, &NetworkClient::loginResponse, this, &MainWindow::onLoginResponse);
    connect(m_networkClient, &NetworkClient::gameFound, this, &MainWindow::onGameFound);
    connect(m_networkClient, &NetworkClient::waitingForOpponent, this, &MainWindow::onWaitingForOpponent);
    connect(m_networkClient, &NetworkClient::shotResult, this, &MainWindow::onShotResult);
    connect(m_networkClient, &NetworkClient::gameOver, this, &MainWindow::onGameOver);
    connect(m_networkClient, &NetworkClient::error, this, &MainWindow::onNetworkError);
    connect(m_networkClient, &NetworkClient::chatMessageReceived, this, &MainWindow::onChatMessageReceived);
    connect(m_networkClient, &NetworkClient::shotReceived, this, &MainWindow::onShotReceived);
    connect(m_networkClient, &NetworkClient::turnChanged, this, &MainWindow::onTurnChanged);
    
    setupUI();
    setupConnections();
    setWindowTitle("Морской бой");
    resize(800, 600);
    
    // Только если выбран сетевой режим — вызываем окно подключения
    if (m_networkMode) {
        QTimer::singleShot(0, this, &MainWindow::onConnectClicked);
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupUI()
{
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    m_stackedWidget = new QStackedWidget(centralWidget);
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->addWidget(m_stackedWidget);

    // Страница размещения кораблей
    m_placementPage = new QWidget();
    QVBoxLayout* placementLayout = new QVBoxLayout(m_placementPage);

    QLabel* placementLabel = new QLabel("Разместите свои корабли:");
    m_placementBoard = new GameBoard(true, this);
    m_placementBoard->setPlacementMode(true);

    m_shipSelectionGroup = new QGroupBox("Выберите тип корабля:");
    QVBoxLayout* shipSelectionLayout = new QVBoxLayout(m_shipSelectionGroup);
    m_battleshipRadio = new QRadioButton("Линкор (размер 4)");
    m_cruiserRadio = new QRadioButton("Крейсер (размер 3)");
    m_destroyerRadio = new QRadioButton("Эсминец (размер 2)");
    m_submarineRadio = new QRadioButton("Подлодка (размер 1)");
    m_battleshipRadio->setChecked(true);

    shipSelectionLayout->addWidget(m_battleshipRadio);
    shipSelectionLayout->addWidget(m_cruiserRadio);
    shipSelectionLayout->addWidget(m_destroyerRadio);
    shipSelectionLayout->addWidget(m_submarineRadio);

    m_rotateShipButton = new QPushButton("Повернуть корабль");
    m_readyButton = new QPushButton("Готово");
    m_placementStatusLabel = new QLabel();

    QHBoxLayout* placementControlsLayout = new QHBoxLayout();
    placementControlsLayout->addWidget(m_shipSelectionGroup);

    QVBoxLayout* placementButtonsLayout = new QVBoxLayout();
    placementButtonsLayout->addWidget(m_rotateShipButton);
    placementButtonsLayout->addWidget(m_readyButton);
    placementButtonsLayout->addStretch();

    placementControlsLayout->addLayout(placementButtonsLayout);

    placementLayout->addWidget(placementLabel);
    placementLayout->addWidget(m_placementBoard);
    placementLayout->addLayout(placementControlsLayout);
    placementLayout->addWidget(m_placementStatusLabel);

    // Добавляем кнопку подключения к серверу
    QPushButton* connectButton = new QPushButton("Подключиться к серверу");
    placementLayout->addWidget(connectButton);
    connect(connectButton, &QPushButton::clicked, this, &MainWindow::onConnectClicked);

    // Страница игры
    m_gamePage = new QWidget();
    QVBoxLayout* gameLayout = new QVBoxLayout(m_gamePage);

    QHBoxLayout* boardsLayout = new QHBoxLayout();

    QVBoxLayout* ownBoardLayout = new QVBoxLayout();
    QLabel* ownBoardLabel = new QLabel("Ваше поле:");
    m_ownBoard = new GameBoard(true, this);
    ownBoardLayout->addWidget(ownBoardLabel);
    ownBoardLayout->addWidget(m_ownBoard);

    QVBoxLayout* opponentBoardLayout = new QVBoxLayout();
    QLabel* opponentBoardLabel = new QLabel("Поле противника:");
    m_opponentBoard = new GameBoard(false, this);
    opponentBoardLayout->addWidget(opponentBoardLabel);
    opponentBoardLayout->addWidget(m_opponentBoard);

    boardsLayout->addLayout(ownBoardLayout);
    boardsLayout->addLayout(opponentBoardLayout);

    m_gameStatusLabel = new QLabel();

    QGroupBox* chatBox = new QGroupBox("Чат");
    QVBoxLayout* chatLayout = new QVBoxLayout(chatBox);
    m_chatDisplay = new QTextEdit();
    m_chatDisplay->setReadOnly(true);
    m_chatInput = new QLineEdit();
    m_sendChatButton = new QPushButton("Отправить");

    QHBoxLayout* chatInputLayout = new QHBoxLayout();
    chatInputLayout->addWidget(m_chatInput);
    chatInputLayout->addWidget(m_sendChatButton);

    chatLayout->addWidget(m_chatDisplay);
    chatLayout->addLayout(chatInputLayout);

    gameLayout->addLayout(boardsLayout);
    gameLayout->addWidget(m_gameStatusLabel);
    gameLayout->addWidget(chatBox);

    m_stackedWidget->addWidget(m_placementPage);
    m_stackedWidget->addWidget(m_gamePage);

    m_stackedWidget->setCurrentIndex(0);
    updateStatusMessage("Разместите свои корабли. Выберите тип корабля и кликните на поле.");

    m_placementMode = true;
}

void MainWindow::setupConnections()
{
    connect(m_battleshipRadio, SIGNAL(toggled(bool)), this, SLOT(onBattleshipRadioToggled(bool)));
    connect(m_cruiserRadio, SIGNAL(toggled(bool)), this, SLOT(onCruiserRadioToggled(bool)));
    connect(m_destroyerRadio, SIGNAL(toggled(bool)), this, SLOT(onDestroyerRadioToggled(bool)));
    connect(m_submarineRadio, SIGNAL(toggled(bool)), this, SLOT(onSubmarineRadioToggled(bool)));

    // Сигналы кнопок и игровых полей

    connect(m_rotateShipButton, &QPushButton::clicked, this, &MainWindow::onRotateShipClicked);
    connect(m_readyButton, &QPushButton::clicked, this, &MainWindow::onReadyClicked);
    connect(m_placementBoard, &GameBoard::cellClicked, this, &MainWindow::onPlacementBoardCellClicked);
    connect(m_opponentBoard, &GameBoard::cellClicked, this, &MainWindow::onOpponentBoardCellClicked);
    connect(m_sendChatButton, &QPushButton::clicked, this, &MainWindow::onSendChatClicked);

    /*
    connect(m_rotateShipButton, SIGNAL(clicked()), this, SLOT(onRotateShipClicked()));
    connect(m_readyButton, SIGNAL(clicked()), this, SLOT(onReadyClicked()));
    connect(m_placementBoard, SIGNAL(&cellClicked()), this, SLOT(&onPlacementBoardCellClicked()));
    connect(m_opponentBoard, SIGNAL(cellClicked(const QPoint& position)), this, SLOT(onOpponentBoardCellClicked(const QPoint& position)));
    connect(m_sendChatButton, SIGNAL(clicked()), this, SLOT(onSendChatClicked()));*/
}

void MainWindow::onBattleshipRadioToggled(bool checked)
{
    if (checked) {
        m_currentShipType = GameBoard::ShipSize::BATTLESHIP;
    }
}

void MainWindow::onCruiserRadioToggled(bool checked)
{
    if (checked) {
        m_currentShipType = GameBoard::ShipSize::CRUISER;
    }
}

void MainWindow::onDestroyerRadioToggled(bool checked)
{
    if (checked) {
        m_currentShipType = GameBoard::ShipSize::DESTROYER;
    }
}

void MainWindow::onSubmarineRadioToggled(bool checked)
{
    if (checked) {
        m_currentShipType = GameBoard::ShipSize::SUBMARINE;
    }
}


void MainWindow::onRotateShipClicked()
{
    m_isHorizontal = !m_isHorizontal;
    updateStatusMessage(m_isHorizontal ? "Корабль расположен горизонтально" : "Корабль расположен вертикально");
}

QString MainWindow::getShipTypeName(GameBoard::ShipSize type) const
{
    switch(type) {
    case GameBoard::ShipSize::BATTLESHIP: return "Линкор";
    case GameBoard::ShipSize::CRUISER: return "Крейсер";
    case GameBoard::ShipSize::DESTROYER: return "Эсминец";
    case GameBoard::ShipSize::SUBMARINE: return "Подлодка";
    default: return "Неизвестный";
    }
}

void MainWindow::onPlacementBoardCellClicked(const QPoint &position) {
    if (!m_placementMode) return;
    
    // Проверяем границы поля
    if (position.x() < 0 || position.x() >= BOARD_SIZE || 
        position.y() < 0 || position.y() >= BOARD_SIZE) {
        return;
    }

    // Проверяем, не превышено ли максимальное количество кораблей данного типа
    bool canPlace = false;
    switch (m_currentShipType) {
        case GameBoard::BATTLESHIP:
            if (m_placedShips.battleships < 1) canPlace = true;
            break;
        case GameBoard::CRUISER:
            if (m_placedShips.cruisers < 2) canPlace = true;
            break;
        case GameBoard::DESTROYER:
            if (m_placedShips.destroyers < 3) canPlace = true;
            break;
        case GameBoard::SUBMARINE:
            if (m_placedShips.submarines < 4) canPlace = true;
            break;
    }

    if (!canPlace) {
        updateStatusMessage("Достигнуто максимальное количество кораблей этого типа");
        return;
    }

    // Пытаемся разместить корабль выбранного типа
    if (m_placementBoard->placeShip(position, m_currentShipType, m_isHorizontal)) {
        // Увеличиваем счётчик размещённых кораблей
        switch (m_currentShipType) {
            case GameBoard::BATTLESHIP:
                m_placedShips.battleships++;
                break;
            case GameBoard::CRUISER:
                m_placedShips.cruisers++;
                break;
            case GameBoard::DESTROYER:
                m_placedShips.destroyers++;
                break;
            case GameBoard::SUBMARINE:
                m_placedShips.submarines++;
                break;
        }

        // Обновляем статус
        QString shipType = getShipTypeName(m_currentShipType);
        QString direction = m_isHorizontal ? "горизонтально" : "вертикально";
        updateStatusMessage(QString("Корабль %1 размещен %2").arg(shipType, direction));
    } else {
        updateStatusMessage("Невозможно разместить корабль в этой позиции");
    }
}

void MainWindow::onReadyClicked() {
    qDebug() << "onReadyClicked: начало метода";
    if (m_networkMode) {
        qDebug() << "onReadyClicked: сетевой режим";
        if (!m_isConnected) {
            qDebug() << "onReadyClicked: нет подключения к серверу";
            showMessage("Сначала подключитесь к серверу");
            return;
        }
        if (!m_placementBoard->isValidShipPlacement()) {
            qDebug() << "onReadyClicked: неправильное размещение кораблей";
            showMessage("Неправильное размещение кораблей");
            return;
        }
        qDebug() << "onReadyClicked: отправка доски и готовности";
        m_ownBoard->setBoard(m_placementBoard->getBoard());
        m_networkClient->sendBoard(m_placementBoard->getBoard());
        m_networkClient->sendReady();
        m_networkClient->findGame();
        switchToPage(1);
        m_gameActive = true;
        m_placementMode = false;
        qDebug() << "onReadyClicked: конец метода";
    } else {
        if (!m_placementBoard->isValidShipPlacement()) {
            showMessage("Неправильное размещение кораблей");
            return;
        }
        m_ownBoard->setBoard(m_placementBoard->getBoard());
        m_opponentBoard->placeRandomShips();
        switchToPage(1);
        m_gameActive = true;
        updateStatusMessage("Игра началась! Ваш ход!");
        m_placementMode = false;
    }
}

void MainWindow::onOpponentBoardCellClicked(const QPoint& position)
{
    qDebug() << "onOpponentBoardCellClicked: начало метода";
    qDebug() << "onOpponentBoardCellClicked: состояние игры - m_gameActive=" << m_gameActive 
             << "m_isMyTurn=" << m_isMyTurn 
             << "m_isGameStarted=" << m_isGameStarted
             << "m_networkMode=" << m_networkMode;
    
    if (!m_gameActive) {
        qDebug() << "onOpponentBoardCellClicked: игра не активна";
        return;
    }
    
    if (!m_isGameStarted) {
        qDebug() << "onOpponentBoardCellClicked: игра еще не началась";
        return;
    }
    
    if (m_networkMode && !m_isMyTurn) {
        qDebug() << "onOpponentBoardCellClicked: не ваш ход";
        return;
    }
    
    if (m_networkMode) {
        qDebug() << "onOpponentBoardCellClicked: сетевой режим, проверяем возможность выстрела";
        GameBoard::CellState cellState = m_opponentBoard->checkShot(position);
        qDebug() << "onOpponentBoardCellClicked: состояние клетки =" << cellState;
        
        if (cellState == GameBoard::EMPTY) {
            qDebug() << "onOpponentBoardCellClicked: отправка выстрела:" << position;
            m_networkClient->sendShot(position.x(), position.y());
            m_opponentBoard->setEnabled(false); // Блокируем поле до получения результата
        } else {
            qDebug() << "onOpponentBoardCellClicked: выстрел невозможен, клетка уже занята";
        }
    } else {
        GameBoard::CellState result = m_opponentBoard->checkShot(position);
        if (result == GameBoard::EMPTY) {
            m_opponentBoard->markShot(position, GameBoard::MISS);
            m_myTurn = false;
            updateStatusMessage("Промах! Ход компьютера.");
            QTimer::singleShot(1000, this, &MainWindow::opponentMove);
        } else if (result == GameBoard::SHIP) {
            m_opponentBoard->markShot(position, GameBoard::HIT);
            updateStatusMessage("Попадание! Стреляйте снова!");
            
            if (m_opponentBoard->allShipsSunk()) {
                m_gameActive = false;
                showMessage("Вы победили!");
            }
        }
    }
}

void MainWindow::opponentMove()
{
    if (!m_networkMode && m_gameActive && !m_myTurn) {
        QPoint aiMove = getNextAIMove();
        if (aiMove.x() >= 0 && aiMove.y() >= 0) {
            GameBoard::CellState result = m_ownBoard->checkShot(aiMove);
            if (result == GameBoard::SHIP) {
                m_ownBoard->markShot(aiMove, GameBoard::HIT);
                m_aiFoundShip = true;
                m_lastAIMove = aiMove;
                updateStatusMessage("Компьютер попал в ваш корабль!");
                QTimer::singleShot(1000, this, &MainWindow::opponentMove);
            } else {
                m_ownBoard->markShot(aiMove, GameBoard::MISS);
                m_myTurn = true;
                updateStatusMessage("Компьютер промахнулся. Ваш ход!");
            }
            
            if (m_ownBoard->allShipsSunk()) {
                m_gameActive = false;
                showMessage("Компьютер победил!");
            }
        }
    }
}

void MainWindow::onSendChatClicked() {
    QString message = m_chatInput->text().trimmed();
    if (!message.isEmpty()) {
        addChatMessage(m_username, message);
        m_networkClient->sendChatMessage(message);
        m_chatInput->clear();
    }
}

void MainWindow::onChatMessageReceived(const QString &sender, const QString &message) {
    addChatMessage(sender, message);
}

void MainWindow::switchToPage(int page)
{
    m_stackedWidget->setCurrentIndex(page);
}

void MainWindow::resetGame()
{
    m_gameActive = false;
    m_myTurn = true;
    m_aiFoundShip = false;
    m_aiPossibleMoves.clear();
    m_placementBoard->reset();
    m_ownBoard->reset();
    m_opponentBoard->reset();
    m_chatDisplay->clear();
    
    // Сбрасываем счётчики размещённых кораблей
    m_placedShips = ShipCount();
    
    switchToPage(0);
    updateStatusMessage("Разместите свои корабли. Выберите тип корабля и кликните на поле.");
}

void MainWindow::updateStatusMessage(const QString& message)
{
    if (m_stackedWidget->currentIndex() == 0) {
        m_placementStatusLabel->setText(message);
    } else {
        m_gameStatusLabel->setText(message);
    }
}

void MainWindow::addChatMessage(const QString& sender, const QString& message)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    m_chatDisplay->append(QString("[%1] %2: %3").arg(timestamp, sender, message));
}

QPoint MainWindow::getNextAIMove()
{
    if (!m_aiFoundShip) {
        // Если корабль не найден, стреляем случайно
        QVector<QPoint> possibleMoves;
        for (int x = 0; x < 10; ++x) {
            for (int y = 0; y < 10; ++y) {
                QPoint pos(x, y);
                if (m_ownBoard->checkShot(pos) == GameBoard::EMPTY) {
                    possibleMoves.append(pos);
                }
            }
        }
        if (!possibleMoves.isEmpty()) {
            int index = QRandomGenerator::global()->bounded(possibleMoves.size());
            return possibleMoves[index];
        }
    } else {
        // Если корабль найден, стреляем вокруг последнего попадания
        QVector<QPoint> directions = {
            QPoint(1, 0), QPoint(-1, 0), QPoint(0, 1), QPoint(0, -1)
        };
        
        for (const QPoint& dir : directions) {
            QPoint nextPos = m_lastAIMove + dir;
            if (nextPos.x() >= 0 && nextPos.x() < 10 && 
                nextPos.y() >= 0 && nextPos.y() < 10 &&
                m_ownBoard->checkShot(nextPos) == GameBoard::EMPTY) {
                return nextPos;
            }
        }
        
        // Если нет доступных ходов вокруг, сбрасываем поиск
        m_aiFoundShip = false;
        return getNextAIMove();
    }
    
    return QPoint(-1, -1); // Не должно произойти
}

void MainWindow::onConnectClicked()
{
    bool ok;
    QString host = QInputDialog::getText(this, "Подключение к серверу",
                                       "Введите IP-адрес сервера:", QLineEdit::Normal,
                                       "127.0.0.1", &ok);
    if (!ok) return;

    int port = QInputDialog::getInt(this, "Подключение к серверу",
                                  "Введите порт сервера:", 12345, 1, 65535, 1, &ok);
    if (!ok) return;

    QString username = QInputDialog::getText(this, "Вход",
                                           "Введите ваше имя:", QLineEdit::Normal,
                                           "", &ok);
    if (!ok) return;

    m_pendingUsername = username;
    m_networkClient->connectToServer(host, port);
}

void MainWindow::onConnected()
{
    m_isConnected = true;
    addChatMessage("Система", "Подключено к серверу");
    updateStatusMessage("Подключено к серверу");
    if (!m_pendingUsername.isEmpty()) {
        m_networkClient->login(m_pendingUsername);
        m_pendingUsername.clear();
    }
}

void MainWindow::onDisconnected()
{
    m_isConnected = false;
    addChatMessage("Система", "Отключено от сервера");
    updateStatusMessage("Отключено от сервера");
}

void MainWindow::onNetworkError(const QString &error)
{
    addChatMessage("Система", "Ошибка: " + error);
    updateStatusMessage("Ошибка: " + error);
}

void MainWindow::onLoginResponse(bool success)
{
    if (success) {
        addChatMessage("Система", "Успешный вход");
        updateStatusMessage("Успешный вход. Разместите корабли и нажмите 'Готово'");
    } else {
        addChatMessage("Система", "Ошибка входа");
        updateStatusMessage("Ошибка входа");
    }
}

void MainWindow::onGameFound(const QString &opponent) {
    qDebug() << "onGameFound: игра найдена с противником" << opponent;
    qDebug() << "onGameFound: текущее состояние - m_isGameStarted=" << m_isGameStarted 
             << "m_gameActive=" << m_gameActive 
             << "m_isMyTurn=" << m_isMyTurn;
    
    updateStatusMessage("Игра найдена! Противник: " + opponent);
    addChatMessage("Система", "Игра найдена! Противник: " + opponent);
    
    m_placementBoard->setEnabled(false);
    m_isGameStarted = true;
    m_gameActive = true;
    m_networkMode = true;
    m_placementMode = false;
    
    // Доска противника должна быть кликабельна только если это наш ход
    m_opponentBoard->setEnabled(m_isMyTurn);
    
    qDebug() << "onGameFound: новое состояние - m_isGameStarted=" << m_isGameStarted 
             << "m_gameActive=" << m_gameActive 
             << "m_isMyTurn=" << m_isMyTurn;
}

void MainWindow::onWaitingForOpponent() {
    updateStatusMessage("Ожидание противника...");
    addChatMessage("Система", "Ожидание противника...");
    m_placementBoard->setEnabled(false);
    m_opponentBoard->setEnabled(false);
}

void MainWindow::onShotResult(int x, int y, bool hit)
{
    QString result = hit ? "попал" : "промахнулся";
    addChatMessage("Система", QString("Выстрел в (%1,%2): %3").arg(x).arg(y).arg(result));
    
    QPoint position(x, y);
    if (hit) {
        m_opponentBoard->markHit(position);
    } else {
        m_opponentBoard->markMiss(position);
    }

    // Отправляем результат выстрела
    m_networkClient->sendShotResult(x, y, hit);
}

void MainWindow::onGameOver(const QString &winner)
{
    QString message = winner == m_networkClient->username() ? "Вы победили!" : "Вы проиграли!";
    addChatMessage("Система", message);
    updateStatusMessage(message);
    QMessageBox::information(this, "Игра окончена", message);
}

void MainWindow::onOpponentBoardClicked(const QPoint& position) {
    if (!m_isGameStarted || !m_myTurn) return;
    
    if (m_opponentBoard->checkShot(position) == GameBoard::EMPTY) {
        m_networkClient->sendShot(position.x(), position.y());
    }
}

void MainWindow::showMessage(const QString &message) {
    ui->statusbar->showMessage(message);
}

void MainWindow::updateStatus() {
    qDebug() << "updateStatus: m_isGameStarted=" << m_isGameStarted << "m_isMyTurn=" << m_isMyTurn;
    if (!m_isGameStarted) {
        m_gameStatusLabel->setText("Ожидание начала игры...");
        return;
    }

    if (m_isMyTurn) {
        m_gameStatusLabel->setText("Ваш ход! Выберите клетку на поле противника.");
    } else {
        m_gameStatusLabel->setText("Ход противника. Ожидайте...");
    }
}

void MainWindow::onClearBoardClicked() {
    if (m_placementBoard) m_placementBoard->clearBoard();
}

void MainWindow::onRandomPlacementClicked() {
    if (m_placementBoard) m_placementBoard->placeRandomShips();
}

void MainWindow::onFindGameClicked() {
    if (m_networkClient) m_networkClient->findGame();
}

void MainWindow::onLoginClicked() {
    // Можно реализовать вызов окна логина или оставить пустым, если не требуется
}

void MainWindow::onGameModeChanged(bool networkMode) {
    m_networkMode = networkMode;
    // Можно добавить дополнительную логику при смене режима
}

void MainWindow::onShotReceived(int x, int y)
{
    qDebug() << "Получен выстрел от противника:" << x << y;
    QPoint position(x, y);

    // Проверяем попадание
    GameBoard::CellState result = m_ownBoard->checkShot(position);
    bool hit = (result == GameBoard::SHIP);

    // Отправляем результат выстрела
    m_networkClient->sendShotResult(x, y, hit);

    // Обновляем доску
    m_ownBoard->markShot(position, hit ? GameBoard::HIT : GameBoard::MISS);

    // Проверяем победу
    if (m_ownBoard->allShipsSunk()) {
        onGameOver(m_networkClient->username());
    }
}

void MainWindow::onTurnChanged(bool isMyTurn)
{
    qDebug() << "onTurnChanged: получен сигнал turnChanged, isMyTurn=" << isMyTurn;
    qDebug() << "onTurnChanged: текущее состояние - m_gameActive=" << m_gameActive 
             << "m_isGameStarted=" << m_isGameStarted 
             << "m_isMyTurn=" << m_isMyTurn;
    
    m_isMyTurn = isMyTurn;
    m_gameActive = true;
    m_isGameStarted = true;
    
    // Обновляем состояние поля противника
    m_opponentBoard->setEnabled(isMyTurn);
    
    // Обновляем статус игры
    if (isMyTurn) {
        updateStatusMessage("Ваш ход! Выберите клетку на поле противника.");
    } else {
        updateStatusMessage("Ход противника. Ожидайте...");
    }
    
    qDebug() << "onTurnChanged: новое состояние - m_gameActive=" << m_gameActive 
             << "m_isGameStarted=" << m_isGameStarted 
             << "m_isMyTurn=" << m_isMyTurn;
}
