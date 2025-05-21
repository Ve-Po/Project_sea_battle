#!/bin/bash

# Проверка на root права
if [ "$EUID" -ne 0 ]; then 
    echo "Пожалуйста, запустите скрипт с правами sudo"
    exit 1
fi

# Установка необходимых зависимостей
echo "Установка зависимостей..."
apt-get update
apt-get install -y \
    build-essential \
    qtbase5-dev \
    qt5-qmake \
    libqt5network5t64 \
    libqt5gui5t64 \
    libqt5widgets5t64 \
    libqt5core5t64

# Создание директории для установки
INSTALL_DIR="/usr/games/sea-battle"
mkdir -p $INSTALL_DIR

# Компиляция клиента
echo "Компиляция клиента..."
cd client
qmake
make clean
make
make install

# Компиляция сервера
echo "Компиляция сервера..."
cd ../server
qmake
make clean
make
make install

# Создание ярлыков запуска
echo "Создание ярлыков запуска..."
cat > /usr/local/bin/sea-battle-client << EOF
#!/bin/bash
$INSTALL_DIR/seabattle_client
EOF

cat > /usr/local/bin/sea-battle-server << EOF
#!/bin/bash
$INSTALL_DIR/GameServer
EOF

chmod +x /usr/local/bin/sea-battle-client
chmod +x /usr/local/bin/sea-battle-server

# Создание .desktop файла для меню приложений
mkdir -p /usr/share/applications
cat > /usr/share/applications/sea-battle.desktop << EOF
[Desktop Entry]
Name=Sea Battle
Comment=Морской бой
Exec=sea-battle-client
Icon=applications-games
Terminal=false
Type=Application
Categories=Game;
EOF

echo "Установка завершена!"
echo "Для запуска сервера: $INSTALL_DIR/GameServer"
echo "Для запуска клиента: $INSTALL_DIR/seabattle_client"
echo "Или найдите 'Sea Battle' в меню приложений" 