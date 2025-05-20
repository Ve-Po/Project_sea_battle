#pragma once

#include <QMainWindow>
#include <QRadioButton>
#include <QGroupBox>
#include <QTextEdit>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include "gameboard.h"
#include <QMessageBox>
#include <QInputDialog>
#include "networkclient.h"
#include "ui_mainwindow.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDateTime>
#include <QTimer>
#include <QRandomGenerator>
#include <QDebug>

namespace Ui {
class MainWindow;
}

// Определяем структуру для отслеживания количества кораблей
struct ShipCount {
    int battleships = 0;  // 1 линкор
    int cruisers = 0;     // 2 крейсера
    int destroyers = 0;   // 3 эсминца
    int submarines = 0;   // 4 подлодки
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onCellClicked(const QPoint& position);
    void onPlacementCellClicked(const QPoint& position);
    void onPlacementBoardCellClicked(const QPoint& position);
    void onStartGameClicked();
    void onRandomPlacementClicked();
    void onHorizontalToggled(bool checked);
    void onSubmarineClicked();
    void onDestroyerClicked();
    void onCruiserClicked();
    void onBattleshipClicked();
    void onNetworkModeToggled(bool checked);
    void onConnectClicked();
    void onDisconnectClicked();
    void onGameModeChanged(bool networkMode);
    void makeAIMove();
    void onBattleshipRadioToggled(bool checked);
    void onCruiserRadioToggled(bool checked);
    void onDestroyerRadioToggled(bool checked);
    void onSubmarineRadioToggled(bool checked);
    void onRotateShipClicked();
    void onReadyClicked();
    void onOpponentBoardCellClicked(const QPoint& position);
    void onSendChatClicked();
    void onChatMessageReceived(const QString& sender, const QString& message);
    void onConnected();
    void onDisconnected();
    void onNetworkError(const QString& error);
    void onLoginResponse(bool success);
    void onGameFound(const QString& opponent);
    void onWaitingForOpponent();
    void onShotResult(int x, int y, bool hit);
    void onGameOver(bool youWin); 
    void onOpponentBoardClicked(const QPoint& pos);
    void onShotReceived(int x, int y);
    void onTurnChanged(bool isMyTurn);
    void startGame();
    void onGameStartConfirmed();

private:
    void setupUI();
    void setupConnections();
    void setupNetworkConnections();
    void updateGameStatus();
    void endGame(bool isWinner);
    void resetGame();
    void updateShipCount();
    void checkGameOver();
    void handleShot(const QPoint& position);
    void handleShotResult(const QPoint& position, bool isHit);
    void handleShipSunk(const QPoint& position);
    void handleTurnChanged(bool isMyTurn);
    QString getShipTypeName(GameBoard::ShipSize type) const;
    void updateShipSelectionUI();
    bool checkOpponentBoardForWin();
    void opponentMove();
    QPoint getNextAIMove();
    void switchToPage(int page);
    void updateStatusMessage(const QString& message);
    void addChatMessage(const QString& sender, const QString& message);
    void showMessage(const QString& message);
    void updateStatus();
    void onClearBoardClicked();
    void onFindGameClicked();
    void onLoginClicked();
    void showNewGameDialog();
    void startNewGame();
    void addAdjacentCells(const QPoint& pos);
    bool checkShipSunk(const QPoint& position);
    int getCurrentShipCount() const;
    void selectNextAvailableShipType();
    void updateGameState();

    Ui::MainWindow *ui;
    NetworkClient *m_networkClient;
    GameBoard *m_playerBoard;
    GameBoard *m_placementBoard;
    GameBoard *m_opponentBoard;
    GameBoard *m_ownBoard;
    QStackedWidget *m_stackedWidget;
    QWidget *m_placementPage;
    QWidget *m_gamePage;
    QGroupBox *m_shipSelectionGroup;
    QRadioButton *m_battleshipRadio;
    QRadioButton *m_cruiserRadio;
    QRadioButton *m_destroyerRadio;
    QRadioButton *m_submarineRadio;
    QPushButton *m_rotateShipButton;
    QPushButton *m_readyButton;
    QLabel *m_placementStatusLabel;
    QLabel *m_gameStatusLabel;
    QTextEdit *m_chatDisplay;
    QLineEdit *m_chatInput;
    QPushButton *m_sendChatButton;

    // Булевые флаги
    bool m_isMyTurn = false;
    bool m_networkMode = false;
    bool m_placementMode = false;
    bool m_isConnected = false;
    bool m_isHorizontal = true;
    bool m_aiFoundShip = false;
    bool m_gameOver = false;
    bool m_gameActive = false;
    bool m_isGameStarted = false;

    // Другие переменные
    GameBoard::ShipSize m_currentShipType = GameBoard::ShipSize::BATTLESHIP;
    QString m_pendingUsername;
    QString m_username;
    QTimer *m_aiTimer = nullptr;
    QPoint m_lastAIMove;
    QVector<QPoint> m_aiPossibleMoves;
    QVector<QPoint> m_activeHits;
    ShipCount m_placedShips;
};