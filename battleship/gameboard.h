#pragma once

#include <QWidget>
#include <QVector>
#include <QPoint>
class GameBoard : public QWidget
{
    Q_OBJECT
public:
    enum class ShipSize {
        BATTLESHIP = 4,
        CRUISER = 3,
        DESTROYER = 2,
        SUBMARINE = 1
    };

    enum CellState {
        EMPTY,
        SHIP,
        HIT,
        MISS,
        SUNK
    };

    explicit GameBoard(bool isPlayerBoard, QWidget *parent = nullptr);

    void reset();
    void setPlacementMode(bool enabled);
    bool placeShip(const QPoint& bow, ShipSize size, bool horizontal);
    bool canPlaceShip(const QPoint& bow, ShipSize size, bool horizontal) const;
    CellState checkShot(const QPoint& position) const;
    bool allShipsSunk() const;
    void markShot(const QPoint& position, CellState result);
    bool placeRandomShips(int maxTotalAttempt);
    bool isValidShipPlacement() const;

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

    bool m_isPlayerBoard;
    bool m_placementMode;
    QVector<QVector<CellState>> m_board;
    static const int GRID_SIZE = 10;
    static const int CELL_SIZE = 30;
    static const int MARGIN = 2;
};
