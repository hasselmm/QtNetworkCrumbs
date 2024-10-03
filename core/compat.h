/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2024 Mathias Hasselmann
 */
#ifndef QNCCORE_COMPAT_H
#define QNCCORE_COMPAT_H

#include <QtGlobal>

#if QT_VERSION_MAJOR < 6

#define Q_IMPLICIT /* implicit */

namespace qnc::core {

template <typename T>
constexpr std::underlying_type_t<T> qToUnderlying(T value) noexcept
{
    return static_cast<std::underlying_type_t<T>>(value);
}

} // namespace qnc::core

using qnc::core::qToUnderlying;

#endif

#endif // QNCCORE_COMPAT_H
