QT       += core network gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# Настройки для временных файлов
MOC_DIR = build/moc
OBJECTS_DIR = build/obj
RCC_DIR = build/rcc
UI_DIR = build/ui

# Клиентская часть
SOURCES += \
    mainwindow.cpp \
    GameBoard.cpp \
    NetworkClient.cpp \
    main.cpp

HEADERS += \
    MainWIndow.h \
    GameBoard.h \
    NetworkClient.h

# Имя исполняемого файла
TARGET = seabattle_client

FORMS += mainwindow.ui

# Правила для развертывания
target.path = /usr/games/sea-battle
INSTALLS += target 