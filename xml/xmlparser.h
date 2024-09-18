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
#include <optional>
#include <variant>

class QVersionNumber;

namespace qnc::xml {

// FIXME: move to qncparse.h?
enum class VersionSegment { Major = 0, Minor = 1 };
void updateVersion(QVersionNumber &version, VersionSegment segment, int number);

template <class Object, class Context>
Object &currentObject(Context &context) { return context; }

namespace detail {

template <typename T>
using RequireEnum = std::enable_if_t<std::is_enum_v<T>, bool>;

template <auto field>
using RequireField = std::enable_if_t<std::is_member_pointer_v<decltype(field)>, bool>;

template <auto member>
using RequireMemberFunction = std::enable_if_t<std::is_member_function_pointer_v<decltype(member)>, bool>;

} // namespace detail

class ParserBase : public QObject
{
    Q_OBJECT

public:
    explicit ParserBase(QXmlStreamReader *reader, QObject *parent = nullptr)
        : QObject{parent}
        , m_xml{reader}
    {}

protected:
    template <typename T>
    void read(const std::function<void(T)> &store);

    template <typename T, class Object, class Context>
    void readField(Context &context, T (Object::*field))
    {
        read<T>([&context, field](T value) {
            (currentObject<Object>(context).*field) = std::move(value);
        });
    }

    template <typename T, typename Field, class Object, class Context>
    void readField(Context &context, Field (Object::*field), void (Field::*setter)(T))
    {
        read<T>([&context, field, setter](T value) {
            auto &fieldInstance = currentObject<Object>(context).*field;
            (fieldInstance.*setter)(std::move(value));
        });
    }

    template <typename T, class Object, class Context>
    static void extendList(Context &context, QList<T> (Object::*list))
    {
        (currentObject<Object>(context).*list).append(T{});
    }

    static QString stateName(const QMetaEnum &metaEnum, int value);

protected:
    class AbstractContext
    {
    public:
        AbstractContext(int initialState) { enterState(initialState); }

        bool isEmpty() const { return m_stack.isEmpty(); }
        void enterState(int state) { m_stack.push(state); }
        int currentState() const { return m_stack.top(); }
        QString currentStateName() const;
        void leaveState() { m_stack.pop(); }

        virtual bool selectNamespace(QStringView namespaceUri) = 0;
        virtual std::optional<int> processElement(QStringView elementName) = 0;
        virtual QString stateName(int state) const = 0;

    private:
        QStack<int> m_stack = {};
    };

    bool parse(const QLoggingCategory &category, AbstractContext &context);

private:
    QXmlStreamReader *const m_xml;
};

template <typename StateEnum>
class Parser : public ParserBase
{
public:
    static_assert(std::is_enum_v<StateEnum>);

    using State          = StateEnum;
    using Processing     = std::function<void()>;
    using Transition     = std::function<State()>;
    using ParseStep      = std::variant<std::monostate, Transition, Processing>;
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
            extendList(context, list);
            return nextState;
        };
    }

    template <typename T>
    Processing run(const std::function<void(T)> &callback)
    {
        return [this, callback] {
            read<T>(callback);
        };
    }

    template <auto field, class Context,
              detail::RequireField<field> = true>
    Processing setField(Context &context)
    {
        return [this, &context] {
            readField(context, field);
        };
    }

    template <auto field, auto setter, class Context,
              detail::RequireMemberFunction<setter> = true,
              detail::RequireField<field> = true>
    Processing setField(Context &context)
    {
        return [this, &context] {
            readField(context, field, setter);
        };
    }

    template <auto field, VersionSegment segment, class Context,
              detail::RequireField<field> = true>
    Processing setField(Context &context)
    {
        return run<int>([&context](int number) {
            updateVersion(context.*field, segment, number);
        });
    };

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

        std::optional<int> processElement(QStringView elementName) override
        {
            const auto state        = static_cast<State>(AbstractContext::currentState());
            const auto &parseSteps  = m_currentNamespace->value(state);
            const auto &currentStep = parseSteps.value(elementName);

            if (const auto transition = std::get_if<Transition>(&currentStep)) {
                return static_cast<int>(std::invoke(*transition));
            } else if (const auto processing = std::get_if<Processing>(&currentStep)) {
                std::invoke(*processing);
                return currentState();
            } else {
                return {};
            }
        }

        QString stateName(int state) const override
        {
            return Parser::stateName(static_cast<State>(state));
        }

    private:
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
