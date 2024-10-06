/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2024 Mathias Hasselmann
 */

#ifndef QNCXML_XMLPARSER_H
#define QNCXML_XMLPARSER_H

// Qt headers
#include <QLoggingCategory>
#include <QMetaEnum>
#include <QObject>
#include <QStack>
#include <QXmlStreamReader>

// STL headers
#include <array>
#include <optional>
#include <variant>

class QVersionNumber;

namespace qnc::xml {

// FIXME: move to qncparse.h?
enum class VersionSegment { Major = 0, Minor = 1 };
void updateVersion(QVersionNumber &version, VersionSegment segment, int number);

template <class Object, class Context>
Object &currentObject(Context &context) { return context; }

template <typename T>
constexpr std::size_t keyCount = 0;

template <typename T, std::size_t N = keyCount<T>>
using KeyValueMap = std::array<std::pair<T, QStringView>, N>;

template <typename T>
constexpr KeyValueMap<T> keyValueMap() { return {}; }

template<typename T>
using OpportunisticEnum = std::variant<std::monostate, T, QString>;

namespace detail {

struct MemberTraits
{
    template <typename T, class Context>
    static constexpr T fieldType(T (Context::*field));

    template <typename T, class Context>
    static constexpr T argumentType(void (Context::*member)(T));

    template <typename T, class Context>
    static constexpr Context objectType(T (Context::*field));
};

template <typename T>
using RequireEnum = std::enable_if_t<std::is_enum_v<T>, bool>;

template <auto field>
using RequireField = std::enable_if_t<std::is_member_pointer_v<decltype(field)>, bool>;

template <auto member>
using RequireMemberFunction = std::enable_if_t<std::is_member_function_pointer_v<decltype(member)>, bool>;

template <auto field, RequireField<field> = true>
using ValueType = decltype(MemberTraits::fieldType(field));

template <auto field, RequireField<field> = true>
using FlagType = typename ValueType<field>::enum_type;

template <auto field, RequireField<field> = true>
using ObjectType = decltype(MemberTraits::objectType(field));

template <auto member, RequireMemberFunction<member> = true>
using ArgumentType = decltype(MemberTraits::argumentType(member));

template<typename T> struct is_opportunistic_enum : std::false_type {};

template<typename T>
struct is_opportunistic_enum<OpportunisticEnum<T>> : std::true_type {};

template<typename T>
constexpr bool is_opportunistic_enum_v = is_opportunistic_enum<T>::value;

template<typename T>
constexpr const char *qt_getEnumName(T) { return nullptr; }

template<typename T>
std::optional<T> keyToValue(QStringView key)
{
    if constexpr (!keyValueMap<T>().empty()) {
        static constexpr auto map = keyValueMap<T>();

        static_assert(!std::get<QStringView>(map.front()).isEmpty(),
                      "Unsupported enum type: keyValueMap() has invalid keys");

        const auto it = std::find_if(map.cbegin(), map.cend(), [key](const auto &pair) {
            return std::get<QStringView>(pair) == key;
        });

        if (Q_LIKELY(it != map.cend()))
            return std::get<T>(*it);
    } else if constexpr (qt_getEnumName(T{}) != nullptr) {
        static const auto &metaEnum = QMetaEnum::fromType<T>();

        auto isValid = false;
        const auto value = metaEnum.keyToValue(key.toLatin1().constData(), &isValid);

        if (Q_LIKELY(isValid))
            return static_cast<T>(value);
    } else {
        static_assert(static_cast<int>(T{}) && false,
                      "Unsupported enum type: Neither keyValueMap() nor Q_ENUM() found");
    }

    return {};
}

template<typename T>
std::optional<int> keyToInt(QStringView key)
{
    if (const auto &value = keyToValue<T>(std::move(key)); Q_LIKELY(value))
        return static_cast<int>(*value);

    return {};
}

} // namespace detail

class ParserBase : public QObject
{
    Q_OBJECT

public:
    using ElementParser = std::function<void()>;
    using GenericParser = std::function<void(const QXmlStreamAttribute *)>;

    explicit ParserBase(QXmlStreamReader *reader, QObject *parent = nullptr)
        : QObject{parent}
        , m_xml{reader}
    {}

    template <typename T>
    GenericParser invoke(const std::function<void(T)> &callback)
    {
        return [this, callback](const QXmlStreamAttribute *attribute) {
            read<T>(attribute, callback);
        };
    }

    template <auto field, class Context,
              detail::RequireField<field> = true>
    GenericParser assign(Context &context)
    {
        return [this, &context](const QXmlStreamAttribute *attribute) {
            using Value  = detail::ValueType <field>;
            using Object = detail::ObjectType<field>;

            read<Value>(attribute, [&context](Value value) {
                (currentObject<Object>(context).*field) = std::move(value);
            });
        };
    }

    template <auto list, class Context,
              detail::RequireField<list> = true>
    GenericParser append(Context &context)
    {
        return [this, &context](const QXmlStreamAttribute *attribute) {
            using Value = typename detail::ValueType<list>::value_type;

            read<Value>(attribute, [&context](Value value) {
                emplaceBack<list>(context, std::move(value));
            });
        };
    }

    template <auto field, auto setter, class Context,
              detail::RequireMemberFunction<setter> = true,
              detail::RequireField<field> = true>
    GenericParser assign(Context &context)
    {
        return [this, &context](const QXmlStreamAttribute *attribute) {
            using Value  = detail::ArgumentType<setter>;
            using Object = detail::ObjectType  <field>;

            read<Value>(attribute, [&context](Value value) {
                (currentObject<Object>(context).*field.*setter)(std::move(value));
            });
        };
    }

    template <auto field, detail::FlagType<field> flag, class Context,
              detail::RequireField<field> = true>
    GenericParser assign(Context &context)
    {
        return [this, &context](const QXmlStreamAttribute *attribute) {
            read<QString>(attribute, [this, &context](QStringView text) {
                parseFlag(text, [&context](bool enabled) {
                    using Object = detail::ObjectType<field>;
                    auto &target = currentObject<Object>(context).*field;
                    target.setFlag(flag, enabled);
                });
            });
        };
    }

    template <auto field, VersionSegment segment, class Context,
              detail::RequireField<field> = true>
    GenericParser assign(Context &context)
    {
        return invoke<int>([&context](int number) {
            updateVersion(context.*field, segment, number);
        });
    };

protected:
    template <auto list, class Context,
              detail::RequireField<list> = true>
    static void emplaceBack(Context &context, typename detail::ValueType<list>::value_type &&value)
    {
        using Object = typename detail::ObjectType<list>;

        QT_WARNING_PUSH
        QT_WARNING_DISABLE_GCC("-Wmaybe-uninitialized")

#if QT_VERSION_MAJOR >= 6
        (currentObject<Object>(context).*list).emplaceBack(std::move(value));
#else // QT_VERSION_MAJOR < 6
        (currentObject<Object>(context).*list).append(std::move(value));
#endif // QT_VERSION_MAJOR < 6

        QT_WARNING_POP
    }

    static QString stateName(const QMetaEnum &metaEnum, int value);

protected:
    class AbstractContext
    {
    public:
        using GenericStep = std::variant<std::monostate, int, ElementParser, GenericParser>;

        AbstractContext(int initialState) { enterState(initialState); }

        bool isEmpty() const { return m_stack.isEmpty(); }
        void enterState(int state) { m_stack.push(state); }
        int currentState() const { return m_stack.top(); }
        QString currentStateName() const;
        void leaveState() { m_stack.pop(); }

        virtual bool selectNamespace(QStringView namespaceUri) = 0;
        virtual GenericStep findStep(QStringView elementName) const = 0;
        virtual QString stateName(int state) const = 0;

        std::optional<int> parseElement(QStringView elementName) const;
        bool parseAttribute(QStringView elementName, const QXmlStreamAttribute &attribute) const;

    private:
        QStack<int> m_stack = {};
    };

    bool parse(const QLoggingCategory &category, AbstractContext &context);

private:
    void parseStartElement(const QLoggingCategory &category, AbstractContext &context);
    void parseEndElement(const QLoggingCategory &category, AbstractContext &context);

    using KeyToIntFunction = std::optional<int> (*)(QStringView);

    void parseEnum(QStringView text, KeyToIntFunction keyToInt, const std::function<void(int)> &store);
    void parseEnum(QStringView text, KeyToIntFunction keyToInt, const std::function<void(int, QStringView)> &store);

    template <typename T>
    void parseEnum(QStringView text, const std::function<void(T)> &store)
    {
        parseEnum(text, &detail::keyToInt<T>, [store](int value) {
            store(static_cast<T>(value));
        });
    }

    template <typename T>
    void parseEnum(QStringView text, const std::function<void(OpportunisticEnum<T>)> &store)
    {
        parseEnum(text, &detail::keyToInt<T>, [store](int value, QStringView text) {
            if (Q_LIKELY(text.isEmpty()))
                store(static_cast<T>(value));
            else
                store(text.toString());
        });
    }

    void parseFlag(QStringView text, const std::function<void(bool)> &store);

    template <typename T>
    void parseValue(QStringView text, const std::function<void(T)> &store);

    template <typename T>
    void read(const QXmlStreamAttribute *attribute, const std::function<void(T)> &store)
    {
        const auto &text = readValue(attribute);

        if constexpr (std::is_enum_v<T> || detail::is_opportunistic_enum_v<T>) {
            parseEnum(text, store);
        } else {
            parseValue(text, store);
        }
    }

    QString readValue(const QXmlStreamAttribute *attribute);

    QXmlStreamReader *const m_xml;
};

template <typename StateEnum>
class Parser : public ParserBase
{
public:
    static_assert(std::is_enum_v<StateEnum>);

    using State          = StateEnum;
    using Transition     = std::function<State()>;
    using ParseStep      = std::variant<std::monostate, Transition, ElementParser, GenericParser>;
    using ElementTable   = QHash<QStringView, ParseStep>;
    using StateTable     = QHash<State, ElementTable>;
    using NamespaceTable = QHash<QStringView, StateTable>;

    using ParserBase::ParserBase;

    template <State NextState>
    static Transition transition()
    {
        return [] {
            return NextState;
        };
    }

    template <State nextState, auto list, class Context>
    static Transition transition(Context &context)
    {
        return [&context] {
            emplaceBack<list>(context, {});
            return nextState;
        };
    }

    [[nodiscard]] bool parse(const QLoggingCategory &category, State initialState, const StateTable &parsers)
    {
        return parse(category, initialState, {{QStringView{}, parsers}});
    }

    [[nodiscard]] bool parse(const QLoggingCategory &category, State initialState, const NamespaceTable &parsers)
    {
        auto context = Context{initialState, parsers};
        return ParserBase::parse(category, context);
    }

private:
    [[nodiscard]] static QString stateName(State state)
    {
        return ParserBase::stateName(QMetaEnum::fromType<State>(), static_cast<int>(state));
    }

private:
    class Context : public AbstractContext
    {
    public:
        Context(State initialState, const NamespaceTable &parsers)
            : AbstractContext{static_cast<int>(initialState)}
            , m_parsers{parsers}
            , m_currentNamespace{m_parsers.cend()}
        {}

        bool selectNamespace(QStringView namespaceUri) override
        {
            m_currentNamespace = m_parsers.constFind(namespaceUri);
            return m_currentNamespace != m_parsers.cend();
        }

        GenericStep findStep(QStringView elementName) const override
        {
            if (Q_UNLIKELY(m_currentNamespace == m_parsers.cend()))
                return {};

            const auto state        = static_cast<State>(AbstractContext::currentState());
            const auto &parseSteps  = m_currentNamespace->value(state);
            const auto &currentStep = parseSteps.value(std::move(elementName));

            return std::visit([](auto &&value) {
                return makeStep(std::forward<decltype(value)>(value));
            }, currentStep);
        }

        QString stateName(int state) const override
        {
            return Parser::stateName(static_cast<State>(state));
        }

    private:
        template <typename T>
        static GenericStep makeStep(T &&value)
        {
            return {std::forward<T>(value)};
        }

        static GenericStep makeStep(const Transition &transition)
        {
            return static_cast<int>(transition());
        }

        using NamespaceIterator = typename NamespaceTable::ConstIterator;

        NamespaceTable    m_parsers;
        NamespaceIterator m_currentNamespace;
    };
};

#if QT_VERSION_MAJOR < 6

template<typename Enum, detail::RequireEnum<Enum> = true>
constexpr uint qHash(Enum value) noexcept
{
    using underlying_type = std::underlying_type_t<Enum>;
    return ::qHash(static_cast<underlying_type>(value));
}

#endif // QT_VERSION_MAJOR < 6

} // namespace qnc::xml

#endif // QNCXML_XMLPARSER_H
