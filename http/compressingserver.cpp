#include <QCoreApplication>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtEndian>

#include <zlib.h>

namespace {

constexpr quint8 s_gzipHeader[] = {
    0x1f, 0x8b,                 // id
    0x08,                       // compression method (deflate)
    0x00,                       // flags
    0x00, 0x00, 0x00, 0x00,     // mtime
    0x00,                       // extra flags
    0xff,                       // operating system (unknown)
};

}

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
                    QByteArray trailer{8, Qt::Uninitialized};
                    const auto length = static_cast<uint>(content.length());
                    const auto checksum = crc32(0, reinterpret_cast<const quint8 *>(content.constData()), length);

                    qToLittleEndian(static_cast<quint32>(checksum), trailer.data());
                    qToLittleEndian(static_cast<quint32>(length), trailer.data() + 4);

                    compressed
                            = QByteArray::fromRawData(reinterpret_cast<const char *>(s_gzipHeader), sizeof s_gzipHeader)
                            + compressed.mid(6, compressed.length() - 10) // strip size, zlib header and trailer
                            + trailer;
                } else {
                    compressed = compressed.mid(4); // strip size
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

