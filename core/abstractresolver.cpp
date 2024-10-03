/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2024 Mathias Hasselmann
 */
#include "abstractresolver.h"

// Qt headers
#include <QLoggingCategory>
#include <QNetworkInterface>
#include <QTimer>
#include <QUdpSocket>

namespace qnc::core {

namespace {

using namespace std::chrono_literals;

Q_LOGGING_CATEGORY(lcResolver,  "qnc.core.resolver")

} // namespace

AbstractResolver::AbstractResolver(QObject *parent)
    : QObject{parent}
    , m_timer{new QTimer{this}}
{
    m_timer->callOnTimeout(this, &AbstractResolver::onTimeout);
    QTimer::singleShot(0, this, &AbstractResolver::onTimeout);
    m_timer->start(15s);
}

void AbstractResolver::setScanInterval(std::chrono::milliseconds ms)
{
    if (scanIntervalAsDuration() != ms) {
        m_timer->setInterval(ms);
        emit scanIntervalChanged(scanInterval());
    }
}

void AbstractResolver::setScanInterval(int ms)
{
    if (scanInterval() != ms) {
        m_timer->setInterval(ms);
        emit scanIntervalChanged(scanInterval());
    }
}

std::chrono::milliseconds AbstractResolver::scanIntervalAsDuration() const
{
    return m_timer->intervalAsDuration();
}

int AbstractResolver::scanInterval() const
{
    return m_timer->interval();
}

AbstractResolver::SocketPointer
AbstractResolver::socketForAddress(const QHostAddress &address) const
{
    return m_sockets[address];
}

void AbstractResolver::scanNetworkInterfaces()
{
    auto newSockets = SocketTable{};

    const auto &allInterfaces = QNetworkInterface::allInterfaces();

    for (const auto &iface : allInterfaces) {
        if (!isSupportedInterface(iface))
            continue;

        const auto addressEntries = iface.addressEntries();
        for (const auto &entry : addressEntries) {
            if (!isSupportedAddress(entry.ip()))
                continue;

            if (const auto socket = socketForAddress(entry.ip())) {
                newSockets.insert(entry.ip(), socket);
                continue;
            }

            qCInfo(lcResolver, "Creating socket for %ls on %ls",
                   qUtf16Printable(entry.ip().toString()),
                   qUtf16Printable(iface.humanReadableName()));

            if (const auto socket = createSocket(iface, entry.ip()))
                newSockets.insert(entry.ip(), socket);
        }
    }

    std::exchange(m_sockets, newSockets);
}

void AbstractResolver::onTimeout()
{
    scanNetworkInterfaces();
    submitQueries(m_sockets);
}

} // namespace qnc
