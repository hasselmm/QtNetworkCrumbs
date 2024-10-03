/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2024 Mathias Hasselmann
 */
#ifndef QNCCORE_ABSTRACTRESOLVER_H
#define QNCCORE_ABSTRACTRESOLVER_H

#include <QAbstractSocket>
#include <QHostAddress>

class QNetworkInterface;
class QTimer;

namespace qnc::core {

class AbstractResolver : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int scanInterval READ scanInterval WRITE setScanInterval NOTIFY scanIntervalChanged FINAL)

public:
    AbstractResolver(QObject *parent = nullptr);

    [[nodiscard]] int scanInterval() const;
    [[nodiscard]] std::chrono::milliseconds scanIntervalAsDuration() const;
    void setScanInterval(std::chrono::milliseconds ms);

public slots:
    void setScanInterval(int ms);

signals:
    void scanIntervalChanged(int interval);

protected:
    using SocketPointer = std::shared_ptr<QAbstractSocket>;
    using SocketTable   = QHash<QHostAddress, SocketPointer>;

    [[nodiscard]] virtual bool isSupportedInterface(const QNetworkInterface &iface) const = 0;
    [[nodiscard]] virtual bool isSupportedAddress(const QHostAddress &address) const = 0;

    [[nodiscard]] virtual SocketPointer createSocket(const QNetworkInterface &iface,
                                                     const QHostAddress &address) = 0;

    [[nodiscard]] SocketPointer socketForAddress(const QHostAddress &address) const;

    virtual void submitQueries(const SocketTable &sockets) = 0;

private:
    void onTimeout();
    void scanNetworkInterfaces();

    QTimer *const   m_timer;
    SocketTable     m_sockets;
};

} // namespace qnc::core

#endif // QNCCORE_ABSTRACTRESOLVER_H
