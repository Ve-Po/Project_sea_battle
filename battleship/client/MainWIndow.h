#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QLabel>
#include <QStackedWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QRandomGenerator>
#include <QGroupBox>
#include <QRadioButton>
#include <QVector>
#include "GameBoard.h"
#include "NetworkClient.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

struct ShipCount {
    int battleships = 0;
    int cruisers = 0;
    int destroyers = 0;
    int submarines = 0;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // Placement slots
    void onPlacementBoardCellClicked(const QPoint& position);
    void onRotateShipClicked();
    void onReadyClicked();
    void onBattleshipRadioToggled(bool checked);
    void onCruiserRadioToggled(bool checked);
    void onDestroyerRadioToggled(bool checked);
    void onSubmarineRadioToggled(bool checked);
    
    // Game slots
    void onOpponentBoardCellClicked(const QPoint& position);
    void onSendChatClicked();
    
    // Network slots
    void onConnectClicked();
    void onConnected();
    void onDisconnected();
    void onLoginResponse(bool success);
    void onGameFound(const QString& opponent);
    void onWaitingForOpponent();
    void onShotResult(int x, int y, bool hit);
    void onGameOver(bool youWin);
    void onNetworkError(const QString& error);
    void onChatMessageReceived(const QString& sender, const QString& message);
    void onShotReceived(int x, int y);
    void onTurnChanged(bool isMyTurn);
    void onGameStartConfirmed();
    void onLobbyCreated(const QString &lobbyId);
    void onShipSunk(int x, int y);



private:
    // UI setup
    void setupUI();
    void setupConnections();
    void switchToPage(int page);
    
    // Game logic
    void resetGame();
    void updateStatusMessage(const QString& message);
    void addChatMessage(const QString& sender, const QString& message);
    void updateGameState();
    QString getShipTypeName(GameBoard::ShipSize type) const;
    bool checkOpponentBoardForWin();
    void opponentMove();
    QPoint getNextAIMove();
    void updateShipSelectionUI();
    void addAdjacentCells(const QPoint& pos);
    bool checkShipSunk(const QPoint& position);
    
    // Helper methods
    int getCurrentShipCount() const;
    void selectNextAvailableShipType();

    Ui::MainWindow *ui;
    
    // Game state
    bool m_gameActive;
    bool m_isGameStarted;
    bool m_isMyTurn;
    bool m_placementMode;
    bool m_isHorizontal;
    bool m_isConnected;
    bool m_networkMode;
    bool m_aiFoundShip;
    
    // Ship placement state
    GameBoard::ShipSize m_currentShipType;
    ShipCount m_placedShips;
    
    // Network state
    QString m_username;
    QString m_pendingUsername;
    NetworkClient* m_networkClient;
    
    // AI state
    QVector<QPoint> m_aiPossibleMoves;
    QVector<QPoint> m_activeHits;
    QPoint m_lastAIMove;
    
    // UI components
    QStackedWidget* m_stackedWidget;
    QWidget* m_placementPage;
    QWidget* m_gamePage;
    GameBoard* m_placementBoard;
    GameBoard* m_opponentBoard;
    GameBoard* m_ownBoard;
    QGroupBox* m_shipSelectionGroup;
    QRadioButton* m_battleshipRadio;
    QRadioButton* m_cruiserRadio;
    QRadioButton* m_destroyerRadio;
    QRadioButton* m_submarineRadio;
    QPushButton* m_rotateShipButton;
    QPushButton* m_readyButton;
    QLabel* m_placementStatusLabel;
    QLabel* m_gameStatusLabel;
    QTextEdit* m_chatDisplay;
    QLineEdit* m_chatInput;
    QPushButton* m_sendChatButton;
};
#endif // MAINWINDOW_H
