#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QDebug>
#include <QCoreApplication>

// Функция для вывода сообщений в консоль с синим цветом
void writeToConsole(const QString& message) {
    qDebug() << "\033[34m" << message << "\033[0m"; // Используем qDebug для вывода в консоль Qt
}

// Класс для обработки каждого клиента в отдельном потоке
class ClientThread : public QThread {
    Q_OBJECT

public:
    ClientThread(qintptr socketDescriptor, QObject *parent = nullptr) :
        QThread(parent), socketDescriptor(socketDescriptor), socket(nullptr) {}

protected:
    void run() override {
        socket = new QTcpSocket(); // Создаем сокет в потоке
        if (!socket->setSocketDescriptor(socketDescriptor)) {
            qDebug() << "Ошибка создания сокета в потоке";
            delete socket;  // Освобождаем память в случае ошибки
            return;
        }

        writeToConsole(QString("Клиент подключен в потоке: ") + socket->peerAddress().toString() + ":" + QString::number(socket->peerPort()));

        connect(socket, &QTcpSocket::readyRead, this, &ClientThread::readData);
        connect(socket, &QTcpSocket::disconnected, this, &ClientThread::disconnected);
        connect(socket, &QThread::finished, this, &ClientThread::threadFinished); // Исправлено
        connect(socket, &QTcpSocket::errorOccurred, this, &ClientThread::socketError);

        exec(); // Запускаем цикл обработки событий в потоке (аналог QEventLoop::exec())
    }

signals:
    void dataReceived(const QByteArray& data, QTcpSocket* socket);
    void clientDisconnected(QTcpSocket* socket);
    void threadFinished(ClientThread* thread);

private slots:
    void readData() {
        if (!socket) return;

        QByteArray data = socket->readAll();
        writeToConsole(QString("Получено сообщение: ") + data + " от клиента " + socket->peerAddress().toString() + ":" + QString::number(socket->peerPort()));
        emit dataReceived(data, socket); // Отправляем данные для обработки в основном потоке
    }

    void disconnected() {
        if (!socket) return;

        writeToConsole(QString("Клиент отключился в потоке: ") + socket->peerAddress().toString() + ":" + QString::number(socket->peerPort()));
        emit clientDisconnected(socket); // Сообщаем основному потоку об отключении
        socket->disconnectFromHost();
        socket->close();

        // Не удаляем сокет здесь!  Он будет удален в threadFinished()
        quit(); // Завершаем цикл обработки событий в потоке
    }

    void threadFinished(){
        if(socket){
            socket->deleteLater();
            socket = nullptr;
        }
        emit threadFinished(this);  //Сигнал для удаление потока
    }

    void socketError(QAbstractSocket::SocketError socketError) {
        Q_UNUSED(socketError); // Предотвращаем предупреждение о неиспользованном параметре

        if (!socket) return;

        writeToConsole(QString("Ошибка сокета в потоке: ") + socket->errorString() + " от клиента " + socket->peerAddress().toString() + ":" + QString::number(socket->peerPort()));
        socket->disconnectFromHost();
        socket->close();
        quit(); // Завершаем цикл обработки событий в потоке
    }

private:
    qintptr socketDescriptor;
    QTcpSocket* socket; // Сокет клиента (теперь указатель, созданный в потоке)
};

// Класс сервера
class Server : public QTcpServer {
    Q_OBJECT

public:
    Server(QObject *parent = nullptr) : QTcpServer(parent) {}

public slots:
    void startServer() {
        if (!listen(QHostAddress::Any, 4400)) {
            qDebug() << "Не удалось запустить сервер";
        } else {
            writeToConsole("Сервер запущен и слушает порт 4400");
        }
    }

protected:
    void incomingConnection(qintptr socketDescriptor) override {
        ClientThread *thread = new ClientThread(socketDescriptor, this);
        connect(thread, &ClientThread::dataReceived, this, &Server::processData);
        connect(thread, &ClientThread::clientDisconnected, this, &Server::removeClient);
        connect(thread, &ClientThread::threadFinished,this,&Server::removeThread);
        thread->start();
        clients.append(thread);
    }

private slots:
    void processData(const QByteArray& data, QTcpSocket* socket) {
        // Обработка данных, полученных от клиента
        writeToConsole(QString("Обработка сообщения: ") + data + " от клиента " + socket->peerAddress().toString() + ":" + QString::number(socket->peerPort()));
        // Разбор сообщения, обновление состояния игры, отправка ответов клиентам
        // ...

        // Пример отправки ответа клиенту
        QString response = "Сервер получил ваше сообщение: " + QString(data);
        socket->write(response.toUtf8());
    }

    void removeClient(QTcpSocket* socket) {
        // Удаление отключившегося клиента из списка (по QTcpSocket*)
        for(int i = 0; i < clients.size(); ++i){
            if(clients[i]->socketDescriptor == socket->socketDescriptor()){
                //  clients[i]->quit();  //Не надо тут вызывать quit(), так как это убьет поток в момент, когда сигнал уже отправлен
                clients.removeAt(i);
                break;
            }
        }

    }

    void removeThread(ClientThread *thread){
        thread->deleteLater();

    }

private:
    QList<ClientThread*> clients;  //Храним потоки
};

#include "server_qt.moc" // Добавляем moc файл (генерируется автоматически)

int main(int argc, char *argv[]) {
    QCoreApplication a(argc, argv);  //Заменили QApplication на QCoreApplication, т.к. GUI не требуется

    Server server;
    server.startServer();

    return a.exec(); // Запускаем цикл обработки событий Qt
}