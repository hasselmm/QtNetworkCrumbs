/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2024 Mathias Hasselmann
 */
#ifndef QNC_HTTPPARSER_H
#define QNC_HTTPPARSER_H

// QtNetworkCrumbs headers
#include "qncliterals.h"

// Qt headers
#include <QByteArray>
#include <QList>

// STL headers
#include <optional>

class QDateTime;
class QIODevice;

namespace qnc::http {

namespace detail {

template <class T> struct view_trait;
template <>        struct view_trait<QByteArray>            { using type = compat::ByteArrayView; };
template <>        struct view_trait<compat::ByteArrayView> { using type = compat::ByteArrayView; };
template <>        struct view_trait<QLatin1String>         { using type = QStringView; };
template <>        struct view_trait<QString>               { using type = QStringView; };
template <>        struct view_trait<QStringView>           { using type = QStringView; };

template <class T, class U = T, typename = void>
struct hasStartsWith : std::false_type {};
template <class T, class U>
struct hasStartsWith<T, U, std::void_t<decltype(T{}.startsWith(U{}, Qt::CaseInsensitive))>> : std::true_type {};

template <class T, class U = T, typename = void>
struct hasEndsWith : std::false_type {};
template <class T, class U>
struct hasEndsWith<T, U, std::void_t<decltype(T{}.endsWith(U{}, Qt::CaseInsensitive))>> : std::true_type {};

static_assert(hasStartsWith<QByteArray>() == false);
static_assert(hasEndsWith<QByteArray>() == false);

static_assert(hasStartsWith<QString>() == true);
static_assert(hasEndsWith<QString>() == true);

} // namespace detail

template <class T>
class CaseInsensitive : public T
{
public:
    using T::T;

    using view_type = typename detail::view_trait<T>::type;

    CaseInsensitive(const T &init) : T{init} {}
    CaseInsensitive(T &&init) : T{std::move(init)} {}

    template <class U>
    friend bool operator==(const CaseInsensitive &l, const U &r)
    { return l.compare(r, Qt::CaseInsensitive) == 0; }

    template <class U>
    friend bool operator==(const U &l, const CaseInsensitive &r)
    { return l.compare(r, Qt::CaseInsensitive) == 0; }

    friend bool operator==(const CaseInsensitive &l, const CaseInsensitive &r)
    { return l.compare(r, Qt::CaseInsensitive) == 0; }

    template <class U>
    bool startsWith(const U &r) const
    {
        if constexpr (detail::hasStartsWith<T, U>()) {
            return T::startsWith(r, Qt::CaseInsensitive);
        } else {
            if (T::size() < r.size())
                return false;

            const auto view = view_type{T::cbegin(), static_cast<compat::lentype>(r.size())};
            return view.compare(r, Qt::CaseInsensitive) == 0;
        }
    }

    template <class U>
    bool endsWith(const U &r) const
    {
        if constexpr (detail::hasEndsWith<T, U>()) {
            return T::endsWith(r, Qt::CaseInsensitive);
        } else {
            if (T::size() < r.size())
                return false;

            return view_type{T::cend() - r.size(), r.size()}.compare(r, Qt::CaseInsensitive) == 0;
        }
    }
};

template <class T> CaseInsensitive(const T &) -> CaseInsensitive<T>;
template <class T> CaseInsensitive(T &&) -> CaseInsensitive<T>;

static_assert(std::is_same_v<compat::ByteArrayView, CaseInsensitive<QByteArray>::view_type>);
static_assert(std::is_same_v<compat::ByteArrayView, CaseInsensitive<compat::ByteArrayView>::view_type>);
static_assert(std::is_same_v<QStringView, CaseInsensitive<QLatin1String>::view_type>);
static_assert(std::is_same_v<QStringView, CaseInsensitive<QString>::view_type>);
static_assert(std::is_same_v<QStringView, CaseInsensitive<QStringView>::view_type>);

class Message
{
    Q_GADGET

public:
    using HeaderList = QList<std::pair<CaseInsensitive<QByteArray>, QByteArray>>;

    enum class Type {
        Invalid,
        Request,
        Response,
    };

    Q_ENUM(Type)

    constexpr Message() noexcept = default;

    [[nodiscard]] constexpr Type      type() const { return m_type; }
    [[nodiscard]] constexpr bool isInvalid() const { return type() == Type::Invalid; }
    [[nodiscard]] HeaderList       headers() const { return m_headers; }


    [[nodiscard]] QByteArray            protocol() const;
    [[nodiscard]] QByteArray                verb() const;
    [[nodiscard]] QByteArray            resource() const;
    [[nodiscard]] std::optional<uint> statusCode() const;
    [[nodiscard]] QByteArray        statusPhrase() const;

    [[nodiscard]] static Message parse(const QByteArray &data);
    [[nodiscard]] static Message parse(QIODevice *device);

private:
    [[nodiscard]] static Message parseStatusLine(const QByteArray &line);
    [[nodiscard]] QByteArray statusField(Type expectedType, int index) const;

    Type           m_type    = Type::Invalid;
    QByteArrayList m_status  = {};
    HeaderList     m_headers = {};
};

QDateTime parseDateTime(const QByteArray &text);
QDateTime parseDateTime(const QString &text);

QDateTime expiryDateTime(const QByteArray &cacheControl, const QByteArray &expires, const QDateTime &now);
QDateTime expiryDateTime(const QByteArray &cacheControl, const QByteArray &expires);

} // namespace qnc::http

#endif // QNC_HTTPPARSER_H
