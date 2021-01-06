#ifndef MDNS_MODELSUPPORT_H
#define MDNS_MODELSUPPORT_H

#include <QMetaEnum>

namespace MDNS {
namespace ModelSupport {

QHash<int, QByteArray> roleNames(QMetaEnum roles);

template<typename Roles>
inline auto roleNames()
{
    static const auto names = roleNames(QMetaEnum::fromType<Roles>());
    return names;
}

} // namespace ModelSupport
} // namespace MDNS

#endif // MDNS_MODELSUPPORT_H
