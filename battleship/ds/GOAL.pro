QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000   

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    gameboard.cpp \
    networkclient.cpp

HEADERS += \
    mainwindow.h \
    gameboard.h \
    networkclient.h

FORMS += \
    mainwindow.ui
