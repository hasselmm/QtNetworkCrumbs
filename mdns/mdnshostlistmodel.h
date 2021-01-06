#ifndef MDNS_HOSTLISTMODEL_H
#define MDNS_HOSTLISTMODEL_H

#include <QAbstractListModel>
#include <QHostAddress>
#include <QPointer>

class QHostAddress;

namespace MDNS {

class Resolver;

class HostListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(MDNS::Resolver *resolver READ resolver WRITE setResolver NOTIFY resolverChanged FINAL)

public:
    enum Columns {
        HostNameColumn,
        AddressesColumn,
        IPv4AddressColumn,
        IPv6AddressColumn,
        TimeToLifeColumn,
        ExpiryTimeColumn,
    };

    Q_ENUM(Columns)

    enum Roles {
        DisplayRole = Qt::DisplayRole,
        HostNameRole = Qt::UserRole,
        AddressesRole,
        IPv4AddressRole,
        IPv6AddressRole,
        TimeToLifeRole,
        ExpiryTimeRole,
    };

    Q_ENUM(Roles)

    using QAbstractListModel::QAbstractListModel;

    Resolver *resolver() const;

public slots:
    void setResolver(Resolver *resolver);

signals:
    void resolverChanged(MDNS::Resolver *resolver);

public: // QAbstractItemModel interface
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    int rowCount(const QModelIndex &parent = {}) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    void onHostNameResolved(QString hostName, QList<QHostAddress> addresses, QList<qint64> ttls);
    void onHostNameQueriesChanged();

    struct Row {
        QString hostName;
        QList<QHostAddress> addresses;
        QList<qint64> expiryTimes;
        QList<qint64> ttl;
    };

    QList<Row> m_rows;
    QPointer<Resolver> m_resolver;
};

} // namespace MDNS

#endif // MDNS_HOSTLISTMODEL_H
