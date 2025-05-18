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

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onRotateShipClicked();
    void onReadyClicked();
    void onSendChatClicked();
    void onPlacementBoardCellClicked(const QPoint& position);
    void onOpponentBoardCellClicked(const QPoint& position);
    void opponentMove();
    void onBattleshipRadioToggled(bool checked);
    void onCruiserRadioToggled(bool checked);
    void onDestroyerRadioToggled(bool checked);
    void onSubmarineRadioToggled(bool checked);
    void onConnectClicked();
    void onLoginResponse(bool success);
    void onGameFound(const QString &opponent);
    void onWaitingForOpponent();
    void onShotResult(int x, int y, bool hit);
    void onGameOver(const QString &winner);
    void showMessage(const QString &message);
    void updateStatus();
    void onOpponentBoardClicked(const QPoint& position);
    void onConnected();
    void onDisconnected();
    void onNetworkError(const QString &error);
    void onLoginClicked();
    void onFindGameClicked();
    void onChatMessageReceived(const QString &sender, const QString &message);
    void onRandomPlacementClicked();
    void onClearBoardClicked();
    void onGameModeChanged(bool networkMode);
    void onShotReceived(int x, int y);
    void onTurnChanged(bool isMyTurn);

private:
    void setupUI();
    void setupConnections();
    void switchToPage(int page);
    void resetGame();
    void updateStatusMessage(const QString& message);
    void addChatMessage(const QString& sender, const QString& message);
    QString getShipTypeName(GameBoard::ShipSize type) const;
    QPoint getNextAIMove();

    QStackedWidget* m_stackedWidget;
    QWidget* m_placementPage;
    GameBoard* m_placementBoard;
    QGroupBox* m_shipSelectionGroup;
    QRadioButton* m_battleshipRadio;
    QRadioButton* m_cruiserRadio;
    QRadioButton* m_destroyerRadio;
    QRadioButton* m_submarineRadio;
    QPushButton* m_rotateShipButton;
    QPushButton* m_readyButton;
    QLabel* m_placementStatusLabel;

    QWidget* m_gamePage;
    GameBoard* m_ownBoard;
    GameBoard* m_opponentBoard;
    QLabel* m_gameStatusLabel;
    QTextEdit* m_chatDisplay;
    QLineEdit* m_chatInput;
    QPushButton* m_sendChatButton;

    bool m_myTurn;
    bool m_gameActive;
    GameBoard::ShipSize m_currentShipType;
    bool m_isHorizontal;

    // Новые поля для ИИ
    QPoint m_lastAIMove;
    bool m_aiFoundShip;
    QVector<QPoint> m_aiPossibleMoves;

    NetworkClient *m_networkClient;
    bool m_isConnected;
    QString m_username;
    QString m_pendingUsername;

    Ui::MainWindow *ui;
    bool m_isMyTurn;
    bool m_isGameStarted;

    bool m_networkMode; // true - сеть, false - компьютер
    bool m_placementMode;

    struct ShipCount {
        int battleships = 0;  // 1 линкор
        int cruisers = 0;     // 2 крейсера
        int destroyers = 0;   // 3 эсминца
        int submarines = 0;   // 4 подлодки
    };
    
    ShipCount m_placedShips;
};
