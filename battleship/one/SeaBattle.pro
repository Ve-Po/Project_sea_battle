QT       += core network gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

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
# Правила по умолчанию для развертывания
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target 