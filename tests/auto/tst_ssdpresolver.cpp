/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2024 Mathias Hasselmann
 */

// QtNetworkCrumbs headers
#include "ssdpresolver.h"
#include "literals.h"

// Qt headers
#include <QTest>

Q_DECLARE_METATYPE(qnc::ssdp::NotifyMessage)

namespace qnc::ssdp::tests {

class ResolverTest : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

private slots:
    void testParseNotifyMessage_data()
    {
        QTest::addColumn<QDateTime>("now");
        QTest::addColumn<QByteArray>("data");
        QTest::addColumn<NotifyMessage>("expectedMessage");

        const auto now = "2024-09-10T22:34:33Z"_iso8601;

        QTest::newRow("empty")
                << now
                << ""_ba
                << NotifyMessage{};

        QTest::newRow("alive")
                << now
                << "NOTIFY * HTTP/1.1\r\n"
                   "Host: 239.255.255.250:1900\r\n"
                   "NT: blenderassociation:blender\r\n"
                   "NTS: ssdp:alive\r\n"
                   "USN: someunique:idscheme3\r\n"
                   "LOCATION: http://192.168.123.45:7890/dd.xml\r\n"
                   "LOCATION: http://192.168.123.45:7890/icon.png\r\n"
                   "AL: <blender:ixl><http://foo/bar>\r\n"
                   "Cache-Control: max-age = 7393\r\n"
                   "\r\n"_ba
                << NotifyMessage{
                   NotifyMessage::Type::Alive,
                   "someunique:idscheme3"_L1,
                   "blenderassociation:blender"_L1,
                   {"http://192.168.123.45:7890/dd.xml"_url, "http://192.168.123.45:7890/icon.png"_url},
                   {"blender:ixl"_url, "http://foo/bar"_url},
                   now.addSecs(7393)};

        QTest::newRow("byebye")
                << now
                << "NOTIFY * HTTP/1.1\r\n"
                   "Host: 239.255.255.250:1900\r\n"
                   "NT: blenderassociation:blender\r\n"
                   "NTS: ssdp:byebye\r\n"
                   "USN: someunique:idscheme3\r\n"
                   "\r\n"_ba
                << NotifyMessage{
                   NotifyMessage::Type::ByeBye,
                   "someunique:idscheme3"_L1,
                   "blenderassociation:blender"_L1};
    }

    void testParseNotifyMessage()
    {
        const QFETCH(QDateTime, now);
        const QFETCH(QByteArray, data);
        const QFETCH(NotifyMessage, expectedMessage);

        const auto &message = NotifyMessage::parse(data, now);

        QCOMPARE(message.type,          expectedMessage.type);
        QCOMPARE(message.serviceName,   expectedMessage.serviceName);
        QCOMPARE(message.serviceType,   expectedMessage.serviceType);
        QCOMPARE(message.locations,     expectedMessage.locations);
        QCOMPARE(message.altLocations,  expectedMessage.altLocations);
        QCOMPARE(message.expiry,        expectedMessage.expiry);
    }
};

} // namespace qnc::ssdp::tests

QTEST_GUILESS_MAIN(qnc::ssdp::tests::ResolverTest)

#include "tst_ssdpresolver.moc"
