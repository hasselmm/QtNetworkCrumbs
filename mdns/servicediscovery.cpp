#include <QCoreApplication>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QTimer>
#include <QUdpSocket>

#include <QtEndian>

using namespace std::literals;

QByteArray operator"" _hex(const char *str, size_t len)
{
    return QByteArray::fromHex({str, static_cast<int>(len)});
}

QDebug operator<<(QDebug debug, QNetworkAddressEntry entry)
{
    return debug << entry.ip();
}

quint16 toUInt16(const char *p)
{
    return qFromBigEndian<quint16>(p);
}

quint32 toUInt32(const char *p)
{
    return qFromBigEndian<quint32>(p);
}

const char *parseName(const char *const header, const char *p, QByteArray *name)
{
    while (const auto len = *p++) {
        switch (len & 0xc0) {
        case 0x00:
            if (name) {
                name->append(p, len);
                name->append('.');
            }
            p += len;
            break;

        case 0xc0:
            parseName(header, header + (toUInt16(p - 1) & 0x3fff), name);
            return ++p;

        default:
            return {};
        }

    }

    return p;
};

QByteArray parseName(const char *const header, const char *p, const char **end)
{
    QByteArray name;
    *end = parseName(header, p, &name);
    return name;
}

const char *parseQuestion(const char *const header, const char *p)
{
    const auto name = parseName(header, p, &p);

    if (!p) {
        qWarning("bad name");
        return {};
    }

    const auto type = toUInt16(p);
    const auto cls = toUInt16(p + 2) & 0x7fff;
    const auto flush = !!(toUInt16(p + 2) & 0x8000);

    qInfo(" QQ(type=%d, class=%d, flush=%d, name=\"%s\")",
          type, cls, flush, name.constData());

    return p + 4;
}

const char *parseResource(const char *const header, const char *p, const char *section)
{
    const auto name = parseName(header, p, &p);

    if (!p) {
        qWarning("bad name");
        return {};
    }

    const auto type = toUInt16(p);
    const auto cls = toUInt16(p + 2) & 0x7fff;
    const auto flush = !!(toUInt16(p + 2) & 0x8000);
    const auto ttl = toUInt32(p + 4);
    const auto len = toUInt16(p + 8);

    p += 10;

    qInfo(" %s(type=%d, class=%d, flush=%d, name=\"%s\", ttl=%d, len=%d)",
          section, type, cls, flush, name.constData(), ttl, len);

    if (type == 1) {
        const auto ip = reinterpret_cast<const quint8 *>(p);
        qInfo(" - A(\"%d.%d.%d.%d\")", ip[0], ip[1], ip[2], ip[3]);
    } else if (type == 12) {
        const char *end;
        const auto name = parseName(header, p, &end);
        qInfo(" - PTR(\"%s\")", name.constData());
    } else if (type == 16) {
        const auto txt = QByteArray::fromRawData(p, len);
        qInfo(" - TXT(\"%s\")", txt.toPercentEncoding("=").constData());
    } else if (type == 28) {
        const auto ip = reinterpret_cast<const quint8 *>(p);
        qInfo(" - AAAA(\"%ls\")", qUtf16Printable(QHostAddress{ip}.toString()));
    } else if (type == 33) {
        const char *end;
        const auto priority = toUInt16(p);
        const auto weight = toUInt16(p + 2);
        const auto port = toUInt16(p + 4);
        const auto target = parseName(header, p + 6, &end);
        qInfo(" - SRV(priority=%d, weight=%d, port=%d, target=\"%s\")", priority, weight, port, target.constData());
    } else if (type == 47) {
        qInfo(" - NSEC()");
    }

    return p + len;
}

bool parseMessage(QByteArray data)
{
    const auto header = data.constData();
    const auto flags = toUInt16(header + 2);

    const auto qqCount = toUInt16(header + 4);
    const auto anCount = toUInt16(header + 6);
    const auto nsCount = toUInt16(header + 8);
    const auto arCount = toUInt16(header + 10);

    qInfo("%s", data.toHex().constData());
    qInfo(" flags=%02x, qq=%d, an=%d, ns=%d, ar=%d", flags, qqCount, anCount, nsCount, arCount);

    auto p = header + 12;

    for (int i = 0; i < qqCount; ++i) {
        if (!(p = parseQuestion(header, p)))
            return false;
    }

    for (int i = 0; i < anCount; ++i) {
        if (!(p = parseResource(header, p, "AN")))
            return false;
    }

    for (int i = 0; i < nsCount; ++i) {
        if (!(p = parseResource(header, p, "NS")))
            return false;
    }

    for (int i = 0; i < arCount; ++i) {
        if (!(p = parseResource(header, p, "AR")))
            return false;
    }

    return data.constEnd() == p;
}

int main(int argc, char *argv[])
{
    QCoreApplication app{argc, argv};

    parseMessage("0000000000010000000000000b5f676f6f676c6563617374045f746370056c6f63616c00000c0001"_hex);
    parseMessage("0000840000000001000000030b5f676f6f676c6563617374045f746370056c6f63616c00000c00010000007800302d4252415649412d344b2d47422d3461336365653731643366376638303239623234613236623930326439373831c00cc02e001080010000119400aa2369643d34613363656537316433663766383032396232346132366239303264393738312363643d463236354331333835343145423031304338423638384430414244424632363703726d3d0576653d30350f6d643d42524156494120344b2047421269633d2f73657475702f69636f6e2e706e670e666e3d4b442d35355844383030350763613d323035330473743d300f62733d464138464430393045304131046e663d310372733dc02e0021800100000078002d000000001f492434613363656537312d643366372d663830322d396232342d613236623930326439373831c01dc12600018001000000780004c0a8b23c"_hex);
    parseMessage("000084000000000c00000005136164622d353461343166303136303031313233045f616462045f746370056c6f63616c000010800100001194000100095f7365727669636573075f646e732d7364045f756470c02a000c0001000011940002c020c020000c0001000011940002c00cc00c002180010000007800100000000015b307416e64726f6964c02a0b4b442d3535584438303035105f616e64726f6964747672656d6f7465c025001080010000119400151462743d34343a31433a41383a37463a31423a3632c03c000c0001000011940002c097c097000c0001000011940002c08bc08b00218001000000780008000000001942c08102363003313738033136380331393207696e2d61646472046172706100000c8001000000780002c0810138013201350144013601380145014601460146013801410146013401320131013001300130013001300130013001300130013001300130013001380145014603697036c110000c8001000000780002c081c08100018001000000780004c0a8b23cc081001c8001000000780010fe80000000000000124fa8fffe86d528c00c002f8001000011940009c00c00050000800040c08b002f8001000011940009c08b00050000800040c0f9002f8001000000780006c0f900020008c122002f8001000000780006c12200020008c081002f8001000000780008c081000440000008"_hex);
    parseMessage("0000000000020000000300002d4252415649412d344b2d47422d34613363656537316433663766383032396232346132366239303264393738310b5f676f6f676c6563617374045f746370056c6f63616c0000ff00012434613363656537312d643366372d663830322d396232342d613236623930326439373831c04b00ff0001c00c001080010000119400aa2369643d34613363656537316433663766383032396232346132366239303264393738312363643d463236354331333835343145423031304338423638384430414244424632363703726d3d0576653d30350f6d643d42524156494120344b2047421269633d2f73657475702f69636f6e2e706e670e666e3d4b442d35355844383030350763613d323035330473743d300f62733d464138464430393045304131046e663d310372733dc00c00218001000000780008000000001f49c056c05600018001000000780004c0a8b23c"_hex);
    parseMessage("000000000004000000040000136164622d353461343166303136303031313233045f616462045f746370056c6f63616c0000ff00010b4b442d3535584438303035105f616e64726f6964747672656d6f7465c02500ff000107416e64726f6964c02a00ff0001c05800ff0001c00c002100010000007800080000000015b3c058c03500210001000000780008000000001942c058c05800010001000000780004c0a8b23cc058001c0001000000780010fe80000000000000124fa8fffe86d528"_hex);

    const QVector<QByteArray> queries = {
        "00 00"                     // Transaction ID"
        "00 00"                     // Flags (Query)
        "00 02"                     // Number of questions
        "00 00"                     // Number of answers
        "00 00"                     // Number of authority resource records
        "00 00"                     // Number of additional resource records
        "05 5f 68 74 74 70"         // "_http"
        "04 5f 74 63 70"            // "_tcp"
        "05 6c 6f 63 61 6c"         // "local"
        "00"                        // Terminator
        "00 0c"                     // Type (PTR record)
        "00 01"                     // Class (INternet)
        "0c 5f 78 70 72 65 73 73 74 72 61 69 6e"
                                    // "_xpresstrain"
        "04 5f 74 63 70"            // "_tcp"
        "05 6c 6f 63 61 6c"         // "local"
        "00"                        // Terminator
        "00 0c"                     // Type (PTR record)
        "00 01"                     // Class (INternet)
        ""_hex,

        "00 00"                         // Transaction ID"
        "00 00"                         // Flags (Query)
        "00 01"                         // Number of questions
        "00 00"                         // Number of answers
        "00 00"                         // Number of authority resource records
        "00 00"                         // Number of additional resource records
        "08 6a 75 69 63 69 66 65 72"    // "Juicifer"
        "05 6c 6f 63 61 6c"             // "local"
        "00"                            // Terminator
        "00 01"                         // Type (A record)
        "00 01"                         // Class (INternet)
        ""_hex,
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
                if (!queries.contains(reply.data()) || true) {
                    qInfo() << reply.senderAddress();
                    parseMessage(reply.data());
                }
            }
        });

        if (!socket->bind(p.bindAddress, 5353, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint))
            qWarning() << socket->errorString();

        for (const auto &iface: QNetworkInterface::allInterfaces()) {
            if (!socket->joinMulticastGroup(p.multicastGroup, iface))
                qWarning() << iface.name() << socket->errorString();
        }

        app.connect(timer, &QTimer::timeout, &app, [mcast = p.multicastGroup, socket, queries] {
            for (const auto &q: queries)
                socket->writeDatagram(q, mcast, 5353);
        });
    }

    timer->start(2s);
    return app.exec();
}

