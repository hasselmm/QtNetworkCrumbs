#ifndef MDNS_SERVICELISTMODEL_H
#define MDNS_SERVICELISTMODEL_H

#include <QAbstractListModel>
#include <QPointer>

namespace MDNS {

class Resolver;
class ServiceDescription;

class ServiceListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(MDNS::Resolver *resolver READ resolver WRITE setResolver NOTIFY resolverChanged FINAL)

public:
    enum Columns {
        ServiceNameColumn,
        ServiceTypeColumn,
        HostNameColumn,
        PortColumn,
        UrlColumn,
        TimeToLifeColumn,
        ExpiryTimeColumn,
    };

    Q_ENUM(Columns)

    enum Roles {
        DisplayRole = Qt::DisplayRole,
        ServiceNameRole = Qt::UserRole,
        ServiceTypeRole,
        HostNameRole,
        PortRole,
        UrlRole,
        TimeToLifeRole,
        ExpiryTimeRole,
        DataRole,
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
    void onServiceResolved(ServiceDescription service);
    void onServiceQueriesChanged();

    struct Row {
        QString serviceName;
        QString serviceType;
        QString hostName;
        int port;
        qint64 ttl;
        QByteArray data;
    };

    QList<Row> m_rows;
    QPointer<Resolver> m_resolver;
};

} // namespace MDNS

#endif // MDNS_SERVICELISTMODEL_H
