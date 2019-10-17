#include <QCoreApplication>
#include <QTcpServer>
#include <QTcpSocket>

class CompressingServer : public QCoreApplication
{
public:
    using QCoreApplication::QCoreApplication;
    int run();
};

int CompressingServer::run()
{
    const auto server = new QTcpServer{this};

    connect(server, &QTcpServer::newConnection, this, [server] {
        while (const auto client = server->nextPendingConnection()) {
            client->waitForReadyRead();
            const auto request = client->readLine().trimmed();

            while (!client->atEnd()) {
                const auto header = client->readLine().trimmed();
                if (header.isEmpty())
                    break;
            }

            if (request.startsWith("GET /")) {
                const auto gzip = request.startsWith("GET /gzip");
                const QByteArray content = "Hello World! How are you?";
                auto compressed = qCompress(content);

                if (gzip) {
                    compressed = QByteArray::fromHex("1f8b 08 00 00000000 00 ff")
                            + compressed.mid(6, compressed.length() - 10)
                            + QByteArray::fromHex("efd37ef7 19000000");
                } else {
                    compressed = compressed.mid(4);
                }

                client->write("HTTP/1.0 200 OK\r\n");
                client->write(gzip ? "Content-Encoding: gzip\r\n" : "Content-Encoding: deflate\r\n");
                client->write("Content-Type: text/plain\r\n");
                client->write("Content-Length: " + QByteArray::number(compressed.length()) + "\r\n");
                client->write("Connection: close\r\n");
                client->write("\r\n");
                client->write(compressed);
            }

            client->flush();
            client->close();

            client->deleteLater();
        }
    });

    server->listen({}, 8080);
    return exec();
}

int main(int argc, char *argv[])
{
    return CompressingServer{argc, argv}.run();
}

