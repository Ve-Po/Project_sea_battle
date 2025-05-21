#include "GameBoard.h"
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
        row.fill(CellState::EMPTY);
    }
    update();
}

void GameBoard::clear()
{
    reset();
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
        for (int x = bow.x(); x < bow.x() + shipSize; ++x) {
            m_board[bow.y()][x] = CellState::SHIP;
        }
    } else {
        for (int y = bow.y(); y < bow.y() + shipSize; ++y) {
            m_board[y][bow.x()] = CellState::SHIP;
        }
    }
    update();
    return true;
}

bool GameBoard::canPlaceShip(const QPoint& bow, ShipSize size, bool horizontal) const
{
    const int shipSize = static_cast<int>(size);
    const int endX = horizontal ? bow.x() + shipSize - 1 : bow.x();
    const int endY = horizontal ? bow.y() : bow.y() + shipSize - 1;

    // Проверка границ
    if (bow.x() < 0 || bow.y() < 0 || endX >= GRID_SIZE || endY >= GRID_SIZE) {
        return false;
    }

    // Проверка области вокруг корабля
    for (int y = qMax(0, bow.y()-1); y <= qMin(GRID_SIZE-1, endY+1); ++y) {
        for (int x = qMax(0, bow.x()-1); x <= qMin(GRID_SIZE-1, endX+1); ++x) {
            if (m_board[y][x] == CellState::SHIP) {
                // Проверяем, не является ли это частью нашего корабля
                bool isOurShip = horizontal
                    ? (y == bow.y() && x >= bow.x() && x <= endX)
                    : (x == bow.x() && y >= bow.y() && y <= endY);

                if (!isOurShip) {
                    return false;
                }
            }
        }
    }

    // Проверяем, что все клетки под кораблем пустые
    if (horizontal) {
        for (int x = bow.x(); x <= endX; ++x) {
            if (m_board[bow.y()][x] != CellState::EMPTY) {
                return false;
            }
        }
    } else {
        for (int y = bow.y(); y <= endY; ++y) {
            if (m_board[y][bow.x()] != CellState::EMPTY) {
                return false;
            }
        }
    }

    return true;
}

GameBoard::CellState GameBoard::checkShot(const QPoint& position) const
{
    if (position.x() < 0 || position.x() >= GRID_SIZE ||
        position.y() < 0 || position.y() >= GRID_SIZE)
        return CellState::EMPTY;

    return m_board[position.y()][position.x()];
}

void GameBoard::markShot(const QPoint& position, CellState result)
{
    if (position.x() < 0 || position.x() >= GRID_SIZE ||
        position.y() < 0 || position.y() >= GRID_SIZE)
        return;

    // Если уже стреляли в эту клетку - ничего не делаем
    if (m_board[position.y()][position.x()] == CellState::HIT ||
        m_board[position.y()][position.x()] == CellState::MISS ||
        m_board[position.y()][position.x()] == CellState::SUNK) {
        return;
    }

    if (result == CellState::HIT) {
        m_board[position.y()][position.x()] = CellState::HIT;
        // Проверяем, не потоплен ли корабль
        if (isShipSunk(position)) {
            markSunkShip(position);
        }
    } else if (result == CellState::MISS) {
        m_board[position.y()][position.x()] = CellState::MISS;
    }
    update();
}

bool GameBoard::allShipsSunk() const
{
    for (const auto &row : m_board) {
        for (auto cell : row) {
            if (cell == CellState::SHIP) return false;
        }
    }
    return true;
}

bool GameBoard::placeRandomShips(int maxTotalAttempts)
{
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
            int x = QRandomGenerator::global()->bounded(GRID_SIZE - (horizontal ? static_cast<int>(size) : 0));
            int y = QRandomGenerator::global()->bounded(GRID_SIZE - (horizontal ? 0 : static_cast<int>(size)));

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

    for (int y = 0; y < GRID_SIZE; ++y) {
        for (int x = 0; x < GRID_SIZE; ++x) {
            if (m_board[y][x] == CellState::SHIP && !visited[y][x]) {
                // Нашли начало корабля
                int size = 1;
                bool isHorizontal = true;

                // Определяем направление корабля
                if (x < GRID_SIZE-1 && m_board[y][x+1] == CellState::SHIP) {
                    // Горизонтальный корабль
                    isHorizontal = true;
                    while (x+size < GRID_SIZE && m_board[y][x+size] == CellState::SHIP) {
                        visited[y][x+size] = true;
                        size++;
                    }
                } else {
                    // Вертикальный корабль
                    isHorizontal = false;
                    while (y+size < GRID_SIZE && m_board[y+size][x] == CellState::SHIP) {
                        visited[y+size][x] = true;
                        size++;
                    }
                }

                // Проверяем окружение корабля
                int startX = qMax(0, x - 1);
                int endX = qMin(GRID_SIZE-1, isHorizontal ? x + size : x + 1);
                int startY = qMax(0, y - 1);
                int endY = qMin(GRID_SIZE-1, isHorizontal ? y + 1 : y + size);

                for (int i = startX; i <= endX; ++i) {
                    for (int j = startY; j <= endY; ++j) {
                        // Пропускаем клетки самого корабля
                        bool isShipCell = isHorizontal
                            ? (j == y && i >= x && i < x + size)
                            : (i == x && j >= y && j < y + size);

                        if (!isShipCell && m_board[j][i] == CellState::SHIP) {
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

                visited[y][x] = true;
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
    for (int y = 0; y < GRID_SIZE; ++y) {
        for (int x = 0; x < GRID_SIZE; ++x) {
            QRect rect = cellRect(y, x);
            CellState state = m_board[y][x];

            switch (state) {
            case CellState::SHIP:
                // Корабли противника не показываем (если это не наша доска)
                if (m_isPlayerBoard) {
                    painter.fillRect(rect, QColor(100, 100, 200)); // Синий для наших кораблей
                }
                break;
            case CellState::HIT:
                // Попадание - красная клетка с крестом
                painter.fillRect(rect, Qt::red);
                painter.drawLine(rect.topLeft(), rect.bottomRight());
                painter.drawLine(rect.topRight(), rect.bottomLeft());
                break;
            case CellState::MISS:
                // Промах - серая клетка с кружком
                painter.fillRect(rect, QColor(200, 200, 200));
                painter.drawEllipse(rect.adjusted(5, 5, -5, -5));
                break;
            case CellState::SUNK:
                // Потопленный корабль - черная клетка с красным крестом
                painter.fillRect(rect, Qt::black);
                painter.setPen(Qt::red);
                painter.drawLine(rect.topLeft(), rect.bottomRight());
                painter.drawLine(rect.topRight(), rect.bottomLeft());
                painter.setPen(Qt::black); // Восстанавливаем цвет пера
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
    if (m_board[position.y()][position.x()] != CellState::HIT) return false;

    // Находим все клетки корабля
    QVector<QPoint> shipCells;
    shipCells.append(position);

    // Проверяем все 4 направления
    const QVector<QPair<int, int>> directions = {
        {-1, 0}, // влево
        {1, 0},  // вправо
        {0, -1}, // вверх
        {0, 1}   // вниз
    };

    // Проверяем каждое направление
    for (const auto& dir : directions) {
        int x = position.x() + dir.first;
        int y = position.y() + dir.second;
        
        // Идем в этом направлении, пока находим клетки корабля
        while (x >= 0 && x < GRID_SIZE && y >= 0 && y < GRID_SIZE) {
            if (m_board[y][x] == CellState::SHIP || m_board[y][x] == CellState::HIT) {
                shipCells.append(QPoint(x, y));
                x += dir.first;
                y += dir.second;
            } else {
                break;
            }
        }
    }

    // Проверяем, все ли клетки корабля подбиты
    for (const QPoint &p : shipCells) {
        if (m_board[p.y()][p.x()] != CellState::HIT) {
            return false;
        }
    }

    return true;
}

void GameBoard::markSunk(const QPoint& position)
{
    // Находим все клетки корабля
    QVector<QPoint> shipCells;
    
    // Определяем направление корабля
    bool isHorizontal = false;
    if (position.x() < GRID_SIZE-1 && (m_board[position.y()][position.x()+1] == CellState::SHIP || 
                                      m_board[position.y()][position.x()+1] == CellState::HIT)) {
        isHorizontal = true;
    } else if (position.y() < GRID_SIZE-1 && (m_board[position.y()+1][position.x()] == CellState::SHIP || 
                                            m_board[position.y()+1][position.x()] == CellState::HIT)) {
        isHorizontal = false;
    }

    // Собираем все клетки корабля
    if (isHorizontal) {
        // Ищем влево
        for (int x = position.x(); x >= 0; --x) {
            if (m_board[position.y()][x] == CellState::SHIP || 
                m_board[position.y()][x] == CellState::HIT) {
                shipCells.append(QPoint(x, position.y()));
            } else {
                break;
            }
        }
        // Ищем вправо
        for (int x = position.x() + 1; x < GRID_SIZE; ++x) {
            if (m_board[position.y()][x] == CellState::SHIP || 
                m_board[position.y()][x] == CellState::HIT) {
                shipCells.append(QPoint(x, position.y()));
            } else {
                break;
            }
        }
    } else {
        // Ищем вверх
        for (int y = position.y(); y >= 0; --y) {
            if (m_board[y][position.x()] == CellState::SHIP || 
                m_board[y][position.x()] == CellState::HIT) {
                shipCells.append(QPoint(position.x(), y));
            } else {
                break;
            }
        }
        // Ищем вниз
        for (int y = position.y() + 1; y < GRID_SIZE; ++y) {
            if (m_board[y][position.x()] == CellState::SHIP || 
                m_board[y][position.x()] == CellState::HIT) {
                shipCells.append(QPoint(position.x(), y));
            } else {
                break;
            }
        }
    }

    // Помечаем все клетки корабля как потопленные
    for (const QPoint &p : shipCells) {
        m_board[p.y()][p.x()] = CellState::SUNK;
    }

    // Помечаем клетки вокруг потопленного корабля
    markAroundSunkShip(shipCells);

    update();
}

void GameBoard::markAroundSunkShip(const QVector<QPoint>& shipCells)
{
    qDebug() << "[DEBUG] markAroundSunkShip called. isPlayerBoard=" << m_isPlayerBoard;
    for (const QPoint& p : shipCells) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                int nx = p.x() + dx;
                int ny = p.y() + dy;
                if (nx >= 0 && nx < GRID_SIZE && ny >= 0 && ny < GRID_SIZE) {
                    if (m_board[ny][nx] == CellState::EMPTY) {
                        qDebug() << "[DEBUG] markAroundSunkShip: MISS set at (" << nx << "," << ny << ")";
                        m_board[ny][nx] = CellState::MISS;
                    }
                }
            }
        }
    }
}
void GameBoard::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QPoint pos = event->pos();
        int row = (pos.y() - MARGIN) / CELL_SIZE;
        int col = (pos.x() - MARGIN) / CELL_SIZE;

        if (row >= 0 && row < GRID_SIZE && col >= 0 && col < GRID_SIZE) {
            emit cellClicked(QPoint(col, row));
        }
    }
    QWidget::mousePressEvent(event);
}

QVector<QVector<int>> GameBoard::getBoard() const {
    qDebug() << "[DEBUG] getBoard called. isPlayerBoard=" << m_isPlayerBoard;
    QVector<QVector<int>> result(GRID_SIZE, QVector<int>(GRID_SIZE));
    for (int y = 0; y < GRID_SIZE; ++y) {
        for (int x = 0; x < GRID_SIZE; ++x) {
            result[y][x] = static_cast<int>(m_board[y][x]);
        }
    }
    return result;
}

void GameBoard::setBoard(const QVector<QVector<int>>& board) {
    qDebug() << "[DEBUG] setBoard called. isPlayerBoard=" << m_isPlayerBoard;
    for (int y = 0; y < board.size() && y < GRID_SIZE; ++y) {
        for (int x = 0; x < board[y].size() && x < GRID_SIZE; ++x) {
            if (board[y][x] == static_cast<int>(CellState::MISS)) {
                qDebug() << "[DEBUG] setBoard: MISS detected at (" << x << "," << y << ")";
            }
            m_board[y][x] = static_cast<CellState>(board[y][x]);
        }
    }
    update();
}


bool GameBoard::makeShot(const QPoint& position) {
    if (position.x() < 0 || position.x() >= GRID_SIZE ||
        position.y() < 0 || position.y() >= GRID_SIZE)
        return false;

    // Проверяем, не стреляли ли уже в эту клетку
    if (m_board[position.y()][position.x()] == CellState::HIT || 
        m_board[position.y()][position.x()] == CellState::MISS ||
        m_board[position.y()][position.x()] == CellState::SUNK)
        return false;

    // Если это поле компьютера, то мы не видим его корабли
    if (!m_isPlayerBoard) {
        if (m_board[position.y()][position.x()] == CellState::SHIP) {
            m_board[position.y()][position.x()] = CellState::HIT;
            update();
            return true;
        } else {
            m_board[position.y()][position.x()] = CellState::MISS;
            update();
            return false;
        }
    } else {
        // Для поля игрока
        if (m_board[position.y()][position.x()] == CellState::SHIP) {
            m_board[position.y()][position.x()] = CellState::HIT;
            update();
            return true;
        } else if (m_board[position.y()][position.x()] == CellState::EMPTY) {
            m_board[position.y()][position.x()] = CellState::MISS;
            update();
            return false;
        }
    }
    return false;
}

GameBoard::CellState GameBoard::getCellState(const QPoint& position) const {
    if (position.x() < 0 || position.x() >= GRID_SIZE ||
        position.y() < 0 || position.y() >= GRID_SIZE)
        return CellState::EMPTY;
    return m_board[position.y()][position.x()];
}

void GameBoard::setCellState(const QPoint& position, CellState state) {
    qDebug() << "[DEBUG] setCellState(" << position << "," << static_cast<int>(state) << ") isPlayerBoard=" << m_isPlayerBoard;
    if (position.x() < 0 || position.x() >= GRID_SIZE ||
        position.y() < 0 || position.y() >= GRID_SIZE)
        return;
    m_board[position.y()][position.x()] = state;
    update();
}

void GameBoard::markHit(const QPoint& position)
{
    if (position.x() >= 0 && position.x() < GRID_SIZE && 
        position.y() >= 0 && position.y() < GRID_SIZE) {
        m_board[position.y()][position.x()] = CellState::HIT;
        update();
    }
}

void GameBoard::markMiss(const QPoint& position)
{
    if (position.x() >= 0 && position.x() < GRID_SIZE && 
        position.y() >= 0 && position.y() < GRID_SIZE) {
        m_board[position.y()][position.x()] = CellState::MISS;
        update();
    }
}

GameBoard::ShipSize GameBoard::getShipType(const QPoint& pos) const 
{
    if (pos.x() < 0 || pos.x() >= GRID_SIZE || pos.y() < 0 || pos.y() >= GRID_SIZE) {
        return ShipSize::SUBMARINE; // Возвращаем значение по умолчанию
    }

    // Проверяем размер корабля по количеству соседних клеток с кораблем
    int shipSize = 1;
    
    // Проверяем горизонтальное направление
    for (int x = pos.x() - 1; x >= 0; --x) {
        if (m_board[pos.y()][x] == CellState::SHIP) {
            shipSize++;
        } else {
            break;
        }
    }
    for (int x = pos.x() + 1; x < GRID_SIZE; ++x) {
        if (m_board[pos.y()][x] == CellState::SHIP) {
            shipSize++;
        } else {
            break;
        }
    }

    // Проверяем вертикальное направление
    for (int y = pos.y() - 1; y >= 0; --y) {
        if (m_board[y][pos.x()] == CellState::SHIP) {
            shipSize++;
        } else {
            break;
        }
    }
    for (int y = pos.y() + 1; y < GRID_SIZE; ++y) {
        if (m_board[y][pos.x()] == CellState::SHIP) {
            shipSize++;
        } else {
            break;
        }
    }

    // Определяем тип корабля по размеру
    switch (shipSize) {
        case 1: return ShipSize::SUBMARINE;
        case 2: return ShipSize::DESTROYER;
        case 3: return ShipSize::CRUISER;
        case 4: return ShipSize::BATTLESHIP;
        default: return ShipSize::SUBMARINE;
    }
}

void GameBoard::markSunkShip(const QPoint& position)
{
    qDebug() << "[DEBUG] markSunkShip called at (" << position.x() << "," << position.y() << ") isPlayerBoard=" << m_isPlayerBoard;
    QVector<QPoint> shipCells = findShipCells(position);
    for (const QPoint& p : shipCells) {
        m_board[p.y()][p.x()] = CellState::SUNK;
    }
    markAroundSunkShip(shipCells);
    update();
}


QVector<QPoint> GameBoard::findShipCells(const QPoint& position) const
{
    QVector<QPoint> shipCells;
    shipCells.append(position);
    
    // Проверяем все 4 направления
    const QVector<QPair<int, int>> directions = {
        {-1, 0}, {1, 0}, {0, -1}, {0, 1}
    };
    
    for (const auto& dir : directions) {
        int x = position.x() + dir.first;
        int y = position.y() + dir.second;
        
        while (x >= 0 && x < GRID_SIZE && y >= 0 && y < GRID_SIZE) {
            if (m_board[y][x] == CellState::SHIP || m_board[y][x] == CellState::HIT) {
                shipCells.append(QPoint(x, y));
                x += dir.first;
                y += dir.second;
            } else {
                break;
            }
        }
    }
    
    return shipCells;
}

QVector<QVector<int>> GameBoard::getInitialBoard() const {
    QVector<QVector<int>> result(GRID_SIZE, QVector<int>(GRID_SIZE));
    for (int y = 0; y < GRID_SIZE; ++y) {
        for (int x = 0; x < GRID_SIZE; ++x) {
            if (m_board[y][x] == CellState::SHIP)
                result[y][x] = static_cast<int>(CellState::SHIP);
            else
                result[y][x] = static_cast<int>(CellState::EMPTY);
        }
    }
    return result;
}
