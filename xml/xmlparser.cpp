/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2024 Mathias Hasselmann
 */
#include "xmlparser.h"

// QtNetworkCrumbs headers
#include "qncliterals.h"
#include "qncparse.h"

// Qt headers
#include <QUrl>
#include <QVersionNumber>

namespace qnc::xml {
namespace {

enum Transition { Entering, Leaving };

const wchar_t *arrow(Transition transition)
{
    switch (transition) {
    case Entering:
        return L"==>";
    case Leaving:
        return L"<==";
    }

    return nullptr;
}

void reportTransition(const QLoggingCategory &category,
                      Transition transition, const QXmlStreamReader *reader,
                      const QString &parentState, const QString &childState)
{
    const auto lineNumber = static_cast<int>(reader->lineNumber());
    const auto columnNumber = static_cast<int>(reader->columnNumber());
    const auto &qualifiedName = reader->qualifiedName().toString();

    if (const auto &namespaceUri = reader->namespaceUri().toString(); namespaceUri.isEmpty()) {
        qCDebug(category,
                "%ls %ls %ls for <%ls> element at line %d, column %d",
                qUtf16Printable(parentState), arrow(transition), qUtf16Printable(childState),
                qUtf16Printable(qualifiedName), lineNumber, columnNumber);
    } else if (const auto &prefix = reader->prefix().toString(); prefix.isEmpty()) {
        qCDebug(category,
                "%ls %ls %ls for <%ls> element (with <%ls>) at line %d, column %d",
                qUtf16Printable(parentState), arrow(transition), qUtf16Printable(childState),
                qUtf16Printable(qualifiedName), qUtf16Printable(namespaceUri),
                lineNumber, columnNumber);
    } else if (const auto &prefix = reader->prefix().toString(); prefix.isEmpty()) {
        qCDebug(category,
                "%ls %ls %ls for <%ls> element (with %ls=<%ls>) at line %d, column %d",
                qUtf16Printable(parentState), arrow(transition), qUtf16Printable(childState),
                qUtf16Printable(qualifiedName), qUtf16Printable(prefix),
                qUtf16Printable(namespaceUri), lineNumber, columnNumber);
    }
}

void reportIgnoredElement(const QLoggingCategory &category, const QXmlStreamReader *reader)
{
    qCDebug(category,
            "Ignoring <%ls> element (with %ls=<%ls>) at line %d, column %d",
            qUtf16Printable(reader->qualifiedName().toString()),
            qUtf16Printable(reader->prefix().toString()),
            qUtf16Printable(reader->namespaceUri().toString()),
            static_cast<int>(reader->lineNumber()),
            static_cast<int>(reader->columnNumber()));
}

void reportIgnoredAttribute(const QLoggingCategory &category, const QXmlStreamReader *reader,
                            const QXmlStreamAttribute &attribute)
{
    qCDebug(category,
            "Ignoring %ls attribute for <%ls> element (with %ls=<%ls>) at line %d, column %d",
            qUtf16Printable(attribute.qualifiedName().toString()),
            qUtf16Printable(reader->qualifiedName().toString()),
            qUtf16Printable(attribute.prefix().toString()),
            qUtf16Printable(attribute.namespaceUri().toString()),
            static_cast<int>(reader->lineNumber()),
            static_cast<int>(reader->columnNumber()));
}

} // namespace

void updateVersion(QVersionNumber &version, VersionSegment segment, int number)
{
    const auto index = static_cast<int>(segment);
    auto segments = version.segments();

    if (segments.size() <= index)
        segments.resize(index + 1);

    segments[index] = number;
    version = QVersionNumber{segments};
}

template <>
void ParserBase::parseValue(QStringView text, const std::function<void(int)> &store)
{
    if (const auto &value = qnc::parse<int>(text))
        store(*value);
    else
        m_xml->raiseError(tr("Invalid number: %1").arg(text));
}

template <>
void ParserBase::parseValue(QStringView text, const std::function<void(QString)> &store)
{
    store(text.toString());
}

template <>
void ParserBase::parseValue(QStringView text, const std::function<void(QUrl)> &store)
{
    store(QUrl{text.toString()});
}

QString ParserBase::stateName(const QMetaEnum &metaEnum, int value)
{
    if (const auto key = metaEnum.valueToKey(value))
        return QString::fromLatin1(key);

    const auto &typeName = QString::fromLatin1(metaEnum.name());
    const auto &prefix = [metaEnum]() -> QString {
        if (const auto scope = metaEnum.scope())
            return QString::fromLatin1(scope) + "::"_L1;

        return {};
    }();

    return "%1%2(%4)"_L1.arg(prefix, typeName, QString::number(value));
}

QString ParserBase::AbstractContext::currentStateName() const
{
    if (Q_UNLIKELY(isEmpty()))
        return {};

    return stateName(currentState());
}

std::optional<int> ParserBase::AbstractContext::parseElement(QStringView elementName) const
{
    const auto currentStep = findStep(elementName);

    if (const auto nextState = std::get_if<int>(&currentStep)) {
        return *nextState;
    } else if (const auto parser = std::get_if<GenericParser>(&currentStep)) {
        std::invoke(*parser, nullptr);
        return currentState();
    } else if (const auto parser = std::get_if<ElementParser>(&currentStep)) {
        std::invoke(*parser);
        return currentState();
    } else {
        return {};
    }
}

bool ParserBase::AbstractContext::parseAttribute(QStringView elementName, const QXmlStreamAttribute &attribute) const
{
    const auto &attributeName = attribute.name().toString();
    const auto &fullPath = elementName.toString() + "/@"_L1 + attributeName;
    const auto &shortPath = QStringView{fullPath.cbegin() + elementName.size() + 1, fullPath.cend()};

    for (const auto path : {QStringView{fullPath}, shortPath}) {
        const auto &step = findStep(path);

        if (const auto parse = std::get_if<GenericParser>(&step)) {
            std::invoke(*parse, &attribute);
            return true;
        }
    }

    return false;
}

bool ParserBase::parse(const QLoggingCategory &category, AbstractContext &context)
{
    qCDebug(category, "Starting ==> %ls",
            qUtf16Printable(context.currentStateName()));

    while (!m_xml->atEnd()
           && !m_xml->hasError()
           && !context.isEmpty()) {
        switch (m_xml->readNext()) {
        case QXmlStreamReader::StartElement:
            parseStartElement(category, context);
            break;

        case QXmlStreamReader::EndElement:
            parseEndElement(category, context);
            break;

        case QXmlStreamReader::NoToken:
            m_xml->readNextStartElement(); // provoke error state
            Q_ASSERT(m_xml->error() == QXmlStreamReader::PrematureEndOfDocumentError);
            break;

        default:
            break;
        }
    }

    if (m_xml->hasError()) {
        qCWarning(category, "Error at line %d, column %d: %ls",
                  static_cast<int>(m_xml->lineNumber()),
                  static_cast<int>(m_xml->columnNumber()),
                  qUtf16Printable(m_xml->errorString()));

        return false;
    }

    return true;
}

void ParserBase::parseStartElement(const QLoggingCategory &category, AbstractContext &context)
{
    const auto &attributeList = m_xml->attributes();
    const auto elementName    = m_xml->name();

    if (!context.selectNamespace(m_xml->namespaceUri())) {
        reportIgnoredElement(category, m_xml);
        m_xml->skipCurrentElement();
    } else if (const auto &nextState = context.parseElement(elementName); !nextState) {
        m_xml->raiseError(tr("Unexpected element <%1> in %2 state").
                          arg(m_xml->qualifiedName(), context.currentStateName()));
    } else {
        if (nextState != context.currentState()) {
            reportTransition(category, Entering, m_xml,
                             context.stateName(context.currentState()),
                             context.stateName(nextState.value()));

            context.enterState(nextState.value());
        }

        for (const auto &attribute : attributeList) {
            if (!attribute.prefix().isEmpty()
                    && !context.selectNamespace(attribute.namespaceUri())) {
                reportIgnoredAttribute(category, m_xml, attribute);
            } else if (!context.parseAttribute(elementName, attribute)) {
                m_xml->raiseError(tr("Unexpected attribute %1 for element <%2> in %3 state").
                                  arg(attribute.qualifiedName(), m_xml->qualifiedName(),
                                      context.currentStateName()));
            }
        }
    }
}

void ParserBase::parseEndElement(const QLoggingCategory &category, AbstractContext &context)
{
    const auto initialState = context.currentState();

    context.leaveState();

    if (context.isEmpty()) {
        qCDebug(category, "%ls ==> leaving",
                qUtf16Printable(context.stateName(initialState)));
    } else {
        reportTransition(category, Leaving, m_xml,
                         context.stateName(context.currentState()),
                         context.stateName(initialState));
    }

}

QString ParserBase::readValue(const QXmlStreamAttribute *attribute)
{
    if (attribute)
        return attribute->value().toString();
    else
        return m_xml->readElementText();
}

} // namespace qnc::xml
