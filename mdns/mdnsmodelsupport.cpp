#include "mdnsmodelsupport.h"

namespace MDNS {
namespace ModelSupport {

QHash<int, QByteArray> roleNames(QMetaEnum roles)
{
    QHash<int, QByteArray> roleNames;

    for (int i = 0; i < roles.keyCount(); ++i) {
        const auto key = roles.key(i);
        const auto nameLength = static_cast<int>(strlen(key)) - 4;
        Q_ASSERT(strcmp(key + nameLength, "Role") == 0);

        auto name = QByteArray{key, nameLength};
        name[0] = static_cast<char>(std::tolower(name[0]));
        roleNames.insert(roles.value(i), std::move(name));
    }

    return roleNames;
}

} // namespace ModelSupport
} // namespace MDNS
