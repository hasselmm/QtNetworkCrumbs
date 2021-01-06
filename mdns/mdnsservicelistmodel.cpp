/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2021 Mathias Hasselmann
 */
#include "mdnsservicelistmodel.h"

// MDNS headers
#include "mdnsmodelsupport.h"
#include "mdnsresolver.h"

// Qt headers
#include <QMetaEnum>

namespace MDNS {

namespace {

template<class Row>
auto sameService(QString serviceName, QString serviceType)
{
    return [serviceName = std::move(serviceName), serviceType = std::move(serviceType)](const Row &row) {
        return row.serviceName.compare(serviceName, Qt::CaseInsensitive) == 0
                && row.serviceType.compare(serviceType, Qt::CaseInsensitive) == 0;
    };
};

} // namespace

void ServiceListModel::setResolver(Resolver *resolver)
{
    if (m_resolver == resolver)
        return;
    if (m_resolver)
        m_resolver->disconnect(this);

    beginResetModel();

    m_resolver = resolver;
    m_rows.clear();

    if (m_resolver) {
        connect(m_resolver, &Resolver::serviceResolved,
                this, &ServiceListModel::onServiceResolved);
        connect(m_resolver, &Resolver::serviceQueriesChanged,
                this, &ServiceListModel::onServiceQueriesChanged);
    }

    emit resolverChanged(m_resolver);
    endResetModel();
}

Resolver *ServiceListModel::resolver() const
{
    return m_resolver.data();
}

QVariant ServiceListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (static_cast<Columns>(section)) {
        case ServiceNameColumn:
            return tr("Service Name");
        case ServiceTypeColumn:
            return tr("Service Type");
        case HostNameColumn:
            return tr("Host Name");
        case PortColumn:
            return tr("Port");
        case UrlColumn:
            return tr("URL");
        case TimeToLifeColumn:
            return tr("TTL");
        case ExpiryTimeColumn:
            return tr("Expiry Time");
        }
    }

    return {};
}

QVariant ServiceListModel::data(const QModelIndex &index, int role) const
{
    if (checkIndex(index, CheckIndexOption::DoNotUseParent)) {
        const auto &row = m_rows[index.row()];
        switch (static_cast<Columns>(index.column())) {
        case ServiceNameColumn:
            if (role == Qt::DisplayRole || role == Qt::UserRole)
                return row.serviceName;
            else if (role == DataRole)
                return row.data;
            else if (role >= Qt::UserRole)
                return this->index(index.row(), role - Qt::UserRole).data(Qt::UserRole);

            break;

        case ServiceTypeColumn:
            if (role == Qt::DisplayRole || role == Qt::UserRole)
                return row.serviceType;

            break;

        case HostNameColumn:
            if (role == Qt::DisplayRole || role == Qt::UserRole)
                return row.hostName;

            break;

        case PortColumn:
            if (role == Qt::DisplayRole)
                return QString::number(row.port);
            else if (role == Qt::UserRole)
                return row.port;
            else if (role == Qt::TextAlignmentRole)
                return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);

            break;

        case UrlColumn:
            break;

        case TimeToLifeColumn:
            if (role == Qt::DisplayRole)
                return QString::number(row.ttl);
            else if (role == Qt::UserRole)
                return row.ttl;
            else if (role == Qt::TextAlignmentRole)
                return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);

            break;

        case ExpiryTimeColumn:
            break;
        }
    }

    return {};
}

int ServiceListModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return QMetaEnum::fromType<Columns>().keyCount();
}

int ServiceListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return m_rows.count();
}

QHash<int, QByteArray> ServiceListModel::roleNames() const
{
    return ModelSupport::roleNames<Roles>();
}

void ServiceListModel::onServiceResolved(ServiceDescription service)
{
    if (const auto it = std::find_if(m_rows.begin(), m_rows.end(),
                                     sameService<Row>(service.name(), service.type()));
            it != m_rows.end()) {
//        const auto row = it - m_rows.begin();

        /*
        if (std::exchange(it->hostName, hostName) != hostName) {
            emit dataChanged(index(row, HostNameColumn), index(row, HostNameColumn),
                             {Qt::DisplayRole, Qt::UserRole, HostNameRole});
        }

        if (std::exchange(it->addresses, addresses) != addresses) {
            emit dataChanged(index(row, 0), index(row, 0),
                             {AddressesRole, IPv4AddressRole, IPv6AddressRole});
            emit dataChanged(index(row, AddressesColumn), index(row, IPv6AddressColumn),
                             {Qt::DisplayRole, Qt::UserRole});
        }
         */
    } else {
        beginInsertRows({}, m_rows.count(), m_rows.count());
        m_rows.append({service.name(), service.type(), service.target(), service.port(), service.ttl(), service.info()});
        endInsertRows();
    }
}

void ServiceListModel::onServiceQueriesChanged()
{
    const auto queries = m_resolver->serviceQueries();
    for (const auto &serviceType: queries) {
        if (std::find_if(m_rows.begin(), m_rows.end(), sameService<Row>("*", serviceType)) == m_rows.end()) {
            beginInsertRows({}, m_rows.count(), m_rows.count());
            m_rows.append({QString{}, serviceType, QString{}, 0, 5, QByteArray{}});
            endInsertRows();
        }
    }
}

} // namespace MDNS
