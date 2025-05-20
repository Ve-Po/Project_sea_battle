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
#include <algorithm>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <random>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
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
    m_placementMode(false),
    m_placedShips(),  // Используем конструктор по умолчанию
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
    qDebug() << "mode: " << mode;
    m_networkMode = (mode == "Играть по сети");
    qDebug() << "m_networkMode: " << m_networkMode;
    m_aiFoundShip = false;
    m_lastAIMove = QPoint(-1, -1);
    m_activeHits.clear();

    // Инициализируем NetworkClient только если выбран сетевой режим
    if (m_networkMode) {
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
    connect(m_networkClient, &NetworkClient::gameStartConfirmed, this, &MainWindow::onGameStartConfirmed);
    connect(m_networkClient, &NetworkClient::waitingForOpponent, this, &MainWindow::onWaitingForOpponent);
    } else {
        m_networkClient = nullptr;
    }
    
    setupUI();
    setupConnections();
    setWindowTitle("Морской бой");
    resize(800, 600);
    
    // Инициализация переменной для отслеживания размещенных кораблей
    m_placedShips = ShipCount();
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

    // Добавляем кнопку подключения к серверу только в сетевом режиме
    if (m_networkMode) {
    QPushButton* connectButton = new QPushButton("Подключиться к серверу");
    placementLayout->addWidget(connectButton);
    connect(connectButton, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    }

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
    // Используем последовательно новый стиль соединений
    connect(m_battleshipRadio, &QRadioButton::toggled, this, &MainWindow::onBattleshipRadioToggled);
    connect(m_cruiserRadio, &QRadioButton::toggled, this, &MainWindow::onCruiserRadioToggled);
    connect(m_destroyerRadio, &QRadioButton::toggled, this, &MainWindow::onDestroyerRadioToggled);
    connect(m_submarineRadio, &QRadioButton::toggled, this, &MainWindow::onSubmarineRadioToggled);
    connect(m_opponentBoard, &GameBoard::cellClicked, this, &MainWindow::onOpponentBoardCellClicked);
    connect(m_placementBoard, &GameBoard::cellClicked, this, &MainWindow::onPlacementBoardCellClicked);


    // Соединяем сигналы кнопок и игровых полей
    connect(m_rotateShipButton, &QPushButton::clicked, this, &MainWindow::onRotateShipClicked);
    connect(m_readyButton, &QPushButton::clicked, this, &MainWindow::onReadyClicked);
    connect(m_placementBoard, &GameBoard::cellClicked, this, &MainWindow::onPlacementBoardCellClicked);
    connect(m_opponentBoard, &GameBoard::cellClicked, this, &MainWindow::onOpponentBoardCellClicked);
    connect(m_sendChatButton, &QPushButton::clicked, this, &MainWindow::onSendChatClicked);
    connect(m_chatInput, &QLineEdit::returnPressed, this, &MainWindow::onSendChatClicked);
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

void MainWindow::onPlacementBoardCellClicked(const QPoint& position)
{
    if (!m_placementMode) return;

    // Проверяем, не превышено ли максимальное количество кораблей данного типа
    bool canPlace = false;
    switch (m_currentShipType) {
        case GameBoard::ShipSize::BATTLESHIP:
            if (m_placedShips.battleships < 1) canPlace = true;
            break;
        case GameBoard::ShipSize::CRUISER:
            if (m_placedShips.cruisers < 2) canPlace = true;
            break;
        case GameBoard::ShipSize::DESTROYER:
            if (m_placedShips.destroyers < 3) canPlace = true;
            break;
        case GameBoard::ShipSize::SUBMARINE:
            if (m_placedShips.submarines < 4) canPlace = true;
            break;
    }
    if (!canPlace) {
        updateStatusMessage("Достигнуто максимальное количество кораблей этого типа");
        return;
    }

    if (m_placementBoard->placeShip(position, m_currentShipType, m_isHorizontal)) {
        // Увеличиваем счетчик размещенных кораблей
        switch (m_currentShipType) {
            case GameBoard::ShipSize::BATTLESHIP:
                m_placedShips.battleships++;
                break;
            case GameBoard::ShipSize::CRUISER:
                m_placedShips.cruisers++;
                break;
            case GameBoard::ShipSize::DESTROYER:
                m_placedShips.destroyers++;
                break;
            case GameBoard::ShipSize::SUBMARINE:
                m_placedShips.submarines++;
                break;
        }

        // Проверяем, все ли корабли размещены
        bool allShipsPlaced = m_placedShips.battleships == 1 &&
                            m_placedShips.cruisers == 2 &&
                            m_placedShips.destroyers == 3 &&
                            m_placedShips.submarines == 4;

        if (allShipsPlaced) {
            m_readyButton->setEnabled(true);
            updateStatusMessage("Все корабли размещены. Нажмите 'Готово' для начала игры.");
        } else {
            updateStatusMessage("Корабль размещен. Разместите следующий корабль.");
        }
        updateShipSelectionUI();
    } else {
        updateStatusMessage("Невозможно разместить корабль в этой позиции.");
    }
}

void MainWindow::onOpponentBoardCellClicked(const QPoint& position) {
     if (!m_isGameStarted || !m_isMyTurn) {
        qDebug() << "Клик заблокирован: игра не активна или не ваш ход";
        return;
    }
    
    // Проверяем, что клетка еще не атакована
    GameBoard::CellState currentState = m_opponentBoard->getCellState(position);
    if (currentState != GameBoard::CellState::EMPTY && 
        currentState != GameBoard::CellState::SHIP) { // Разрешаем клик по клеткам с кораблями
        qDebug() << "Клетка уже атакована:" << position;
        return;
    }
    
    // Логика выстрела
    bool hit = m_opponentBoard->makeShot(position);
    qDebug() << "Выстрел в" << position << "-" << (hit ? "Попадание!" : "Промах!");

    // Обновляем состояние клетки
    m_opponentBoard->setCellState(position, 
        hit ? GameBoard::CellState::HIT : GameBoard::CellState::MISS);
        
    if (hit) {
        if (m_opponentBoard->isShipSunk(position)) {
            m_opponentBoard->markSunkShip(position);
            if (checkOpponentBoardForWin()) {
                QMessageBox::information(this, "Победа!", "Вы победили!");
                resetGame();
                return;
            }
        }
        updateStatusMessage("Попадание! Стреляйте снова.");
    } else {
        updateStatusMessage("Мимо! Ход противника.");
        m_isMyTurn = false;
        if (!m_networkMode) QTimer::singleShot(1000, this, &MainWindow::opponentMove);
    }
}

// Новый вспомогательный метод для проверки победы
bool MainWindow::checkOpponentBoardForWin()
{
    for (int i = 0; i < GameBoard::GRID_SIZE; ++i) {
        for (int j = 0; j < GameBoard::GRID_SIZE; ++j) {
            // Если есть хоть одна клетка с кораблем, которая не подбита, игра продолжается
            if (m_opponentBoard->getBoard()[i][j] == 1 && 
                m_opponentBoard->getCellState(QPoint(i, j)) != GameBoard::CellState::HIT &&
                m_opponentBoard->getCellState(QPoint(i, j)) != GameBoard::CellState::SUNK) {
                return false;
            }
        }
    }
    return true; // Все корабли подбиты, игрок победил
}

void MainWindow::opponentMove() {
    if (m_isMyTurn || !m_isGameStarted) return;

    QPoint move = getNextAIMove();
    if (move == QPoint(-1, -1)) return;

    bool hit = m_ownBoard->makeShot(move);
    m_ownBoard->setCellState(move, hit ? GameBoard::CellState::HIT : GameBoard::CellState::MISS);

    if (hit) {
        if (m_ownBoard->isShipSunk(move)) {
            m_ownBoard->markSunkShip(move);
            if (m_ownBoard->allShipsSunk()) {
                QMessageBox::information(this, "Поражение", "Все ваши корабли потоплены!");
                resetGame();
                return;
            }
                QTimer::singleShot(1000, this, &MainWindow::opponentMove);
            } else {
            QTimer::singleShot(1000, this, &MainWindow::opponentMove);
        }
    } else {
        m_isMyTurn = true;
        updateStatusMessage("Ваш ход! Выберите клетку.");
    }
}
QPoint MainWindow::getNextAIMove() {
    // Собираем все клетки с попаданиями, которые ещё не привели к потоплению корабля
    QVector<QPoint> activeHits;
    for (int y = 0; y < GameBoard::GRID_SIZE; ++y) {
        for (int x = 0; x < GameBoard::GRID_SIZE; ++x) {
            QPoint pos(x, y);
            if (m_ownBoard->getCellState(pos) == GameBoard::CellState::HIT && !m_ownBoard->isShipSunk(pos)) {
                activeHits.append(pos);
            }
        }
    }
    // Если есть такие попадания — добиваем корабль
    if (!activeHits.isEmpty()) {
        // Пробуем добивать по всем направлениям от каждого попадания
        for (const QPoint& hit : activeHits) {
            QVector<QPoint> directions = { {-1,0}, {1,0}, {0,-1}, {0,1} };
            for (const QPoint& dir : directions) {
                QPoint next = hit + dir;
                if (next.x() >= 0 && next.x() < GameBoard::GRID_SIZE &&
                    next.y() >= 0 && next.y() < GameBoard::GRID_SIZE) {
                    auto state = m_ownBoard->getCellState(next);
                    if (state == GameBoard::CellState::EMPTY || state == GameBoard::CellState::SHIP) {
                        return next;
                    }
                }
            }
        }
    }
    // Если нет попаданий — стреляем случайно по клеткам, где нет выстрелов
    QVector<QPoint> availableCells;
    for (int y = 0; y < GameBoard::GRID_SIZE; ++y) {
        for (int x = 0; x < GameBoard::GRID_SIZE; ++x) {
            QPoint pos(x, y);
            auto state = m_ownBoard->getCellState(pos);
            if (state == GameBoard::CellState::EMPTY || state == GameBoard::CellState::SHIP) {
                availableCells.append(pos);
            }
        }
    }
    if (!availableCells.isEmpty()) {
        return availableCells[QRandomGenerator::global()->bounded(availableCells.size())];
    }
    return QPoint(-1, -1);
}
void MainWindow::onSendChatClicked() {
    QString message = m_chatInput->text().trimmed();
    if (!message.isEmpty()) {
        if (m_networkMode && m_networkClient) {
        addChatMessage(m_username, message);
        m_networkClient->sendChatMessage(message);
        } else {
            addChatMessage("Вы", message);
        }
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
    m_isGameStarted = false;
    m_gameActive = false;
    m_isMyTurn = false;
    m_placementMode = true;
    m_aiFoundShip = false;
    m_aiPossibleMoves.clear();
    m_activeHits.clear();
    
    // Сбрасываем счетчики кораблей
    m_placedShips = ShipCount();  // Используем конструктор по умолчанию
    
    // Очищаем доски
    if (m_placementBoard) m_placementBoard->clear();
    if (m_ownBoard) m_ownBoard->clear();
    if (m_opponentBoard) m_opponentBoard->clear();
    
    // Сбрасываем UI выбора кораблей
    if (m_battleshipRadio) {
        m_battleshipRadio->setChecked(true);
        m_battleshipRadio->setEnabled(true);
        m_currentShipType = GameBoard::ShipSize::BATTLESHIP;
    }
    if (m_cruiserRadio) m_cruiserRadio->setEnabled(true);
    if (m_destroyerRadio) m_destroyerRadio->setEnabled(true);
    if (m_submarineRadio) m_submarineRadio->setEnabled(true);
    
    // Возвращаемся на страницу размещения
    switchToPage(0);
    updateStatusMessage("Разместите свои корабли. Выберите тип корабля и кликните на поле.");
    updateShipSelectionUI();  // Обновляем UI после сброса
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

void MainWindow::onConnectClicked()
{
    if (!m_networkMode || !m_networkClient) {
        return;
            }

    bool ok;
    QString host = QInputDialog::getText(this, "Подключение к серверу",
                                       "Введите адрес сервера:", QLineEdit::Normal,
                                       "localhost", &ok);
    if (!ok || host.isEmpty()) {
        return;
    }

    quint16 port = QInputDialog::getInt(this, "Подключение к серверу",
                                      "Введите порт сервера:", 1234, 1, 65535, 1, &ok);
    if (!ok) {
        return;
    }

    QString username = QInputDialog::getText(this, "Вход",
                                           "Введите ваше имя:", QLineEdit::Normal,
                                           "", &ok);
    if (!ok || username.isEmpty()) {
        return;
    }

    m_pendingUsername = username;
    m_networkClient->connectToServer(host, port);
}

void MainWindow::onConnected()
{
    if (m_networkClient && !m_pendingUsername.isEmpty()) {
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
        if (m_networkMode && m_networkClient) {
            m_networkClient->findGame();
        }
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
    
    m_placementBoard->setEnabled(true);
    m_isGameStarted = false;
    m_gameActive = false;
    m_networkMode = true;
    m_placementMode = true;
    
    m_opponentBoard->setEnabled(false);
    
    qDebug() << "onGameFound: новое состояние - m_isGameStarted=" << m_isGameStarted 
             << "m_gameActive=" << m_gameActive 
             << "m_isMyTurn=" << m_isMyTurn;
}

void MainWindow::onWaitingForOpponent() {
    updateStatusMessage("Ожидание противника...");
    addChatMessage("Система", "Ожидание противника...");
    m_opponentBoard->setEnabled(false);
}

void MainWindow::onShotResult(int x, int y, bool hit)
{
    QPoint position(x, y);
    if (hit) {
        m_opponentBoard->markHit(position);
        if (m_opponentBoard->isShipSunk(position)) {
            m_opponentBoard->markSunk(position);
        }
    } else {
        m_opponentBoard->markMiss(position);
    }
    
    // Проверяем, не закончилась ли игра
    if (m_opponentBoard->allShipsSunk()) {
        QMessageBox::information(this, "Победа!", "Вы победили!");
        m_gameActive = false;
        return;
    }
    
    // Если попали, ход остается за текущим игроком
    if (!hit) {
        m_isMyTurn = false;
        updateStatusMessage("Ход противника");
    } else {
        updateStatusMessage("Вы попали! Сделайте еще один выстрел");
    }
    
    // Обновляем состояние кнопок и доступность поля
    updateGameState();
}

void MainWindow::updateGameState()
{
    // Поле противника доступно только во время хода игрока
    m_opponentBoard->setEnabled(m_isMyTurn && m_gameActive);
    
    // Обновляем статус игры
    if (!m_gameActive) {
        updateStatusMessage("Игра не активна");
    } else if (m_isMyTurn) {
        updateStatusMessage("Ваш ход");
    } else {
        updateStatusMessage("Ход противника");
    }
}

void MainWindow::onGameOver(bool youWin) {
    QString message = youWin ? "Вы победили!" : "Вы проиграли!";
    addChatMessage("Система", message);
    updateStatusMessage(message);
    QMessageBox::information(this, "Игра окончена", message);
}
void MainWindow::onOpponentBoardClicked(const QPoint& pos)
{
    if (!m_isGameStarted || !m_isMyTurn) return;
    
    if (m_opponentBoard->checkShot(pos) == GameBoard::CellState::EMPTY) {
        m_networkClient->sendShot(pos.x(), pos.y());
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

void MainWindow::onRandomPlacementClicked() {
    if (m_placementBoard) m_placementBoard->placeRandomShips(100); // Using 100 as max attempts
}

void MainWindow::onGameModeChanged(bool networkMode) {
    m_networkMode = networkMode;
    // Можно добавить дополнительную логику при смене режима
}

void MainWindow::onShotReceived(int x, int y)
{
    QPoint position(x, y);
    qDebug() << "Получен выстрел от противника в (" << x << "," << y << ")";
    
    // Проверяем попадание
    bool hit = m_ownBoard->makeShot(position);
    
    // Отправляем результат выстрела
    if (m_networkClient) {
        m_networkClient->sendShotResult(x, y, hit);
    }
    
    // Обновляем доску
    m_ownBoard->setCellState(position, 
        hit ? GameBoard::CellState::HIT : GameBoard::CellState::MISS);
    
    // Если попали, проверяем, не потоплен ли корабль
    if (hit && m_ownBoard->isShipSunk(position)) {
        m_ownBoard->markSunkShip(position);
        if (m_networkClient) {
            m_networkClient->sendShipSunk(x, y);
        }
    }
    
    // Проверяем, не закончилась ли игра
    if (m_ownBoard->allShipsSunk()) {
        if (m_networkClient) {
            m_networkClient->gameOver(false); // false = мы проиграли
        }
        QMessageBox::information(this, "Игра окончена", "Все ваши корабли потоплены!");
        resetGame();
    }
}
void MainWindow::onTurnChanged(bool isMyTurn)
{
    m_isMyTurn = isMyTurn;
    m_opponentBoard->setEnabled(isMyTurn);
    
    if (isMyTurn) {
        updateStatusMessage("Ваш ход. Выберите клетку на поле противника.");
    } else {
        updateStatusMessage("Ход противника. Ожидайте...");
    }
    
    // Обновляем состояние кнопок и полей
    m_opponentBoard->setPlacementMode(false);
    m_opponentBoard->setEnabled(isMyTurn);
    m_ownBoard->setEnabled(false);
}

void MainWindow::addAdjacentCells(const QPoint& pos) {
    const QList<QPair<int, int>> directions = {{-1,0}, {1,0}, {0,-1}, {0,1}};
    for (const auto& dir : directions) {
        QPoint p(pos.x() + dir.first, pos.y() + dir.second);
        if (p.x() >= 0 && p.x() < GameBoard::GRID_SIZE && 
            p.y() >= 0 && p.y() < GameBoard::GRID_SIZE && 
            (m_ownBoard->getCellState(p) == GameBoard::CellState::EMPTY || 
             m_ownBoard->getCellState(p) == GameBoard::CellState::SHIP)) {
            if (!m_aiPossibleMoves.contains(p)) {
                m_aiPossibleMoves.append(p);
            }
        }
    }
}
bool MainWindow::checkShipSunk(const QPoint& position) {
    return m_ownBoard->isShipSunk(position);
}

// Добавляем вспомогательные методы
int MainWindow::getCurrentShipCount() const {
    switch (m_currentShipType) {
        case GameBoard::ShipSize::BATTLESHIP: return m_placedShips.battleships;
        case GameBoard::ShipSize::CRUISER: return m_placedShips.cruisers;
        case GameBoard::ShipSize::DESTROYER: return m_placedShips.destroyers;
        case GameBoard::ShipSize::SUBMARINE: return m_placedShips.submarines;
        default: return 0;
    }
}

void MainWindow::selectNextAvailableShipType() {
    if (m_placedShips.battleships < 1) {
        m_battleshipRadio->setChecked(true);
        m_currentShipType = GameBoard::ShipSize::BATTLESHIP;
    } else if (m_placedShips.cruisers < 2) {
        m_cruiserRadio->setChecked(true);
        m_currentShipType = GameBoard::ShipSize::CRUISER;
    } else if (m_placedShips.destroyers < 3) {
        m_destroyerRadio->setChecked(true);
        m_currentShipType = GameBoard::ShipSize::DESTROYER;
    } else if (m_placedShips.submarines < 4) {
        m_submarineRadio->setChecked(true);
        m_currentShipType = GameBoard::ShipSize::SUBMARINE;
    }
}

void MainWindow::makeAIMove()
{
    if (!m_isMyTurn || m_gameOver) return;

    QPoint target;
    if (m_aiFoundShip) {
        // Если нашли корабль, стреляем вокруг него
        QVector<QPoint> possibleTargets;
        for (int y = 0; y < GameBoard::GRID_SIZE; ++y) {
            for (int x = 0; x < GameBoard::GRID_SIZE; ++x) {
                QPoint pos(x, y);
                if (m_opponentBoard->getCellState(pos) == GameBoard::CellState::HIT) {
                    // Проверяем соседние клетки
                    QVector<QPoint> neighbors = {
                        QPoint(x+1, y), QPoint(x-1, y),
                        QPoint(x, y+1), QPoint(x, y-1)
                    };
                    for (const QPoint& neighbor : neighbors) {
                        if (neighbor.x() >= 0 && neighbor.x() < GameBoard::GRID_SIZE &&
                            neighbor.y() >= 0 && neighbor.y() < GameBoard::GRID_SIZE &&
                            m_opponentBoard->getCellState(neighbor) == GameBoard::CellState::EMPTY) {
                            possibleTargets.append(neighbor);
                        }
                    }
                }
            }
        }
        
        if (!possibleTargets.isEmpty()) {
            target = possibleTargets[QRandomGenerator::global()->bounded(possibleTargets.size())];
        } else {
            // Если не нашли подходящих целей, стреляем в случайную пустую клетку
            do {
                target = QPoint(
                    QRandomGenerator::global()->bounded(GameBoard::GRID_SIZE),
                    QRandomGenerator::global()->bounded(GameBoard::GRID_SIZE)
                );
            } while (m_opponentBoard->getCellState(target) != GameBoard::CellState::EMPTY);
            m_aiFoundShip = false;
        }
    } else {
        // Если не нашли корабль, стреляем в случайную пустую клетку
        do {
            target = QPoint(
                QRandomGenerator::global()->bounded(GameBoard::GRID_SIZE),
                QRandomGenerator::global()->bounded(GameBoard::GRID_SIZE)
            );
        } while (m_opponentBoard->getCellState(target) != GameBoard::CellState::EMPTY);
    }

    // Делаем выстрел
    if (m_opponentBoard->makeShot(target)) {
        m_aiFoundShip = true;
        if (m_opponentBoard->isShipSunk(target)) {
            m_aiFoundShip = false;
        }
        m_isMyTurn = false;
    } else {
        m_isMyTurn = false;
    }

    updateGameStatus();
}

void MainWindow::updateGameStatus()
{
    if (!m_gameActive) return;

    QString status;
    if (m_networkMode) {
        status = m_isMyTurn ? "Ваш ход" : "Ход противника";
    } else {
        status = m_isMyTurn ? "Ваш ход" : "Ход компьютера";
    }

    if (m_gameOver) {
        status = m_isMyTurn ? "Вы проиграли!" : "Вы победили!";
    }

    m_gameStatusLabel->setText(status);
}

void MainWindow::onDisconnectClicked()
{
    if (m_networkClient) {
        m_networkClient->disconnect();
    }
    m_isConnected = false;
    updateStatus();
}

void MainWindow::onNetworkModeToggled(bool checked)
{
    m_networkMode = checked;
    if (checked) {
        m_networkClient = new NetworkClient(this);
        setupNetworkConnections();
    } else {
        if (m_networkClient) {
            m_networkClient->deleteLater();
            m_networkClient = nullptr;
        }
    }
    updateStatus();
}

void MainWindow::onBattleshipClicked()
{
    m_currentShipType = GameBoard::ShipSize::BATTLESHIP;
    updateShipSelectionUI();
}

void MainWindow::onCruiserClicked()
{
    m_currentShipType = GameBoard::ShipSize::CRUISER;
    updateShipSelectionUI();
}

void MainWindow::onDestroyerClicked()
{
    m_currentShipType = GameBoard::ShipSize::DESTROYER;
    updateShipSelectionUI();
}

void MainWindow::onSubmarineClicked()
{
    m_currentShipType = GameBoard::ShipSize::SUBMARINE;
    updateShipSelectionUI();
}

void MainWindow::onHorizontalToggled(bool checked)
{
    m_isHorizontal = checked;
}

void MainWindow::onStartGameClicked()
{
    if (getCurrentShipCount() != 10) {
        QMessageBox::warning(this, "Ошибка", "Разместите все корабли перед началом игры!");
        return;
    }
    startGame();
}

void MainWindow::onCellClicked(const QPoint& position)
{
    if (!m_gameActive || !m_isMyTurn || m_gameOver) return;

    if (m_networkMode) {
        if (m_networkClient && m_isConnected) {
            m_networkClient->sendShot(position.x(), position.y());
        }
    } else {
        handleShot(position);
    }
}

void MainWindow::setupNetworkConnections()
{
    if (!m_networkClient) return;

    connect(m_networkClient, &NetworkClient::connected, this, &MainWindow::onConnected);
    connect(m_networkClient, &NetworkClient::disconnected, this, &MainWindow::onDisconnected);
    connect(m_networkClient, &NetworkClient::error, this, &MainWindow::onNetworkError);
    connect(m_networkClient, &NetworkClient::loginResponse, this, &MainWindow::onLoginResponse);
    connect(m_networkClient, &NetworkClient::gameFound, this, &MainWindow::onGameFound);
    connect(m_networkClient, &NetworkClient::waitingForOpponent, this, &MainWindow::onWaitingForOpponent);
    connect(m_networkClient, &NetworkClient::shotResult, this, &MainWindow::onShotResult);
    connect(m_networkClient, &NetworkClient::gameOver, this, &MainWindow::onGameOver);
    connect(m_networkClient, &NetworkClient::shotReceived, this, &MainWindow::onShotReceived);
    connect(m_networkClient, &NetworkClient::turnChanged, this, &MainWindow::onTurnChanged);
    connect(m_networkClient, &NetworkClient::chatMessageReceived, this, &MainWindow::onChatMessageReceived);
    connect(m_networkClient, &NetworkClient::gameStartConfirmed, this, &MainWindow::onGameStartConfirmed);
}

void MainWindow::startGame()
{
    m_gameActive = true;
    m_gameOver = false;
    m_placementMode = false;
    m_isGameStarted = true;

    // Скрываем доску размещения и показываем игровую доску
    m_stackedWidget->setCurrentWidget(m_gamePage);
    updateGameStatus();
}

void MainWindow::handleShot(const QPoint& position)
{
    if (!m_gameActive || !m_isMyTurn || m_gameOver) return;

    if (m_opponentBoard->makeShot(position)) {
        // Попадание
        if (m_opponentBoard->isShipSunk(position)) {
            // Корабль потоплен
            handleShipSunk(position);
        }
        m_isMyTurn = true;
    } else {
        // Промах
        m_isMyTurn = false;
        if (!m_networkMode) {
            QTimer::singleShot(1000, this, &MainWindow::makeAIMove);
        }
    }

    updateGameStatus();
    checkGameOver();
}

void MainWindow::onPlacementCellClicked(const QPoint& position)
{
    if (!m_placementMode) return;

    if (m_playerBoard->placeShip(position, m_currentShipType, m_isHorizontal)) {
        updateShipCount();
        selectNextAvailableShipType();
    }
}

void MainWindow::updateShipCount() 
{
    // Обновляем счетчики размещенных кораблей
    m_placedShips.battleships = 0;
    m_placedShips.cruisers = 0;
    m_placedShips.destroyers = 0;
    m_placedShips.submarines = 0;

    // Проходим по доске и считаем корабли
    for (int y = 0; y < GameBoard::GRID_SIZE; ++y) {
        for (int x = 0; x < GameBoard::GRID_SIZE; ++x) {
            QPoint pos(x, y);
            GameBoard::ShipSize shipType = m_placementBoard->getShipType(pos);
            
            switch (shipType) {
                case GameBoard::ShipSize::BATTLESHIP:
                    m_placedShips.battleships++;
                    break;
                case GameBoard::ShipSize::CRUISER:
                    m_placedShips.cruisers++;
                    break;
                case GameBoard::ShipSize::DESTROYER:
                    m_placedShips.destroyers++;
                    break;
                case GameBoard::ShipSize::SUBMARINE:
                    m_placedShips.submarines++;
                    break;
                default:
                    break;
            }
        }
    }

    // Обновляем UI выбора кораблей
    updateShipSelectionUI();
}

void MainWindow::checkGameOver() 
{
    // Проверка победы в режиме с компьютером
    if (!m_networkMode) {
        if (m_ownBoard->allShipsSunk()) {
            m_gameOver = true;
            QMessageBox::information(this, "Игра окончена", "Компьютер победил!");
            resetGame();
        } else if (m_opponentBoard->allShipsSunk()) {
            m_gameOver = true;
            QMessageBox::information(this, "Игра окончена", "Вы победили!");
            resetGame();
        }
    }
}

void MainWindow::handleShipSunk(const QPoint& position) 
{
    // Отмечаем потопленный корабль
    if (m_networkMode) {
        // В сетевом режиме отправляем сообщение о потопленном корабле
        m_networkClient->sendShipSunk(position.x(), position.y());
    } else {
        // В режиме с компьютером просто обновляем UI
        m_opponentBoard->markSunk(position);
    }

    // Проверяем окончание игры
    checkGameOver();
}

void MainWindow::onReadyClicked()
{
    if (!m_placementMode) return;

    // Проверяем, что все корабли размещены
    if (m_placedShips.battleships != 1 || m_placedShips.cruisers != 2 ||
        m_placedShips.destroyers != 3 || m_placedShips.submarines != 4) {
        QMessageBox::warning(this, "Ошибка", "Разместите все корабли перед началом игры!");
        return;
    }

    if (m_networkMode && m_networkClient) {
        // В сетевом режиме отправляем доску и готовность
        m_networkClient->sendBoard(m_placementBoard->getBoard());
        m_networkClient->sendReady();
        m_placementBoard->setEnabled(false);
        updateStatusMessage("Ожидание противника...");
    } else {
        // Режим игры с компьютером
        int attempts = 0;
        while (!m_opponentBoard->placeRandomShips(100) && attempts++ < 100) {
            m_opponentBoard->clear();
        }
        
        if (attempts >= 100) {
            QMessageBox::critical(this, "Ошибка", "Не удалось разместить корабли противника");
            return;
        }
        
        m_placementMode = false;
        m_isGameStarted = true;
        m_gameActive = true;
        
        // Копируем корабли с доски размещения на игровую доску
        m_ownBoard->setBoard(m_placementBoard->getBoard());
        
        m_isMyTurn = true;
        switchToPage(1);
        updateStatusMessage("Ваш ход!");
    }
}

void MainWindow::updateShipSelectionUI()
{
    // Проверяем, достигнуто ли максимальное количество текущего типа кораблей
    bool currentTypeMaxReached = false;
    switch (m_currentShipType) {
        case GameBoard::ShipSize::BATTLESHIP:
            currentTypeMaxReached = (m_placedShips.battleships >= 1);
            break;
        case GameBoard::ShipSize::CRUISER:
            currentTypeMaxReached = (m_placedShips.cruisers >= 2);
            break;
        case GameBoard::ShipSize::DESTROYER:
            currentTypeMaxReached = (m_placedShips.destroyers >= 3);
            break;
        case GameBoard::ShipSize::SUBMARINE:
            currentTypeMaxReached = (m_placedShips.submarines >= 4);
            break;
    }
    
    // Если текущий тип достиг максимума, выбираем следующий доступный
    if (currentTypeMaxReached) {
        if (m_placedShips.battleships < 1) {
            m_battleshipRadio->setChecked(true);
        } else if (m_placedShips.cruisers < 2) {
            m_cruiserRadio->setChecked(true);
        } else if (m_placedShips.destroyers < 3) {
            m_destroyerRadio->setChecked(true);
        } else if (m_placedShips.submarines < 4) {
            m_submarineRadio->setChecked(true);
        }
    }
    
    // Отключаем радиокнопки для типов кораблей, достигших максимума
    m_battleshipRadio->setEnabled(m_placedShips.battleships < 1);
    m_cruiserRadio->setEnabled(m_placedShips.cruisers < 2);
    m_destroyerRadio->setEnabled(m_placedShips.destroyers < 3);
    m_submarineRadio->setEnabled(m_placedShips.submarines < 4);
    
    // Активируем кнопку "Готово" только если все корабли размещены
    m_readyButton->setEnabled(
        m_placedShips.battleships == 1 && 
        m_placedShips.cruisers == 2 &&
        m_placedShips.destroyers == 3 &&
        m_placedShips.submarines == 4
    );
}

void MainWindow::onGameStartConfirmed()
{
    qDebug() << "GAMAMMA";
    m_gameActive = true;
    m_isMyTurn = true;
   // m_ownBoard->setBoard(m_placementBoard->getBoard());
   //m_isMyTurn = true;
    switchToPage(1);
    updateGameState();
    updateStatusMessage("Игра началась! Ваш ход");
}