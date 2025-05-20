QT += core network
QT -= gui

CONFIG += c++11 console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += \
    gameserver.cpp \
    server.cpp

HEADERS += \
    gameserver.h

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target 