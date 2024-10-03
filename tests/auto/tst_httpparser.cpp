/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2024 Mathias Hasselmann
 */

#include "httpparser.h"
#include "literals.h"

#include <QTest>

namespace qnc::http::tests {

class ParserTest : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

private slots:
    void testCaseInsensitiveEqual()
    {
        for (const auto &a : m_samples) {
            for (const auto &b : m_samples) {
                QCOMPARE(CaseInsensitive{a}, b);
                QCOMPARE(a, CaseInsensitive{b});

                QCOMPARE(CaseInsensitive{QString::fromUtf8(a)}, QString::fromUtf8(b));
                QCOMPARE(QString::fromUtf8(a), CaseInsensitive{QString::fromUtf8(b)});
            }
        }
    }

    void testCaseInsensitiveNotEqual()
    {
        for (const auto &a : m_samples) {
            QVERIFY(CaseInsensitive{a} != "whatever"_baview);
            QVERIFY(a != CaseInsensitive{"whatever"_ba});

            QVERIFY(CaseInsensitive{QString::fromUtf8(a)} != "whatever"_L1);
            QVERIFY(QString::fromUtf8(a) != CaseInsensitive{"whatever"_L1});
        }
    }

    void testCaseInsensitiveStartsWith()
    {
        for (const auto &a : m_samples) {
            for (const auto &b : m_samples) {
                QVERIFY2(CaseInsensitive{a}.startsWith(b.left(5)), (a + " / " + b).constData());

                const auto astr   = QString::fromUtf8(a);
                const auto bstr   = QString::fromUtf8(b);
                const auto substr = QStringView{bstr.cbegin(), 5};

                QVERIFY2(CaseInsensitive{astr}.startsWith(substr), qPrintable(astr + " / "_L1 + bstr));
            }
        }
    }

    void testCaseInsensitiveEndsWith()
    {
        for (const auto &a : m_samples) {
            for (const auto &b : m_samples) {
                QVERIFY(CaseInsensitive{a}.endsWith(b.mid(5)));

                const auto astr   = QString::fromUtf8(a);
                const auto bstr   = QString::fromUtf8(b);
                const auto substr = QStringView{bstr.cbegin() + 5, bstr.cend()};

                QVERIFY(CaseInsensitive{astr}.endsWith(substr));
            }
        }
    }

    void testCaseInsensitiveContains()
    {
        auto strList   = QStringList{};
        auto istrList = QList<CaseInsensitive<QString>>{};
        auto ibaList  = QList<CaseInsensitive<QByteArray>>{};

        std::transform(m_samples.cbegin(), m_samples.cend(), std::back_inserter(strList),
                       [](const QByteArray &ba) { return QString::fromUtf8(ba); });
        std::copy(m_samples.cbegin(), m_samples.cend(), std::back_inserter(ibaList));
        std::copy(strList.cbegin(), strList.cend(), std::back_inserter(istrList));

        for (const auto &a : m_samples) {
            QVERIFY(m_samples.contains(CaseInsensitive{a}));
            QVERIFY(ibaList.contains(a));

            const auto astr = QString::fromUtf8(a);

            QVERIFY(strList.contains(CaseInsensitive{astr}));
            QVERIFY(istrList.contains(astr));
        }
    }

    void testDateTime_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QDateTime>("expectedDateTime");

        // examples from https://www.rfc-editor.org/rfc/rfc9110#section-5.6.7
        QTest::newRow("RFC1123") << "Sun, 06 Nov 1994 08:49:37 GMT"  << "1994-11-06T08:49:37Z"_iso8601;
        QTest::newRow("RFC850")  << "Sunday, 06-Nov-94 08:49:37 GMT" << "1994-11-06T08:49:37Z"_iso8601;
        QTest::newRow("asctime") << "Sun Nov  6 08:49:37 1994"       << "1994-11-06T08:49:37Z"_iso8601;
    }

    void testDateTime()
    {
        const QFETCH(QString, text);
        const QFETCH(QDateTime, expectedDateTime);
        QCOMPARE(parseDateTime(text), expectedDateTime);
    }

    void testExpiryDateTime_data()
    {
        QTest::addColumn<QDateTime >("now");
        QTest::addColumn<QByteArray>("cacheControl");
        QTest::addColumn<QByteArray>("expires");
        QTest::addColumn<QDateTime >("expectedDateTime");

        const auto now = "1994-11-06T08:49:37Z"_iso8601;
        const auto expires = "Sun, 06 Nov 1994 08:54:37 GMT"_ba;
        const auto empty = ""_ba;

        QTest::addRow("nothing")  << now << empty                     << empty   << QDateTime{};
        QTest::addRow("no-cache") << now << "no-cache"_ba             << empty   << now;
        QTest::addRow("max-age")  << now << "max-age=60"_ba           << empty   << now.addSecs(60);
        QTest::addRow("expires")  << now << empty                     << expires << now.addSecs(300);
        QTest::addRow("all")      << now << "max-age=60, no-cache"_ba << expires << now;
    }

    void testExpiryDateTime()
    {
        const QFETCH(QByteArray, cacheControl);
        const QFETCH(QByteArray, expires);
        const QFETCH(QDateTime,  now);
        const QFETCH(QDateTime,  expectedDateTime);

        QCOMPARE(expiryDateTime(cacheControl, expires, now), expectedDateTime);
    }

    void testParseRequest()
    {
        const auto &message = Message::parse("M-SEARCH * HTTP/1.1\r\n"
                                             "HOST: 239.255.255.250:1900\r\n"
                                             "MAN: \"ssdp:discover\"\r\n"
                                             "MX: 1\r\n"
                                             "ST: upnp:rootdevice\r\n"
                                             "\r\n"_ba);

        QCOMPARE(message.type(),  Message::Type::Request);
        QCOMPARE(message.protocol(),          "HTTP/1.1");
        QCOMPARE(message.verb(),              "M-SEARCH");
        QCOMPARE(message.resource(),                 "*");
        QCOMPARE(message.statusCode().has_value(), false);
        QCOMPARE(message.statusPhrase(),              "");
        QCOMPARE(message.headers().size(),             4);

        QCOMPARE(message.headers().at(0).first,  "Host");
        QCOMPARE(message.headers().at(0).second, "239.255.255.250:1900");
        QCOMPARE(message.headers().at(1).first,  "MAN");
        QCOMPARE(message.headers().at(1).second, "\"ssdp:discover\"");
        QCOMPARE(message.headers().at(2).first,  "MX");
        QCOMPARE(message.headers().at(2).second, "1");
        QCOMPARE(message.headers().at(3).first,  "ST");
        QCOMPARE(message.headers().at(3).second, "upnp:rootdevice");
    }

    void testParseResponse()
    {
        const auto &message = Message::parse("HTTP/1.1 200 OK\r\n"
                                             "Cache-Control: max-age=1800\r\n"
                                             "Location: http://192.168.0.4:49000/servicedesc.xml\r\n"
                                             "Server: Hyper 6000 UPnP/1.0 Company Hyper 6000 1.2.3\r\n"
                                             "Ext: \r\n"
                                             "ST: upnp:rootdevice\r\n"
                                             "USN: uuid:285fe440-2eee-4a0e-b11c-d051f4caa274:upnp:rootdevice\r\n"
                                             "\r\n"_ba);

        QCOMPARE(message.type(), Message::Type::Response);
        QCOMPARE(message.protocol(),          "HTTP/1.1");
        QCOMPARE(message.verb(),                      "");
        QCOMPARE(message.resource(),                  "");
        QCOMPARE(message.statusCode(),               200);
        QCOMPARE(message.statusPhrase(),            "OK");
        QCOMPARE(message.headers().size(),             6);

        QCOMPARE(message.headers().at(0).first,  "Cache-Control");
        QCOMPARE(message.headers().at(0).second, "max-age=1800");
        QCOMPARE(message.headers().at(1).first,  "Location");
        QCOMPARE(message.headers().at(1).second, "http://192.168.0.4:49000/servicedesc.xml");
        QCOMPARE(message.headers().at(2).first,  "Server");
        QCOMPARE(message.headers().at(2).second, "Hyper 6000 UPnP/1.0 Company Hyper 6000 1.2.3");
        QCOMPARE(message.headers().at(3).first,  "Ext");
        QCOMPARE(message.headers().at(3).second, "");
        QCOMPARE(message.headers().at(4).first,  "ST");
        QCOMPARE(message.headers().at(4).second, "upnp:rootdevice");
        QCOMPARE(message.headers().at(5).first,  "USN");
        QCOMPARE(message.headers().at(5).second, "uuid:285fe440-2eee-4a0e-b11c-d051f4caa274:upnp:rootdevice");
    }

private:
    const QByteArrayList m_samples = { "cache-control"_ba, "Cache-Control"_ba, "CACHE-CONTROL"_ba };
};

} // namespace qnc::mdns::tests

QTEST_GUILESS_MAIN(qnc::http::tests::ParserTest)

#include "tst_httpparser.moc"
