/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2024 Mathias Hasselmann
 */
#ifndef QNC_QNCTESTSUPPORT_H
#define QNC_QNCTESTSUPPORT_H

// Qt headers
#include <QMetaType>

// STL headers
#include <cmath>

class QDebug;

Q_DECLARE_METATYPE(long double)

namespace QTest {

[[nodiscard]] char *toString(const long double &t);
[[nodiscard]] bool qCompare(long double const &t1, long double const &t2,
                            const char *actual, const char *expected,
                            const char *file, int line);

[[nodiscard]] inline bool qCompare(long double const &t1, const double &t2,
                                   const char *actual, const char *expected,
                                   const char *file, int line)
{
    return qCompare(t1, static_cast<long double>(t2), actual, expected, file, line);
}

[[nodiscard]] inline bool qCompare(long double const &t1, const float &t2,
                                   const char *actual, const char *expected,
                                   const char *file, int line)
{
    return qCompare(t1, static_cast<long double>(t2), actual, expected, file, line);
}

[[nodiscard]] inline bool qCompare(const double &t1, const long double &t2,
                                   const char *actual, const char *expected,
                                   const char *file, int line)
{
    return qCompare(static_cast<long double>(t1), t2, actual, expected, file, line);
}

[[nodiscard]] inline bool qCompare(float const &t1, const long double &t2,
                                   const char *actual, const char *expected,
                                   const char *file, int line)
{
    return qCompare(static_cast<long double>(t1), t2, actual, expected, file, line);
}

} // namespace QTest

[[nodiscard]] inline bool qFuzzyCompare(long double p1, long double p2)
{
    if constexpr (sizeof(long double) != sizeof(double)) {
        return (std::abs(p1 - p2) * 10000000000000000. <= std::min(std::abs(p1), std::abs(p2)));
    } else {
        return qFuzzyCompare(static_cast<double>(p1), static_cast<double>(p2));
    }
}

QDebug operator<<(QDebug debug, long double value);

namespace qnc::tests {
bool initialize();
} // namespace qnc::tests

#endif // QNC_QNCTESTSUPPORT_H
