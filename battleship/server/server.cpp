#include "gameserver.h"
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDebug>

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    QCommandLineParser parser;
    parser.setApplicationDescription("UDP Game Server");
    parser.addHelpOption();
    QCommandLineOption portOption(QStringList() << "p" << "port", "Port to listen on", "port", "12345");
    parser.addOption(portOption);
    parser.process(app);

    quint16 port = parser.value(portOption).toUShort();
    GameServer server;
    if (!server.start(port)) {
        qDebug() << "Не удалось запустить сервер";
        return 1;
    }

    return app.exec();
} 