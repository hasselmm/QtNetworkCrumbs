/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2021 Mathias Hasselmann
 */

// MDNS headers
#include "mdnshostlistmodel.h"
#include "mdnsmessage.h"
#include "mdnsresolver.h"
#include "mdnsservicelistmodel.h"

// Qt headers
#include <QApplication>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLoggingCategory>
#include <QMainWindow>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QTableView>

namespace {

Q_LOGGING_CATEGORY(lcDemo, "mdns.demo.models")

class LookupPage : public QWidget
{
    Q_OBJECT

public:
    explicit LookupPage(QString label, QList<int> fixedColumns, QWidget *parent = {})
        : QWidget{parent}
        , m_proxyModel{new QSortFilterProxyModel{this}}
        , m_tableView{new QTableView{this}}
        , m_fixedColumns{std::move(fixedColumns)}
    {
        m_proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);

        m_tableView->setSelectionBehavior(QTableView::SelectRows);
        m_tableView->setMinimumSize(1000, 400);
        m_tableView->setSortingEnabled(true);
        m_tableView->setModel(m_proxyModel);

        const auto lookupLabel = new QLabel{std::move(label), this};
        const auto lookupEdit = new QLineEdit{this};
        const auto loopupButton = new QPushButton{tr("&Lookup"), this};
        lookupLabel->setBuddy(lookupEdit);

        connect(lookupEdit, &QLineEdit::returnPressed,
                loopupButton, &QPushButton::click);

        connect(loopupButton, &QPushButton::clicked,
                this, [this, lookupEdit] {
            const auto text = lookupEdit->text().trimmed();
            emit lookupRequested(text);
            lookupEdit->clear();
        });

        const auto layout = new QGridLayout{this};
        layout->addWidget(m_tableView, 0, 0, 1, 3);
        layout->addWidget(lookupLabel, 1, 0, Qt::AlignVCenter);
        layout->addWidget(lookupEdit, 1, 1, Qt::AlignVCenter);
        layout->addWidget(loopupButton, 1, 2, Qt::AlignVCenter);
    }

    void setModel(QAbstractItemModel *model)
    {
        m_proxyModel->setSourceModel(model);

        if (model) {
            m_tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
            m_tableView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);

            for (auto i: m_fixedColumns)
                m_tableView->horizontalHeader()->setSectionResizeMode(i, QHeaderView::ResizeToContents);

            m_tableView->sortByColumn(MDNS::HostListModel::HostNameColumn, Qt::AscendingOrder);
        }
    }

signals:
    void lookupRequested(QString text);

private:
    QSortFilterProxyModel *const m_proxyModel;
    QTableView *const m_tableView;
    QList<int> m_fixedColumns;
};

class HostLookupPage : public LookupPage
{
public:
    explicit HostLookupPage(MDNS::Resolver *resolver, QWidget *parent = {})
        : LookupPage{tr("&Host name:"), {
                     MDNS::HostListModel::IPv4AddressColumn,
                     MDNS::HostListModel::TimeToLifeColumn,
                     MDNS::HostListModel::ExpiryTimeColumn}, parent}
    {
        const auto hostListModel = new MDNS::HostListModel{this};
        hostListModel->setResolver(resolver);
        setModel(hostListModel);

        connect(this, &HostLookupPage::lookupRequested,
                this, [resolver](const auto hostName) {
            resolver->lookupHostNames({hostName});
        });
    }
};

class ServiceLookupPage : public LookupPage
{
public:
    explicit ServiceLookupPage(MDNS::Resolver *resolver, QWidget *parent = {})
        : LookupPage{tr("&Service type:"), {
                     MDNS::ServiceListModel::PortColumn,
                     MDNS::ServiceListModel::TimeToLifeColumn,
                     MDNS::ServiceListModel::ExpiryTimeColumn}, parent}
    {
        const auto hostListModel = new MDNS::ServiceListModel{this};
        hostListModel->setResolver(resolver);
        setModel(hostListModel);

        connect(this, &HostLookupPage::lookupRequested,
                this, [resolver](const auto hostName) {
            resolver->lookupServices({hostName});
        });
    }
};

class ModelDemoWindow : public QMainWindow
{
public:
    explicit ModelDemoWindow(QWidget *parent = {})
        : QMainWindow{parent}
    {
        const auto resolver = new MDNS::Resolver{this};
        resolver->lookupServices({"_http._tcp", "_xpresstrain._tcp", "_googlecast._tcp"});
        resolver->lookupHostNames({"juicifer", "android"});

        connect(resolver, &MDNS::Resolver::messageReceived,
                this, [](const auto message) {
            qCDebug(lcDemo).verbosity(QDebug::MinimumVerbosity)
                    << "message received:" << message;
        });

        const auto tabView = new QTabWidget{this};
        tabView->addTab(new HostLookupPage{resolver, tabView}, tr("Lookup Host Names"));
        tabView->addTab(new ServiceLookupPage{resolver, tabView}, tr("Lookup Services"));
        setCentralWidget(tabView);
    }
};

} // namespace

int main(int argc, char *argv[])
{
    QApplication app{argc, argv};
    (new ModelDemoWindow)->show();
    return app.exec();
}

#include "mdnsmodeldemo.moc"
