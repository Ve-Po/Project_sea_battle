QT += core network
QT -= gui

CONFIG += c++11 console
CONFIG -= app_bundle

TEMPLATE = app

# Пути к заголовочным файлам Qt
INCLUDEPATH += /usr/include/qt5
INCLUDEPATH += /usr/include/qt5/QtCore
INCLUDEPATH += /usr/include/qt5/QtNetwork

# Настройки компиляции
QMAKE_CXXFLAGS += -Wall -Wextra
QMAKE_CXXFLAGS_DEBUG += -g
QMAKE_CXXFLAGS_RELEASE += -O2

SOURCES += \
    server.cpp \
    gameserver.cpp

HEADERS += \
    gameserver.h

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target 