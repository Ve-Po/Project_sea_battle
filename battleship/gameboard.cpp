#include "gameboard.h"
#include <QPainter>
#include <QMouseEvent>
#include <QRandomGenerator>
#include <QDebug>

GameBoard::GameBoard(bool isPlayerBoard, QWidget *parent)
    : QWidget(parent), m_isPlayerBoard(isPlayerBoard), m_placementMode(false)
{
    reset();
    setFixedSize(GRID_SIZE * CELL_SIZE + 2 * MARGIN,
                 GRID_SIZE * CELL_SIZE + 2 * MARGIN);
}

void GameBoard::reset()
{
    m_board.resize(GRID_SIZE);
    for (auto &row : m_board) {
        row.resize(GRID_SIZE);
        row.fill(EMPTY);
    }
    update();
}

void GameBoard::setPlacementMode(bool enabled)
{
    m_placementMode = enabled;
    update();
}

bool GameBoard::placeShip(const QPoint& bow, ShipSize size, bool horizontal)
{
    if (!canPlaceShip(bow, size, horizontal))
        return false;

    int shipSize = static_cast<int>(size);
    if (horizontal) {
        for (int y = bow.y(); y < bow.y() + shipSize; ++y) {
            m_board[bow.x()][y] = SHIP;
        }
    } else {
        for (int x = bow.x(); x < bow.x() + shipSize; ++x) {
            m_board[x][bow.y()] = SHIP;
        }
    }
    update();
    return true;
}

bool GameBoard::canPlaceShip(const QPoint& bow, ShipSize size, bool horizontal) const {
    const int shipSize = static_cast<int>(size);
    const int endX = horizontal ? bow.x() : bow.x() + shipSize - 1;
    const int endY = horizontal ? bow.y() + shipSize - 1 : bow.y();

    // Проверка границ
    if (bow.x() < 0 || bow.y() < 0 || endX >= GRID_SIZE || endY >= GRID_SIZE) {
        return false;
    }

    // Проверка области вокруг корабля
    for (int x = qMax(0, bow.x()-1); x <= qMin(GRID_SIZE-1, endX+1); ++x) {
        for (int y = qMax(0, bow.y()-1); y <= qMin(GRID_SIZE-1, endY+1); ++y) {
            if (m_board[x][y] == SHIP) {
                // Проверяем, не является ли это частью нашего корабля
                bool isOurShip = horizontal
                                     ? (x == bow.x() && y >= bow.y() && y <= endY)
                                     : (y == bow.y() && x >= bow.x() && x <= endX);

                if (!isOurShip) {
                    return false;
                }
            }
        }
    }
    return true;
}

GameBoard::CellState GameBoard::checkShot(const QPoint& position) const
{
    if (position.x() < 0 || position.x() >= GRID_SIZE ||
        position.y() < 0 || position.y() >= GRID_SIZE)
        return EMPTY;

    return m_board[position.x()][position.y()];
}

void GameBoard::markShot(const QPoint& position, CellState result)
{
    if (position.x() < 0 || position.x() >= GRID_SIZE ||
        position.y() < 0 || position.y() >= GRID_SIZE)
        return;

    m_board[position.x()][position.y()] = result;

    if (result == HIT && isShipSunk(position)) {
        markSunk(position);
    }

    update();
}

bool GameBoard::allShipsSunk() const
{
    for (const auto &row : m_board) {
        for (auto cell : row) {
            if (cell == SHIP) return false;
        }
    }
    return true;
}



bool GameBoard::placeRandomShips(int maxTotalAttempts = 1000) {
    reset();
    const ShipSize shipSizes[] = {
        ShipSize::BATTLESHIP,
        ShipSize::CRUISER, ShipSize::CRUISER,
        ShipSize::DESTROYER, ShipSize::DESTROYER, ShipSize::DESTROYER,
        ShipSize::SUBMARINE, ShipSize::SUBMARINE, ShipSize::SUBMARINE, ShipSize::SUBMARINE
    };

    int attempts = 0;
    for (ShipSize size : shipSizes) {
        bool placed = false;
        int shipAttempts = 0;

        while (!placed && attempts++ < maxTotalAttempts && shipAttempts++ < 50) {
            bool horizontal = QRandomGenerator::global()->bounded(2) == 0;
            int x = QRandomGenerator::global()->bounded(GRID_SIZE - (horizontal ? 0 : static_cast<int>(size)));
            int y = QRandomGenerator::global()->bounded(GRID_SIZE - (horizontal ? static_cast<int>(size) : 0));

            placed = placeShip(QPoint(x, y), size, horizontal);
        }

        if (!placed) {
            qWarning() << "Failed to place all ships";
            return false;
        }
    }
    return true;
}

bool GameBoard::isValidShipPlacement() const
{
    int battleships = 0;
    int cruisers = 0;
    int destroyers = 0;
    int submarines = 0;

    QVector<QVector<bool>> visited(GRID_SIZE, QVector<bool>(GRID_SIZE, false));

    for (int x = 0; x < GRID_SIZE; ++x) {
        for (int y = 0; y < GRID_SIZE; ++y) {
            if (m_board[x][y] == SHIP && !visited[x][y]) {
                // Нашли начало корабля
                int size = 1;
                bool isHorizontal = true;

                // Определяем направление корабля
                if (y < GRID_SIZE-1 && m_board[x][y+1] == SHIP) {
                    // Горизонтальный корабль
                    isHorizontal = true;
                    while (y+size < GRID_SIZE && m_board[x][y+size] == SHIP) {
                        visited[x][y+size] = true;
                        size++;
                    }
                } else {
                    // Вертикальный корабль
                    isHorizontal = false;
                    while (x+size < GRID_SIZE && m_board[x+size][y] == SHIP) {
                        visited[x+size][y] = true;
                        size++;
                    }
                }

                // Проверяем окружение корабля
                int startX = qMax(0, x - 1);
                int endX = qMin(GRID_SIZE-1, isHorizontal ? x + 1 : x + size);
                int startY = qMax(0, y - 1);
                int endY = qMin(GRID_SIZE-1, isHorizontal ? y + size : y + 1);

                for (int i = startX; i <= endX; ++i) {
                    for (int j = startY; j <= endY; ++j) {
                        // Пропускаем клетки самого корабля
                        bool isShipCell = isHorizontal
                                              ? (i == x && j >= y && j < y + size)
                                              : (j == y && i >= x && i < x + size);

                        if (!isShipCell && m_board[i][j] == SHIP) {
                            return false; // Корабли слишком близко
                        }
                    }
                }

                // Учитываем корабль соответствующего типа
                switch(size) {
                case 4: battleships++; break;
                case 3: cruisers++; break;
                case 2: destroyers++; break;
                case 1: submarines++; break;
                default: return false; // Недопустимый размер
                }

                visited[x][y] = true;
            }
        }
    }

    // Проверяем количество кораблей каждого типа
    return battleships == 1 && cruisers == 2 &&
           destroyers == 3 && submarines == 4;
}

void GameBoard::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.fillRect(rect(), Qt::white);
    drawGrid(painter);
    drawCells(painter);
}

void GameBoard::drawGrid(QPainter &painter)
{
    painter.setPen(Qt::black);

    for (int i = 0; i <= GRID_SIZE; ++i) {
        painter.drawLine(MARGIN, MARGIN + i * CELL_SIZE,
                         MARGIN + GRID_SIZE * CELL_SIZE, MARGIN + i * CELL_SIZE);
        painter.drawLine(MARGIN + i * CELL_SIZE, MARGIN,
                         MARGIN + i * CELL_SIZE, MARGIN + GRID_SIZE * CELL_SIZE);
    }
}

void GameBoard::drawCells(QPainter &painter)
{
    for (int x = 0; x < GRID_SIZE; ++x) {
        for (int y = 0; y < GRID_SIZE; ++y) {
            QRect rect = cellRect(x, y);

            switch (m_board[x][y]) {
            case SHIP:
                if (m_isPlayerBoard) {
                    painter.fillRect(rect, QColor(100, 100, 200));
                }
                break;
            case HIT:
                painter.setBrush(Qt::red);
                painter.drawEllipse(rect.center(), CELL_SIZE/4, CELL_SIZE/4);
                break;
            case MISS:
                painter.setBrush(Qt::blue);
                painter.drawEllipse(rect.center(), CELL_SIZE/6, CELL_SIZE/6);
                break;
            case SUNK:
                painter.fillRect(rect, Qt::darkRed);
                painter.setPen(Qt::white);
                painter.drawLine(rect.topLeft(), rect.bottomRight());
                painter.drawLine(rect.topRight(), rect.bottomLeft());
                painter.setPen(Qt::black);
                break;
            default:
                break;
            }
        }
    }
}

QRect GameBoard::cellRect(int row, int col) const
{
    return QRect(MARGIN + col * CELL_SIZE + 1,
                 MARGIN + row * CELL_SIZE + 1,
                 CELL_SIZE - 2,
                 CELL_SIZE - 2);
}

bool GameBoard::isShipSunk(const QPoint& position) const
{
    if (m_board[position.x()][position.y()] != HIT) return false;

    QVector<QPoint> shipCells;
    QVector<QPoint> queue;
    queue.append(position);

    while (!queue.isEmpty()) {
        QPoint p = queue.takeFirst();

        if (shipCells.contains(p)) continue;
        shipCells.append(p);

        if (p.x() > 0 && m_board[p.x()-1][p.y()] >= SHIP)
            queue.append(QPoint(p.x()-1, p.y()));
        if (p.x() < GRID_SIZE-1 && m_board[p.x()+1][p.y()] >= SHIP)
            queue.append(QPoint(p.x()+1, p.y()));
        if (p.y() > 0 && m_board[p.x()][p.y()-1] >= SHIP)
            queue.append(QPoint(p.x(), p.y()-1));
        if (p.y() < GRID_SIZE-1 && m_board[p.x()][p.y()+1] >= SHIP)
            queue.append(QPoint(p.x(), p.y()+1));
    }

    for (const QPoint &p : shipCells) {
        if (m_board[p.x()][p.y()] == SHIP) return false;
    }

    return true;
}

void GameBoard::markSunk(const QPoint& position)
{
    QVector<QPoint> queue;
    queue.append(position);

    while (!queue.isEmpty()) {
        QPoint p = queue.takeFirst();

        if (m_board[p.x()][p.y()] == SUNK) continue;
        m_board[p.x()][p.y()] = SUNK;

        if (p.x() > 0 && m_board[p.x()-1][p.y()] >= HIT)
            queue.append(QPoint(p.x()-1, p.y()));
        if (p.x() < GRID_SIZE-1 && m_board[p.x()+1][p.y()] >= HIT)
            queue.append(QPoint(p.x()+1, p.y()));
        if (p.y() > 0 && m_board[p.x()][p.y()-1] >= HIT)
            queue.append(QPoint(p.x(), p.y()-1));
        if (p.y() < GRID_SIZE-1 && m_board[p.x()][p.y()+1] >= HIT)
            queue.append(QPoint(p.x(), p.y()+1));
    }

    update();
}

void GameBoard::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QPoint pos = event->pos();
        int row = (pos.y() - MARGIN) / CELL_SIZE;
        int col = (pos.x() - MARGIN) / CELL_SIZE;

        if (row >= 0 && row < GRID_SIZE && col >= 0 && col < GRID_SIZE) {
            emit cellClicked(QPoint(row, col));
        }
    }
    QWidget::mousePressEvent(event);
}
