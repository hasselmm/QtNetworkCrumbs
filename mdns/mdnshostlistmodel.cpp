/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2021 Mathias Hasselmann
 */
#include "mdnshostlistmodel.h"

// MDNS headers
#include "mdnsmodelsupport.h"
#include "mdnsresolver.h"

// Qt headers
#include <QMetaEnum>

// STL headers
#include <QDateTime>
#include <algorithm>

Q_DECLARE_METATYPE(QHostAddress)

namespace MDNS {

namespace {

template<class T>
QString valueToString(T value)
{
    return value.toString();
}

template<typename T>
QString listToString(QList<T> values)
{
    QStringList strings;
    std::transform(values.begin(), values.end(), std::back_inserter(strings), &valueToString<T>);
    return strings.join(", ");
}

QHostAddress firstAddressOfType(QList<QHostAddress> addresses, QAbstractSocket::NetworkLayerProtocol protocol)
{
    const auto sameProtocol = [protocol](QHostAddress address) {
        return address.protocol() == protocol;
    };

    const auto it = std::find_if(addresses.begin(), addresses.end(), sameProtocol);
    return it != addresses.end() ? *it : QHostAddress{};
}

template<class Row>
auto sameHostName(QString hostName)
{
    return [hostName = std::move(hostName)](const Row &row) {
        return row.hostName.compare(hostName, Qt::CaseInsensitive) == 0;
    };
};

QVariant minimumValue(const QList<qint64> values)
{
    if (const auto first = values.begin(), last = values.end(); first != last)
        return *std::min_element(first, last);

    return {};
}

QList<qint64> expiries(const QList<qint64> &ttls)
{
    const auto addCurrentTime = [baseTime = QDateTime::currentSecsSinceEpoch()](qint64 dt) {
        return baseTime + dt;
    };

    QList<qint64> expiries;
    expiries.reserve(ttls.size());
    std::transform(ttls.begin(), ttls.end(), std::back_inserter(expiries), addCurrentTime);
    return expiries;
}

} // namespace

void HostListModel::setResolver(Resolver *resolver)
{
    if (m_resolver == resolver)
        return;
    if (m_resolver)
        m_resolver->disconnect(this);

    beginResetModel();

    m_rows.clear();
    m_resolver = resolver;

    if (m_resolver) {
        const auto queries = m_resolver->hostNameQueries();
        m_rows.reserve(queries.size());

        for (const auto &hostName: queries)
            m_rows.append({hostName, {}, {}, {}});

        connect(m_resolver, &Resolver::hostNameResolved,
                this, &HostListModel::onHostNameResolved);
        connect(m_resolver, &Resolver::hostNameQueriesChanged,
                this, &HostListModel::onHostNameQueriesChanged);
    }

    emit resolverChanged(m_resolver);
    endResetModel();
}

Resolver *HostListModel::resolver() const
{
    return m_resolver.data();
}

QVariant HostListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (static_cast<Columns>(section)) {
        case HostNameColumn:
            return tr("Host Name");
        case AddressesColumn:
            return tr("Addresses");
        case IPv4AddressColumn:
            return tr("IPv4 Address");
        case IPv6AddressColumn:
            return tr("IPv6 Address");
        case TimeToLifeColumn:
            return tr("TTL");
        case ExpiryTimeColumn:
            return tr("Expiry Time");
        }
    }

    return {};
}

QVariant HostListModel::data(const QModelIndex &index, int role) const
{
    if (checkIndex(index, CheckIndexOption::DoNotUseParent)) {
        const auto &row = m_rows[index.row()];
        switch (static_cast<Columns>(index.column())) {
        case HostNameColumn:
            if (role == Qt::DisplayRole || role == Qt::UserRole)
                return row.hostName;
            else if (role >= Qt::UserRole)
                return this->index(index.row(), role - Qt::UserRole).data(Qt::UserRole);

            break;

        case AddressesColumn:
            if (role == Qt::DisplayRole)
                return listToString(row.addresses);
            else if (role == Qt::UserRole)
                return QVariant::fromValue(row.addresses);

            break;

        case IPv4AddressColumn:
            if (const auto address = firstAddressOfType(row.addresses, QAbstractSocket::IPv4Protocol); !address.isNull()) {
                if (role == Qt::DisplayRole)
                    return valueToString(address);
                else if (role == Qt::UserRole)
                    return QVariant::fromValue(address);
            }

            break;

        case IPv6AddressColumn:
            if (const auto address = firstAddressOfType(row.addresses, QAbstractSocket::IPv6Protocol); !address.isNull()) {
                if (role == Qt::DisplayRole)
                    return valueToString(address);
                else if (role == Qt::UserRole)
                    return QVariant::fromValue(address);
            }

            break;

        case TimeToLifeColumn:
            if (role == Qt::DisplayRole)
                return minimumValue(row.ttl).toString();
            else if (role == Qt::UserRole)
                return minimumValue(row.ttl);
            else if (role == Qt::TextAlignmentRole)
                return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);

            break;

        case ExpiryTimeColumn:
            if (role == Qt::DisplayRole) {
                if (const auto expiry = minimumValue(row.expiryTimes); !expiry.isNull())
                    return QDateTime::fromSecsSinceEpoch(expiry.value<qint64>()).toString();
            } else if (role == Qt::UserRole) {
                if (const auto expiry = minimumValue(row.expiryTimes); !expiry.isNull())
                    return QDateTime::fromSecsSinceEpoch(expiry.value<qint64>());
            }

            break;
        }
    }

    return {};
}

int HostListModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return QMetaEnum::fromType<Columns>().keyCount();
}

int HostListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return m_rows.count();
}

QHash<int, QByteArray> HostListModel::roleNames() const
{
    return ModelSupport::roleNames<Roles>();
}

void HostListModel::onHostNameResolved(QString hostName, QList<QHostAddress> addresses, QList<qint64> ttl)
{
    const auto expiryTimes = expiries(ttl);

    if (const auto it = std::find_if(m_rows.begin(), m_rows.end(), sameHostName<Row>(hostName));
            it != m_rows.end()) {
        const auto row = it - m_rows.begin();
        QVector<int> roles;

        if (std::exchange(it->hostName, hostName) != hostName)
            roles.append({Qt::DisplayRole, Qt::UserRole, HostNameRole});

        if (std::exchange(it->addresses, addresses) != addresses) {
            emit dataChanged(index(row, AddressesColumn), index(row, IPv6AddressColumn), {Qt::DisplayRole, Qt::UserRole});
            roles.append({AddressesRole, IPv4AddressRole, IPv6AddressRole});
        }

        if (std::exchange(it->ttl, ttl) != ttl) {
            emit dataChanged(index(row, TimeToLifeColumn), index(row, TimeToLifeColumn), {Qt::DisplayRole, Qt::UserRole});
            roles.append(TimeToLifeRole);
        }

        if (std::exchange(it->expiryTimes, expiryTimes) != expiryTimes) {
            emit dataChanged(index(row, ExpiryTimeColumn), index(row, ExpiryTimeColumn), {Qt::DisplayRole, Qt::UserRole});
            roles.append(ExpiryTimeRole);
        }

        if (!roles.isEmpty())
            emit dataChanged(index(row), index(row), roles);
    } else {
        beginInsertRows({}, m_rows.count(), m_rows.count());
        m_rows.append({hostName, addresses, expiryTimes, ttl});
        endInsertRows();
    }
}

void HostListModel::onHostNameQueriesChanged()
{
    const auto queries = m_resolver->hostNameQueries();
    for (const auto &hostName: queries) {
        if (std::find_if(m_rows.begin(), m_rows.end(), sameHostName<Row>(hostName)) == m_rows.end()) {
            beginInsertRows({}, m_rows.count(), m_rows.count());
            m_rows.append({hostName, QList<QHostAddress>{}, QList<qint64>{}, QList<qint64>{}});
            endInsertRows();
        }
    }
}

} // namespace MDNS
