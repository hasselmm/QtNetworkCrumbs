/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2024 Mathias Hasselmann
 */
#include "mdnsurlfinder.h"

// QtNetworkCrumbs headers
#include "mdnsresolver.h"
#include "literals.h"

namespace qnc::mdns {
namespace {

auto finders()
{
    // http://www.dns-sd.org/ServiceTypes.html
    static auto s_finders = QHash<QString, UrlFinder::Function> {
        {"_afpovertcp._tcp"_L1, DefaultUrlFinder{u"afp",        548, u"path"}},
        {"_ftp._tcp"_L1,        DefaultUrlFinder{u"ftp",         21, u"path"}},
        {"_http._tcp"_L1,       DefaultUrlFinder{u"http",        80, u"path"}},
        {"_https._tcp"_L1,      DefaultUrlFinder{u"https",      443, u"path"}},
        {"_ipp._tcp"_L1,        PrinterUrlFinder{u"ipp",        631, u"rp"}},
        {"_ipps._tcp"_L1,       PrinterUrlFinder{u"ipps",       631, u"rp"}},
        {"_mqtt._tcp"_L1,       DefaultUrlFinder{u"mqtt",      1883, u"topic"}},
        {"_nfs._tcp"_L1,        DefaultUrlFinder{u"nfs",       2049, u"path"}},
        {"_printer._tcp"_L1,    DefaultUrlFinder{u"ftp",        515, u"queue"}},
        {"_rtsp._tcp"_L1,       DefaultUrlFinder{u"rtsp",       554, u"path"}},
        {"_rtsp._udp"_L1,       DefaultUrlFinder{u"rtspu",      554, u"path"}},
        {"_sftp-ssh._tcp"_L1,   DefaultUrlFinder{u"sftp",        22, u"path"}},
        {"_smb._tcp"_L1,        DefaultUrlFinder{u"smb",        139, u"path"}},
        {"_ssh._tcp"_L1,        DefaultUrlFinder{u"ssh",         22}},
        {"_telnet._tcp"_L1,     DefaultUrlFinder{u"telnet",      23}},
        {"_webdav._tcp"_L1,     DefaultUrlFinder{u"webdav",      80, u"path"}},
        {"_webdavs._tcp"_L1,    DefaultUrlFinder{u"webdavs",    443, u"path"}},
        // FIXME: googlecast?
    };

    return &s_finders;
}

} // namespace

void UrlFinder::add(const QString &serviceType, const Function &finder)
{
    finders()->insert(serviceType, finder);
}

QList<QUrl> UrlFinder::find(const ServiceDescription &service)
{
    if (const auto &findLocations = finders()->value(service.type()))
        return findLocations(service);

    return {};
}

QList<QUrl> DefaultUrlFinder::run(const ServiceDescription &service) const
{
    auto url = QUrl{};
    url.setScheme(m_urlScheme.toString());
    url.setHost(service.target());

    if (const auto port = service.port(); port != m_defaultPort)
        url.setPort(port);
    if (const auto &userName = service.info("u"_L1); !userName.isNull())
        url.setUserName(userName);
    if (const auto &password = service.info("p"_L1); !password.isNull())
        url.setPassword(password);

    if (!m_pathKey.isEmpty()) {
        auto path = service.info(m_pathKey.toString());

        if (!path.startsWith('/'_L1))
            path.prepend('/'_L1);

        url.setPath(path);
    } else {
        url.setPath("/"_L1);
    }

    return {url};
}

QList<QUrl> PrinterUrlFinder::run(const ServiceDescription &service) const
{
    auto locations = DefaultUrlFinder::run(service);

    if (const auto &url = service.info("adminurl"_L1); !url.isEmpty())
        locations += QUrl{url};
    if (const auto &uuid = service.info("DUUID"_L1); !uuid.isEmpty())
        locations += QUrl{"urn:uuid:"_L1 + uuid};

    return locations;
}

} // namespace qnc::mdns
