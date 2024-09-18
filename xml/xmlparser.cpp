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
void ParserBase::read(const std::function<void(int)> &store)
{
    const auto &text = m_xml->readElementText();

    if (const auto &value = qnc::parse<int>(text))
        store(*value);
    else
        m_xml->raiseError(tr("Invalid number: %1").arg(text));
}

template <>
void ParserBase::read(const std::function<void(QString)> &store)
{
    store(m_xml->readElementText());
}

template <>
void ParserBase::read(const std::function<void(QUrl)> &store)
{
    store(QUrl{m_xml->readElementText()});
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
    if (!context.selectNamespace(m_xml->namespaceUri())) {
        qCDebug(category, "Ignoring <%ls> element (with %ls=<%ls>) at line %d, column %d",
                qUtf16Printable(m_xml->qualifiedName().toString()),
                qUtf16Printable(m_xml->prefix().toString()),
                qUtf16Printable(m_xml->namespaceUri().toString()),
                static_cast<int>(m_xml->lineNumber()),
                static_cast<int>(m_xml->columnNumber()));

        m_xml->skipCurrentElement();
    } else if (const auto &nextState = context.processElement(m_xml->name()); !nextState) {
        m_xml->raiseError(tr("Unexpected <%1> element in %2 state").
                          arg(m_xml->name(), context.currentStateName()));
    } else if (nextState != context.currentState()) {
        reportTransition(category, Entering, m_xml,
                         context.stateName(context.currentState()),
                         context.stateName(nextState.value()));

        context.enterState(nextState.value());
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

} // namespace qnc::xml
