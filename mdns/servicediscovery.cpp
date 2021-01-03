#include "mdnsmessage.h"

#include <QCoreApplication>
#include <QLoggingCategory>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QTimer>
#include <QUdpSocket>

using namespace std::literals;

namespace {
Q_LOGGING_CATEGORY(lcMDNS, "mdns")
}

int main(int argc, char *argv[])
{
    QCoreApplication app{argc, argv};

    const QVector queries = {
        MDNS::Message{}.
        addQuestion({"_http._tcp.local", MDNS::Message::PTR}).
        addQuestion({"_xpresstrain._tcp.local", MDNS::Message::PTR}).data(),
        MDNS::Message{}.
        addQuestion({"juicifer.local", MDNS::Message::A}).data(),
    };

    const auto timer = new QTimer;

    struct Protocol {
        QHostAddress bindAddress;
        QHostAddress multicastGroup;
    };

    for (const auto &p: {
         Protocol{QHostAddress::AnyIPv4, QHostAddress{"224.0.0.251"}},
         Protocol{QHostAddress::AnyIPv6, QHostAddress{"ff02::fb"}},
    }) {
        const auto socket = new QUdpSocket;

        app.connect(socket, &QUdpSocket::readyRead, &app, [socket, queries] {
            while (socket->hasPendingDatagrams()) {
                const auto reply = socket->receiveDatagram();

                if (!queries.contains(reply.data()) || true)
                    qCDebug(lcMDNS) << reply.senderAddress() << MDNS::Message{reply.data()};
            }
        });

        if (!socket->bind(p.bindAddress, 5353, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint))
            qWarning() << socket->errorString();

        const auto allInterfaces = QNetworkInterface::allInterfaces();
        for (const auto &iface: allInterfaces) {
            if (iface.type() == QNetworkInterface::Ethernet
                    || iface.type() == QNetworkInterface::Wifi) {
                if (iface.flags().testFlag(QNetworkInterface::IsRunning)
                        && iface.flags().testFlag(QNetworkInterface::CanMulticast)
                        && !socket->joinMulticastGroup(p.multicastGroup, iface))
                    qWarning() << iface.name() << socket->errorString();
            }
        }

        app.connect(timer, &QTimer::timeout, &app, [mcast = p.multicastGroup, socket, queries] {
            for (const auto &q: queries)
                socket->writeDatagram(q, mcast, 5353);
        });
    }

    timer->start(2s);
    return app.exec();
}
