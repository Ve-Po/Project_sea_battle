QT += core network
QT -= gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# Настройки для временных файлов
MOC_DIR = build/moc
OBJECTS_DIR = build/obj
RCC_DIR = build/rcc
UI_DIR = build/ui

TEMPLATE = app

SOURCES += \
    server.cpp \
    gameserver.cpp

HEADERS += \
    gameserver.h

TARGET = GameServer

# Правила для развертывания
target.path = /usr/games/sea-battle
INSTALLS += target 