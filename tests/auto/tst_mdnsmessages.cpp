#include "mdnsmessage.h"

#include <QHostAddress>
#include <QTest>

Q_DECLARE_METATYPE(QHostAddress)

namespace MDNS {
namespace Tests {

class MessagesTest : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

private slots:
    void parseMessage_data();
    void parseMessage();

    void buildMessage_data();
    void buildMessage();

private:
    using RecordList = QList<QVariantList>;
};

void MessagesTest::parseMessage_data()
{
    const auto hostAddress = [](QString address) {
        return QVariant::fromValue(QHostAddress{address});
    };

    QTest::addColumn<QByteArray>("data");
    QTest::addColumn<QList<int>>("expectedHeaders");
    QTest::addColumn<RecordList>("expectedRecords");

    QTest::newRow("googlecast:q1")
            << QByteArray::fromHex("001e 0000"
                                   "0002 0000 0000 0000"
                                   "2a 5f25394535453743 3846343739383935 3236433942434439"              // #QR#0: PTR
                                   "   3544323430383446 3646304232374335 4544"
                                   "04 5f737562"
                                   "0b 5f676f6f676c6563 617374"
                                   "04 5f746370"
                                   "05 6c6f63616c"
                                   "00"
                                   "000c 0001"
                                   "c 03c"                                                              // #QR#1: PTR
                                   "000c 0001")
            << QList<int>{30, 0, 2, 0, 0, 0}
            << RecordList({{"_%9E5E7C8F47989526C9BCD95D24084F6F0B27C5ED._sub._googlecast._tcp.local.", Message::PTR, Message::IN, false},
                           {"_googlecast._tcp.local.", Message::PTR, Message::IN, false}});

    QTest::newRow("googlecast:q2")
            << QByteArray::fromHex("0000 0000"
                                   "0001 0000 0000 0000"
                                   "0b 5f676f6f676c6563 617374"                                         // QR#0: PTR
                                   "04 5f746370"
                                   "05 6c6f63616c"
                                   "00"
                                   "000c 0001")
            << QList<int>{0, 0, 1, 0, 0, 0}
            << RecordList({{"_googlecast._tcp.local.", Message::PTR, Message::IN, false}});

    QTest::newRow("googlecast:r1")
            << QByteArray::fromHex("0000 8400"
                                   "0000 0001 0000 0003"
                                   "0b 5f676f6f676c6563 617374"                                         // AN#0: PTR
                                   "04 5f746370"
                                   "05 6c6f63616c"
                                   "00"
                                   "000c 0001 00000078 0030"
                                   "2d 4252415649412d34 4b2d47422d346133 6365653731643366"
                                   "   3766383032396232 3461323662393032 6439373831"
                                   "c 00c"
                                   "c 02e"                                                              // AD#0: TXT
                                   "0010 8001 00001194 00aa"
                                   "2369643d34613363 6565373164336637 6638303239623234 6132366239303264"    // 0x00
                                   "393738312363643d 4632363543313338 3534314542303130 4338423638384430"    // 0x20
                                   "4142444246323637 03726d3d0576653d 30350f6d643d4252 4156494120344b20"    // 0x40
                                   "47421269633d2f73 657475702f69636f 6e2e706e670e666e 3d4b442d35355844"    // 0x60
                                   "383030350763613d 323035330473743d 300f62733d464138 4644303930453041"    // 0x80
                                   "31046e663d310372 733d"                                                  // 0xa0
                                   "c 02e"                                                              // AD#1: SRV
                                   "0021 8001 00000078 002d"
                                   "0000 0000 1f49"
                                   "24 3461336365653731 2d643366372d6638 30322d396232342d"
                                   "   6132366239303264 39373831"
                                   "c 01d"
                                   "c 126"                                                              // AD#2: A
                                   "0001 8001 00000078 0004"
                                   "c0a8b23c")
            << QList<int>{0, Message::IsResponse | Message::AuthoritativeAnswer, 0, 1, 0, 3}
            << RecordList({{"_googlecast._tcp.local.", Message::PTR, Message::IN, false, 120, "BRAVIA-4K-GB-4a3cee71d3f7f8029b24a26b902d9781._googlecast._tcp.local."},
                           {"BRAVIA-4K-GB-4a3cee71d3f7f8029b24a26b902d9781._googlecast._tcp.local.", Message::TXT, Message::IN, true, 4500},
                           {"BRAVIA-4K-GB-4a3cee71d3f7f8029b24a26b902d9781._googlecast._tcp.local.", Message::SRV, Message::IN, true, 120, 0, 0, 8009, "4a3cee71-d3f7-f802-9b24-a26b902d9781.local."},
                           {"4a3cee71-d3f7-f802-9b24-a26b902d9781.local.", Message::A, Message::IN, true, 120, hostAddress("192.168.178.60")}});

    QTest::newRow("androidtv:q1")
            << QByteArray::fromHex("0000 0000"
                                   "0002 0000 0003 0000"
                                   "2d 4252415649412d34 4b2d47422d346133 6365653731643366"              // QR#0: ANY
                                   "   3766383032396232 3461323662393032 6439373831"
                                   "0b 5f676f6f676c6563 617374"
                                   "04 5f746370"
                                   "05 6c6f63616c"
                                   "00"
                                   "00ff 0001"
                                   "24 3461336365653731 2d643366372d6638 30322d396232342d"              // QR#1: ANY
                                   "   6132366239303264 39373831"
                                   "c 04b"
                                   "00ff 0001"
                                   "c 00c"                                                              // AN#0: TXT
                                   "0010 8001 00001194 00aa"
                                   "2369643d34613363 6565373164336637 6638303239623234 6132366239303264"    // 0x00
                                   "393738312363643d 4632363543313338 3534314542303130 4338423638384430"    // 0x20
                                   "4142444246323637 03726d3d0576653d 30350f6d643d4252 4156494120344b20"    // 0x40
                                   "47421269633d2f73 657475702f69636f 6e2e706e670e666e 3d4b442d35355844"    // 0x60
                                   "383030350763613d 323035330473743d 300f62733d464138 4644303930453041"    // 0x80
                                   "31046e663d310372 733d"                                                  // 0xa0
                                   "c 00c"                                                              // AN#1: SRV
                                   "0021 8001 00000078 0008"
                                   "0000 0000 1f49"
                                   "c 056"
                                   "c 056"                                                              // AN#2: A
                                   "0001 8001 00000078 0004"
                                   "c0a8b23c"
                                 // 1 2 3 4 5 6 7 8  1 2 3 4 5 6 7 8  1 2 3 4 5 6 7 8  1 2 3 4 5 6 7 8  1 2 3 4 5 6 7 8  1 2 3 4 5 6 7 8  1 2 3 4 5 6 7 8
                )
            << QList<int>{0, 0, 2, 0, 3, 0}
            << RecordList({{"BRAVIA-4K-GB-4a3cee71d3f7f8029b24a26b902d9781._googlecast._tcp.local.", Message::ANY, Message::IN, false},
                           {"4a3cee71-d3f7-f802-9b24-a26b902d9781.local.", Message::ANY, Message::IN, false},
                           {"BRAVIA-4K-GB-4a3cee71d3f7f8029b24a26b902d9781._googlecast._tcp.local.", Message::TXT, Message::IN, true, 4500},
                           {"BRAVIA-4K-GB-4a3cee71d3f7f8029b24a26b902d9781._googlecast._tcp.local.", Message::SRV, Message::IN, true, 120, 0, 0, 8009, "4a3cee71-d3f7-f802-9b24-a26b902d9781.local."},
                           {"4a3cee71-d3f7-f802-9b24-a26b902d9781.local.", Message::A, Message::IN, true, 120, hostAddress("192.168.178.60")},
                          });

    QTest::newRow("androidtv:r2")
            << QByteArray::fromHex("0000 0000"
                                   "0004 0000 0004 0000"
                                   "13 6164622d35346134 3166303136303031 313233"                        // QR#0: ANY
                                   "04 5f616462"
                                   "04 5f746370"
                                   "05 6c6f63616c"
                                   "00"
                                   "00ff 0001"
                                   "0b 4b442d3535584438 303035"                                         // QR#1: ANY
                                   "10 5f616e64726f6964 747672656d6f7465"
                                   "c 025"
                                   "00ff 0001"
                                   "07 416e64726f6964"
                                   "c 02a"                                                              // QR#2: ANY
                                   "00ff 0001"
                                   "c 058"                                                              // QR#3: ANY
                                   "00ff 0001"
                                   "c 00c"                                                              // AN#0: SRV
                                   "0021 0001 00000078 0008"
                                   "0000 0000 15b3"
                                   "c 058"
                                   "c 035"                                                              // AN#1: SRV
                                   "0021 0001 00000078 0008"
                                   "0000 0000 1942"
                                   "c 058"
                                   "c 058"                                                              // AN#2: A
                                   "0001 0001 00000078 0004"
                                   "c0a8b23c"
                                   "c 058"                                                              // AN#2: AAAA
                                   "001c 0001 00000078 0010"
                                   "fe80000000000000124fa8fffe86d528")
            << QList<int>{0, 0, 4, 0, 4, 0}
            << RecordList({{"adb-54a41f016001123._adb._tcp.local.", Message::ANY, Message::IN, false},
                           {"KD-55XD8005._androidtvremote._tcp.local.", Message::ANY, Message::IN, false},
                           {"Android.local.", Message::ANY, Message::IN, false},
                           {"Android.local.", Message::ANY, Message::IN, false},
                           {"adb-54a41f016001123._adb._tcp.local.", Message::SRV, Message::IN, false, 120, 0, 0, 5555, "Android.local."},
                           {"KD-55XD8005._androidtvremote._tcp.local.", Message::SRV, Message::IN, false, 120, 0, 0, 6466, "Android.local."},
                           {"Android.local.", Message::A, Message::IN, false, 120, hostAddress("192.168.178.60")},
                           {"Android.local.", Message::AAAA, Message::IN, false, 120, hostAddress("fe80::124f:a8ff:fe86:d528")},
                          });

    QTest::newRow("androidtv:r1")
            << QByteArray::fromHex("0000 8400"
                                   "0000 000c 0000 0005"
                                   "13 6164622d35346134 3166303136303031 313233"                        // AN#0: TXT
                                   "04 5f616462"
                                   "04 5f746370"
                                   "05 6c6f63616c"
                                   "00"
                                   "0010 8001 00001194 0001"
                                   "00"                                                                     // 0x00
                                   "09 5f73657276696365 73"                                             // AN#1: PTR
                                   "07 5f646e732d7364"
                                   "04 5f756470"
                                   "c 02a"
                                   "000c 0001 00001194 0002"
                                   "c 020"
                                   "c 020"                                                              // AN#2: PTR
                                   "000c 0001 00001194 0002"
                                   "c00c"
                                   "c 00c"                                                              // AN#3: SRV
                                   "0021 8001 00000078 0010"
                                   "0000 0000 15b3"
                                   "07 416e64726f6964"
                                   "c 02a"
                                   "0b 4b442d3535584438 303035"                                         // AN#4: TXT
                                   "10 5f616e64726f6964 747672656d6f7465"                                   // 0x00
                                   "c 025"
                                   "0010 8001 00001194 0015"
                                   "1462743d34343a31 433a41383a37463a 31423a3632"
                                   "c 03c"                                                              // AN#5: PTR
                                   "000c 0001 00001194 0002"
                                   "c 097"
                                   "c 097"                                                              // AN#6: PTR
                                   "000c 0001 00001194 0002"
                                   "c 08b"
                                   "c 08b"                                                              // AN#7: SRV
                                   "0021 8001 00000078 0008"
                                   "0000 0000 1942"
                                   "c 081"
                                   "02 3630"                                                            // AN#8: PTR
                                   "03 313738"
                                   "03 313638"
                                   "03 313932"
                                   "07 696e2d61646472"
                                   "04 61727061"
                                   "00"
                                   "000c 8001 00000078 0002"
                                   "c 081"
                                   "01 38" "01 32" "01 35" "01 44"                                      // AN#9: PTR
                                   "01 36" "01 38" "01 45" "01 46"
                                   "01 46" "01 46" "01 38" "01 41"
                                   "01 46" "01 34" "01 32" "01 31"
                                   "01 30" "01 30" "01 30" "01 30"
                                   "01 30" "01 30" "01 30" "01 30"
                                   "01 30" "01 30" "01 30" "01 30"
                                   "01 30" "01 38" "01 45" "01 46"
                                   "03 697036"
                                   "c 110"
                                   "000c 8001 00000078 0002"
                                   "c 081"
                                   "c 081"                                                              // AN#10: A
                                   "0001 8001 00000078 0004"
                                   "c0a8b23c"
                                   "c 081"                                                              // AN#11: AAAA
                                   "001c 8001 00000078 0010"
                                   "fe80 0000 0000 0000 124f a8ff fe86 d528"
                                   "c 00c"                                                              // AD#0: NSEC
                                   "002f 8001 00001194 0009"
                                   "c 00c 00 05 0000800040"
                                   "c 08b"                                                              // AD#1: NSEC
                                   "002f 8001 00001194 0009"
                                   "c 08b 00 05 0000800040"
                                   "c 0f9"                                                              // AD#2: NSEC
                                   "002f 8001 00000078 0006"
                                   "c 0f9 00 02 0008"
                                   "c 122"                                                              // AD#3: NSEC
                                   "002f 8001 00000078 0006"
                                   "c 122 00 02 0008"
                                   "c 081"                                                              // AD#4: NSEC
                                   "002f 8001 00000078 0008"
                                   "c 081 00 04 40000008")
            << QList<int>{0, Message::IsResponse | Message::AuthoritativeAnswer, 0, 12, 0, 5}
            << RecordList({{"adb-54a41f016001123._adb._tcp.local.", Message::TXT, Message::IN, true, 4500},
                           {"_services._dns-sd._udp.local.", Message::PTR, Message::IN, false, 4500, "_adb._tcp.local."},
                           {"_adb._tcp.local.", Message::PTR, Message::IN, false, 4500, "adb-54a41f016001123._adb._tcp.local."},
                           {"adb-54a41f016001123._adb._tcp.local.", Message::SRV, Message::IN, true, 120, 0, 0, 5555, "Android.local."},
                           {"KD-55XD8005._androidtvremote._tcp.local.", Message::TXT, Message::IN, true, 4500},
                           {"_services._dns-sd._udp.local.", Message::PTR, Message::IN, false, 4500, "_androidtvremote._tcp.local."},
                           {"_androidtvremote._tcp.local.", Message::PTR, Message::IN, false, 4500, "KD-55XD8005._androidtvremote._tcp.local."},
                           {"KD-55XD8005._androidtvremote._tcp.local.", Message::SRV, Message::IN, true, 120, 0, 0, 6466, "Android.local."},
                           {"60.178.168.192.in-addr.arpa.", Message::PTR, Message::IN, true, 120, "Android.local."},
                           {"8.2.5.D.6.8.E.F.F.F.8.A.F.4.2.1.0.0.0.0.0.0.0.0.0.0.0.0.0.8.E.F.ip6.arpa.", Message::PTR, Message::IN, true, 120, "Android.local."},
                           {"Android.local.", Message::A, Message::IN, true, 120, hostAddress("192.168.178.60")},
                           {"Android.local.", Message::AAAA, Message::IN, true, 120, hostAddress("fe80::124f:a8ff:fe86:d528")},
                           {"adb-54a41f016001123._adb._tcp.local.", Message::NSEC, Message::IN, true, 4500},
                           {"KD-55XD8005._androidtvremote._tcp.local.", Message::NSEC, Message::IN, true, 4500},
                           {"60.178.168.192.in-addr.arpa.", Message::NSEC, Message::IN, true, 120},
                           {"8.2.5.D.6.8.E.F.F.F.8.A.F.4.2.1.0.0.0.0.0.0.0.0.0.0.0.0.0.8.E.F.ip6.arpa.", Message::NSEC, Message::IN, true, 120},
                           {"Android.local.", Message::NSEC, Message::IN, true, 120},
                          });
}

void MessagesTest::parseMessage()
{
    const QFETCH(QByteArray, data);
    const QFETCH(QList<int>, expectedHeaders);
    QFETCH(QList<QVariantList>, expectedRecords);

    QVERIFY(!data.isEmpty());
    const Message message{data};

    QCOMPARE(expectedHeaders.size(), 6);
    QCOMPARE(message.serial(), expectedHeaders[0]);
    QCOMPARE(message.flags(), static_cast<Message::Flags>(expectedHeaders[1]));
    QCOMPARE(message.questionCount(), expectedHeaders[2]);
    QCOMPARE(message.answerCount(), expectedHeaders[3]);
    QCOMPARE(message.authorityCount(), expectedHeaders[4]);
    QCOMPARE(message.additionalCount(), expectedHeaders[5]);

    for (int i = 0; i < message.questionCount(); ++i) {
        QVERIFY2(!expectedRecords.isEmpty(), QByteArray::number(i).constData());
        auto expectedFields = expectedRecords.takeFirst();
        const auto question = message.question(i);

        QVERIFY(!expectedFields.isEmpty());
        QCOMPARE(question.name().toByteArray(), expectedFields.takeFirst());

        QVERIFY(!expectedFields.isEmpty());
        QCOMPARE(question.type(), expectedFields.takeFirst());

        QVERIFY(!expectedFields.isEmpty());
        QCOMPARE(question.networkClass(), expectedFields.takeFirst());

        QVERIFY(!expectedFields.isEmpty());
        QCOMPARE(question.flush(), expectedFields.takeFirst());

        QCOMPARE(expectedFields, {});
    }

    struct RecordType {
        int (Message::*count)() const;
        Resource (Message::*lookup)(int) const;
    };

    const std::vector<RecordType> recordTypes = {
        {&Message::answerCount, &Message::answer},
        {&Message::authorityCount, &Message::authority},
        {&Message::additionalCount, &Message::additional},
    };
    for (const auto &rt: recordTypes) {
        for (int i = 0; i < (message.*rt.count)(); ++i) {
            QVERIFY2(!expectedRecords.isEmpty(), QByteArray::number(i).constData());
            auto expectedFields = expectedRecords.takeFirst();
            const auto resource = (message.*rt.lookup)(i);

            QVERIFY(!expectedFields.isEmpty());
            QCOMPARE(resource.name().toByteArray(), expectedFields.takeFirst());

            QVERIFY(!expectedFields.isEmpty());
            QCOMPARE(resource.type(), expectedFields.takeFirst());

            QVERIFY(!expectedFields.isEmpty());
            QCOMPARE(resource.networkClass(), expectedFields.takeFirst());

            QVERIFY(!expectedFields.isEmpty());
            QCOMPARE(resource.flush(), expectedFields.takeFirst());

            QVERIFY(!expectedFields.isEmpty());
            QCOMPARE(resource.timeToLife(), expectedFields.takeFirst());

            switch (resource.type()) {
            case Message::A:
            case Message::AAAA:
                QVERIFY(!expectedFields.isEmpty());
                QCOMPARE(resource.address(), expectedFields.takeFirst().value<QHostAddress>());
                break;

            case Message::PTR:
                QVERIFY(!expectedFields.isEmpty());
                QCOMPARE(resource.pointer().toByteArray(), expectedFields.takeFirst());
                break;

            case Message::SRV:
                QVERIFY(!expectedFields.isEmpty());
                QCOMPARE(resource.service().priority(), expectedFields.takeFirst());

                QVERIFY(!expectedFields.isEmpty());
                QCOMPARE(resource.service().weight(), expectedFields.takeFirst());

                QVERIFY(!expectedFields.isEmpty());
                QCOMPARE(resource.service().port(), expectedFields.takeFirst());

                QVERIFY(!expectedFields.isEmpty());
                QCOMPARE(resource.service().target().toByteArray(), expectedFields.takeFirst());
                break;

            default:
                qWarning("FIXME: Resource type %d not verified", resource.type());
                break;
            }

            QCOMPARE(expectedFields, {});
        }
    }

    QCOMPARE(expectedRecords, {});
}

void MessagesTest::buildMessage_data()
{
    QTest::addColumn<Message>("message");
    QTest::addColumn<QByteArray>("expectedData");

    QTest::newRow("empty")
            << MDNS::Message{}
            << QByteArray::fromHex("0000 0000"
                                   "0000 0000 0000 0000");

    QTest::newRow("xpresstrain")
            << MDNS::Message{}.
               addQuestion({"_http._tcp.local", MDNS::Message::PTR}).
               addQuestion({"_xpresstrain._tcp.local", MDNS::Message::PTR})
            << QByteArray::fromHex("0000 0000"
                                   "0002 0000 0000 0000"
                                   "05 5f68747470"
                                   "04 5f746370"
                                   "05 6c6f63616c"
                                   "00"
                                   "000c 0001"
                                   "0c 5f787072657373747261696e"
                                   "04 5f746370"
                                   "05 6c6f63616c"
                                   "00"
                                   "000c 0001");

    QTest::newRow("juicifer")
            << MDNS::Message{}.
               addQuestion({"juicifer.local", MDNS::Message::A})
            << QByteArray::fromHex("0000 0000"
                                   "0001 0000 0000 0000"
                                   "08 6a75696369666572"
                                   "05 6c6f63616c"
                                   "00"
                                   "0001 0001");
}

void MessagesTest::buildMessage()
{
    QFETCH(Message, message);
    QFETCH(QByteArray, expectedData);
    QCOMPARE(message.data(), expectedData);
}

}
}

QTEST_GUILESS_MAIN(MDNS::Tests::MessagesTest)

#include "tst_mdnsmessages.moc"
