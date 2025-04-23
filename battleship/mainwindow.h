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

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
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

private:
    void setupUI();
    void setupConnections();
    void switchToPage(int page);
    void resetGame();
    void updateStatusMessage(const QString& message);
    void addChatMessage(const QString& sender, const QString& message);
    QString getShipTypeName(GameBoard::ShipSize type) const;

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
};
