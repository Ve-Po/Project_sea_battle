#ifndef GAMEBOARD_H
#define GAMEBOARD_H

#include <QWidget>
#include <QVector>
#include <QPoint>

const int BOARD_SIZE = 10;

class GameBoard : public QWidget
{
    Q_OBJECT
public:
    enum CellState {
        EMPTY = 0,
        SHIP = 1,
        HIT = 2,
        MISS = 3,
        SUNK = 4
    };

    enum ShipSize {
        SUBMARINE = 1,
        DESTROYER = 2,
        CRUISER = 3,
        BATTLESHIP = 4
    };

    static const int GRID_SIZE = 10;
    static const int CELL_SIZE = 30;
    static const int MARGIN = 20;

    explicit GameBoard(bool isPlayerBoard, QWidget *parent = nullptr);
    void reset();
    void setPlacementMode(bool enabled);
    bool placeShip(const QPoint& bow, ShipSize size, bool horizontal);
    bool canPlaceShip(const QPoint& bow, ShipSize size, bool horizontal) const;
    CellState checkShot(const QPoint& position) const;
    void markShot(const QPoint& position, CellState result);
    bool allShipsSunk() const;
    bool placeRandomShips(int maxTotalAttempts = 1000);
    bool isValidShipPlacement() const;
    QVector<QVector<int>> getBoard() const;
    void setBoard(const QVector<QVector<int>>& board);
    int getShipSizeAt(const QPoint& position) const;
    void clearBoard();
    void markHit(const QPoint& position);
    void markMiss(const QPoint& position);

signals:
    void cellClicked(const QPoint& position);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    void drawGrid(QPainter &painter);
    void drawCells(QPainter &painter);
    QRect cellRect(int row, int col) const;
    bool isShipSunk(const QPoint& position) const;
    void markSunk(const QPoint& position);

    QVector<QVector<CellState>> m_board;
    bool m_isPlayerBoard;
    bool m_placementMode;
};

#endif // GAMEBOARD_H
