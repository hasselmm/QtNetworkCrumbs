/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2021 Mathias Hasselmann
 */
#include "mdnsresolver.h"

// MDNS headers
#include "mdnsmessage.h"

// Qt headers
#include <QHostAddress>
#include <QLoggingCategory>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QTimer>
#include <QUdpSocket>

// STL headers
#include <unordered_map>

namespace MDNS {

namespace {

using namespace std::chrono_literals;

Q_LOGGING_CATEGORY(lcResolver, "mdns.resolver")

constexpr auto s_mdnsPort = 5353;

auto multicastGroup(QUdpSocket *socket)
{
    switch (socket->localAddress().protocol()) {
    case QUdpSocket::IPv4Protocol:
        return QHostAddress{"224.0.0.251"};
    case QUdpSocket::IPv6Protocol:
        return QHostAddress{"ff02::fb"};
    default:
        Q_UNREACHABLE();
        return QHostAddress{};
    }
};

auto normalizedHostName(QByteArray name, QString domain)
{
    auto normalizedName = QString::fromLatin1(name);

    if (normalizedName.endsWith('.'))
        normalizedName.truncate(normalizedName.length() - 1);
    if (normalizedName.endsWith('.' + domain))
        normalizedName.truncate(normalizedName.length() - domain.length() - 1);

    return normalizedName;
}

auto qualifiedHostName(QString name, QString domain)
{
    if (name.endsWith('.'))
        name.truncate(name.length() - 1);
    else if (const auto domainSuffix = '.' + domain; !name.endsWith(domainSuffix))
        name.append(domainSuffix);

    return name;
}

auto encodedHostName(QString hostName)
{
    return hostName.toLatin1(); // FIXME: encode non-ASCII characters (issue #5)
}

auto decodedHostName(QByteArray hostName)
{
    return QString::fromLatin1(hostName); // FIXME: encode non-ASCII characters (issue #5)
}

template<class Record>
auto isAddressRecord(Record record)
{
    return record.type() == Message::A || record.type() == Message::AAAA;
}

auto isServiceQuery(Question question)
{
    return question.type() == Message::PTR && !question.name().endsWith({"arpa"});
}

} // namespace

ServiceDescription::ServiceDescription(QString domain, QByteArray name, ServiceRecord service, QByteArray info)
    : m_name{normalizedHostName(name, domain)}
    , m_target{normalizedHostName(service.target().toByteArray(), domain)}
    , m_port{service.port()}
    , m_priority{service.priority()}
    , m_weight{service.weight()}
    , m_info{std::move(info)}
{
    if (const auto separator = m_name.indexOf('.'); separator >= 0) {
        m_type = m_name.mid(separator + 1);
        m_name.truncate(separator);
    }
}

Resolver::Resolver(QObject *parent)
    : QObject{parent}
    , m_timer{new QTimer{this}}
    , m_domain{"local"}
{
    createSockets();
    m_timer->callOnTimeout(this, &Resolver::onTimeout);
    QTimer::singleShot(0, this, &Resolver::onTimeout);
    m_timer->start(2s);
}

void Resolver::setDomain(QString domain)
{
    if (std::exchange(m_domain, domain) != domain)
        emit domainChanged(m_domain);
}

QString Resolver::domain() const
{
    return m_domain;
}

void Resolver::setInterval(std::chrono::milliseconds ms)
{
    if (intervalAsDuration() != ms) {
        m_timer->setInterval(ms);
        emit intervalChanged(interval());
    }
}

void Resolver::setInterval(int ms)
{
    if (interval() != ms) {
        m_timer->setInterval(ms);
        emit intervalChanged(interval());
    }
}

std::chrono::milliseconds Resolver::intervalAsDuration() const
{
    return m_timer->intervalAsDuration();
}

int Resolver::interval() const
{
    return m_timer->interval();
}

QStringList Resolver::hostNameQueries() const
{
    return m_hostNameQueries;
}

QStringList Resolver::serviceQueries() const
{
    return m_serviceQueries;
}

QList<Message> Resolver::queries() const
{
    const auto makeMessage = [](QByteArray data) {
        return Message{std::move(data)};
    };

    QList<Message> messages;
    messages.reserve(m_queries.size());
    std::transform(m_queries.begin(), m_queries.end(),
                   std::back_inserter(messages), makeMessage);
    return messages;
}

bool Resolver::lookupHostNames(QStringList hostNames)
{
    MDNS::Message message;

    for (const auto &name: hostNames) {
        const auto qualifiedName = qualifiedHostName(name, m_domain);
        if (!m_hostNameQueries.contains(qualifiedName)) {
            const auto encodedName = encodedHostName(qualifiedName);
            message.addQuestion({encodedName, MDNS::Message::AAAA});
            message.addQuestion({encodedName, MDNS::Message::A});
        }
    }

    return lookup(message);
}

bool Resolver::lookupServices(QStringList serviceTypes)
{
    MDNS::Message message;

    for (const auto &type: serviceTypes) {
        const auto qualifiedName = qualifiedHostName(type, m_domain);
        if (!m_serviceQueries.contains(qualifiedName)) {
            const auto encodedName = encodedHostName(qualifiedName);
            message.addQuestion({encodedName, MDNS::Message::PTR});
        }
    }

    return lookup(message);
}

bool Resolver::lookup(Message query)
{
    if (query.questionCount() == 0)
        return false;

    // remember the raw query data
    const auto data = query.data();
    if (m_queries.contains(data))
        return false;

    m_queries.append(std::move(data));

    // extract host name and service types queries
    const auto initialHostNameQueryCount = m_hostNameQueries.count();
    const auto initialServiceQueryCount = m_serviceQueries.count();

    for (const auto &question: query.questions()) {
        if (isAddressRecord(question)) {
            const auto decodedName = decodedHostName(question.name().toByteArray());
            if (!m_hostNameQueries.contains(decodedName))
                m_hostNameQueries.append(decodedName);
        }

        if (isServiceQuery(question)) {
            const auto decodedName = decodedHostName(question.name().toByteArray());
            if (!m_serviceQueries.contains(decodedName))
                m_serviceQueries.append(decodedName);
        }
    }

    if (initialHostNameQueryCount != m_hostNameQueries.count())
        emit hostNameQueriesChanged(m_hostNameQueries);
    if (initialServiceQueryCount != m_serviceQueries.count())
        emit serviceQueriesChanged(m_serviceQueries);

    emit queriesChanged();
    return true;
}

bool Resolver::isOwnMessage(QNetworkDatagram message) const
{
    if (message.senderPort() != s_mdnsPort)
        return false;
    if (!m_ownAddresses.contains(message.senderAddress()))
        return false;

    return m_queries.contains(message.data());
}

void Resolver::createSockets()
{
    for (const QHostAddress &address: {QHostAddress::AnyIPv4, QHostAddress::AnyIPv6}) {
        auto socket = std::make_unique<QUdpSocket>(this);

        if (!socket->bind(address, s_mdnsPort, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
            qCWarning(lcResolver, "Could not bind mDNS socket to %ls: %ls",
                      qUtf16Printable(address.toString()),
                      qUtf16Printable(socket->errorString()));
            continue;
        }

        connect(socket.get(), &QUdpSocket::readyRead,
                this, [this, socket = socket.get()] {
            onReadyRead(socket);
        });

        m_sockets.append(socket.release());
    }
}

void Resolver::scanNetworkInterfaces()
{
    const auto isSupportedType = [](const auto type) {
        return type == QNetworkInterface::Ethernet
                || type == QNetworkInterface::Wifi;
    };

    const auto hasMulticastFlags = [](const auto flags) {
        return flags.testFlag(QNetworkInterface::IsRunning)
                && flags.testFlag(QNetworkInterface::CanMulticast);
    };

    QList<QHostAddress> ownAddresses;

    const auto allInterfaces = QNetworkInterface::allInterfaces();
    for (const auto &iface: allInterfaces) {
        if (isSupportedType(iface.type())
                && hasMulticastFlags(iface.flags())) {
            const auto allAddresses = iface.allAddresses();
            for (const auto &address: allAddresses) {
                if (!ownAddresses.contains(address))
                    ownAddresses.append(address);
            }

            for (const auto socket: qAsConst(m_sockets)) {
                const auto group = multicastGroup(socket);
                if (socket->joinMulticastGroup(group, iface)) {
                    qCDebug(lcResolver, "Multicast group %ls joined",
                            qUtf16Printable(group.toString()));
                } else if (socket->error() != QUdpSocket::UnknownSocketError) {
                    qCWarning(lcResolver, "Could not join multicast group %ls: %ls",
                              qUtf16Printable(group.toString()),
                              qUtf16Printable(socket->errorString()));
                }
            }
        }
    }

    m_ownAddresses = std::move(ownAddresses);
}

void Resolver::submitQueries() const
{
    for (const auto socket: m_sockets) {
        const auto group = multicastGroup(socket);

        for (const auto &data: m_queries)
            socket->writeDatagram(data, group, 5353);
    }
}

void Resolver::onReadyRead(QUdpSocket *socket)
{
    while (socket->hasPendingDatagrams()) {
        if (const auto received = socket->receiveDatagram(); !isOwnMessage(received)) {
            const MDNS::Message message{received.data()};

            std::unordered_map<QByteArray, QList<QHostAddress>> resolvedAddresses;
            std::unordered_map<QByteArray, ServiceRecord> resolvedServices;
            std::unordered_map<QByteArray, QByteArray> resolvedText;

            for (int i = 0; i < message.responseCount(); ++i) {
                const auto response = message.response(i);

                if (const auto address = response.address(); !address.isNull()) {
                    auto &knownAddresses = resolvedAddresses[response.name().toByteArray()];
                    if (!knownAddresses.contains(address))
                        knownAddresses.append(address);
                } else if (const auto service = response.service(); !service.isNull()) {
                    resolvedServices.insert({response.name().toByteArray(), service});
                } else if (const auto text = response.text(); !text.isNull()) {
                    resolvedText.insert({response.name().toByteArray(), text});
                }
            }

            for (const auto &[name, service]: resolvedServices)
                emit serviceResolved({m_domain, name, service, resolvedText[name]});
            for (const auto &[name, addresses]: resolvedAddresses)
                emit hostNameResolved(normalizedHostName(name, m_domain), addresses);

            emit messageReceived(std::move(message));
        }
    }
}

void Resolver::onTimeout()
{
    scanNetworkInterfaces();
    submitQueries();
}

} // namespace MDNS

QDebug operator<<(QDebug debug, const MDNS::ServiceDescription &service)
{
    const QDebugStateSaver saver{debug};

    if (debug.verbosity() >= QDebug::DefaultVerbosity)
        debug.nospace() << service.staticMetaObject.className();

    return debug.nospace()
            << "(" << service.name()
            << ", type=" << service.type()
            << ", target=" << service.target()
            << ", port=" << service.port()
            << ", priority=" << service.priority()
            << ", weight=" << service.weight()
            << ", info=" << service.info()
            << ")";
}

#include "moc_mdnsresolver.cpp"
