#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QDateTime>
#include <QTimer>
#include <QRandomGenerator>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    m_myTurn(true),
    m_gameActive(false),
    m_currentShipType(GameBoard::ShipSize::BATTLESHIP),
    m_isHorizontal(true)
{
    setupUI();
    setupConnections();
    setWindowTitle("Морской бой");
    resize(800, 600);
}

MainWindow::~MainWindow() {}

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

void MainWindow::onPlacementBoardCellClicked(const QPoint& position)
{
    if (m_placementBoard->placeShip(position, m_currentShipType, m_isHorizontal)) {
        updateStatusMessage(QString("%1 размещен").arg(getShipTypeName(m_currentShipType)));
    } else {
        updateStatusMessage("Невозможно разместить корабль здесь");
    }
}

void MainWindow::onReadyClicked()
{
    if (!m_placementBoard->isValidShipPlacement()) {
        QMessageBox::warning(this, "Ошибка", "Пожалуйста, разместите все корабли по правилам:\n"
                                             "1 линкор (4 клетки)\n"
                                             "2 крейсера (3 клетки)\n"
                                             "3 эсминца (2 клетки)\n"
                                             "4 подлодки (1 клетка)\n"
                                             "Корабли не должны соприкасаться!");
        return;
    }

    // Копируем расстановку на игровое поле
    m_ownBoard->reset();
    for (int x = 0; x < 10; ++x) {
        for (int y = 0; y < 10; ++y) {
            if (m_placementBoard->checkShot(QPoint(x, y)) == GameBoard::SHIP) {
                m_ownBoard->placeShip(QPoint(x, y), GameBoard::ShipSize::SUBMARINE, true);
            }
        }
    }

    // Размещаем корабли противника
    m_opponentBoard->placeRandomShips(1000);

    switchToPage(1);
    m_gameActive = true;
    m_myTurn = true;
    updateStatusMessage("Игра началась. Ваш ход!");
}

void MainWindow::onOpponentBoardCellClicked(const QPoint& position)
{
    if (!m_gameActive || !m_myTurn) return;

    auto cellState = m_opponentBoard->checkShot(position);
    if (cellState != GameBoard::EMPTY && cellState != GameBoard::SHIP) {
        return; // Уже стреляли сюда
    }

    if (cellState == GameBoard::SHIP) {
        m_opponentBoard->markShot(position, GameBoard::HIT);
        updateStatusMessage("Попадание! Ваш ход снова.");

        if (m_opponentBoard->allShipsSunk()) {
            QMessageBox::information(this, "Победа!", "Вы выиграли! Все корабли противника потоплены.");
            resetGame();
            return;
        }
    } else {
        m_opponentBoard->markShot(position, GameBoard::MISS);
        m_myTurn = false;
        updateStatusMessage("Промах! Ход противника.");
        QTimer::singleShot(1000, this, &MainWindow::opponentMove);
    }
}

void MainWindow::opponentMove()
{
    if (!m_gameActive || m_myTurn) return;

    // Простой ИИ - случайные выстрелы
    QPoint target;
    do {
        target = QPoint(QRandomGenerator::global()->bounded(10),
                        QRandomGenerator::global()->bounded(10));
    } while (m_ownBoard->checkShot(target) != GameBoard::EMPTY &&
             m_ownBoard->checkShot(target) != GameBoard::SHIP);

    auto cellState = m_ownBoard->checkShot(target);
    if (cellState == GameBoard::SHIP) {
        m_ownBoard->markShot(target, GameBoard::HIT);
        addChatMessage("Противник", QString("Попадание в %1,%2").arg(target.x()).arg(target.y()));

        if (m_ownBoard->allShipsSunk()) {
            QMessageBox::information(this, "Поражение", "Вы проиграли! Все ваши корабли потоплены.");
            resetGame();
            return;
        }

        QTimer::singleShot(1000, this, &MainWindow::opponentMove);
    } else {
        m_ownBoard->markShot(target, GameBoard::MISS);
        addChatMessage("Противник", QString("Промах в %1,%2").arg(target.x()).arg(target.y()));
        m_myTurn = true;
        updateStatusMessage("Противник промахнулся! Ваш ход.");
    }
}

void MainWindow::onSendChatClicked()
{
    QString message = m_chatInput->text().trimmed();
    if (!message.isEmpty()) {
        addChatMessage("Вы", message);
        m_chatInput->clear();
    }
}

void MainWindow::switchToPage(int page)
{
    m_stackedWidget->setCurrentIndex(page);
}

void MainWindow::resetGame()
{
    m_placementBoard->reset();
    m_ownBoard->reset();
    m_opponentBoard->reset();
    m_chatDisplay->clear();

    m_gameActive = false;
    m_myTurn = true;
    m_currentShipType = GameBoard::ShipSize::BATTLESHIP;
    m_isHorizontal = true;
    m_placementBoard->setPlacementMode(true);
    m_battleshipRadio->setChecked(true);

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
