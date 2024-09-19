/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2024 Mathias Hasselmann
 */
#include "qnctestsupport.h"

// Qt headers
#include <QTest>

// STL headers
#include <iomanip>
#include <sstream>

namespace qnc::tests {
namespace {

[[nodiscard]] std::string toStdString(long double value)
{
    const auto p = std::numeric_limits<long double>::digits;
    return (std::ostringstream{} << std::setprecision(p) << value).str();
}

[[nodiscard]] QString toString(long double value)
{
    return QString::fromStdString(toStdString(value));
}

} // namespace

bool initialize()
{
    return QMetaType::registerConverter<long double, QString>(&toString);
}

} // namespace qnc::tests

[[nodiscard]] char *QTest::toString(const long double &t)
{
    return qstrdup(qnc::tests::toStdString(t).c_str());
}

[[nodiscard]] bool QTest::qCompare(long double const &t1, long double const &t2,
                                   const char *actual, const char *expected,
                                   const char *file, int line)
{
    auto success = false;

    if (std::isnan(t1)) {
        success = std::isnan(t2);
    } else if (std::isinf(t1) && t1 > 0) {
        success = std::isinf(t2) && t2 > 0;
    } else if (std::isinf(t1) && t1 < 0) {
        success = std::isinf(t2) && t2 < 0;
    } else {
        success = qFuzzyCompare(t1, t2);
    }

#if QT_VERSION_MAJOR < 6

    return compare_helper(success, "Compared numbers are not the same",
                          toString(t1), toString(t2), actual, expected, file, line);

#else // QT_VERSION_MAJOR >= 6

    return reportResult(success,
                        [t1] { return toString(t1); },
                        [t2] { return toString(t2); },
                        actual, expected, ComparisonOperation::Equal,
                        file, line);

#endif // QT_VERSION_MAJOR >= 6
}

QDebug operator<<(QDebug debug, long double value)
{
    const auto _ = QDebugStateSaver{debug};
    return debug.noquote() << qnc::tests::toString(value);
}
