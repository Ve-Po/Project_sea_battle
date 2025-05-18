#ifndef GAMEBOARD_H
#define GAMEBOARD_H

#include <QWidget>
#include <QVector>
#include <QPoint>

class GameBoard : public QWidget
{
    Q_OBJECT

public:
    enum class ShipSize {
        SUBMARINE = 1,
        DESTROYER = 2,
        CRUISER = 3,
        BATTLESHIP = 4
    };

    enum class CellState {
        EMPTY = 0,
        SHIP = 1,
        HIT = 2,
        MISS = 3,
        SUNK = 4
    };

    static const int GRID_SIZE = 10;
    static const int CELL_SIZE = 30;
    static const int MARGIN = 20;

    explicit GameBoard(bool isPlayerBoard, QWidget *parent = nullptr);
    void reset();
    void clear();
    void setPlacementMode(bool enabled);
    bool placeShip(const QPoint& bow, ShipSize size, bool horizontal);
    bool canPlaceShip(const QPoint& bow, ShipSize size, bool horizontal) const;
    bool placeRandomShips(int maxTotalAttempts = 1000);
    bool isValidShipPlacement() const;
    bool allShipsSunk() const;
    CellState checkShot(const QPoint& position) const;
    void markShot(const QPoint& position, CellState result);
    bool makeShot(const QPoint& position);
    CellState getCellState(const QPoint& position) const;
    void setCellState(const QPoint& position, CellState state);
    QVector<QVector<int>> getBoard() const;
    void setBoard(const QVector<QVector<int>>& board);
    bool isShipSunk(const QPoint& position) const;
    void markSunk(const QPoint& position);
    void markHit(const QPoint& position);
    void markMiss(const QPoint& position);
    void markAroundSunkShip(const QVector<QPoint>& shipCells);
    ShipSize getShipType(const QPoint& pos) const;
    void markSunkShip(const QPoint& position);

signals:
    void cellClicked(const QPoint& position);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    void drawGrid(QPainter &painter);
    void drawCells(QPainter &painter);
    QRect cellRect(int row, int col) const;
    void drawNumbers(QPainter &painter);
    void drawLetters(QPainter &painter);

    QVector<QVector<CellState>> m_board;
    bool m_isPlayerBoard;
    bool m_placementMode;
    bool m_gameOver;
};

#endif // GAMEBOARD_H