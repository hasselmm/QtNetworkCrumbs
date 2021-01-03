#include "mdnsmessage.h"

#include <QHostAddress>
#include <QLoggingCategory>

#include <QtEndian>

namespace MDNS {

namespace {

Q_LOGGING_CATEGORY(lcMessage, "mdns.message")

auto readUInt16(const char *p)
{
    return qFromBigEndian<quint16>(p);
}

void writeUInt16(char *p, quint16 value)
{
    qToBigEndian(value, p);
}

auto readUInt32(const char *p)
{
    return qFromBigEndian<quint32>(p);
}

auto makeByteArray(QList<Label> labels)
{
    QByteArray data;

    for (const auto &l: labels)
        data.append(l.data());

    return data;
}

auto makeLabelList(QList<QByteArray> strings)
{
    QList<Label> labels;

    for (const auto &s: strings) {
        Q_ASSERT(s.length() > 0);
        Q_ASSERT(s.length() < 64);
        labels.append(Label{static_cast<char>(s.length()) + s, 0});
    }

    labels.append(Label{QByteArray{"", 1}, 0});

    return labels;
}

} // namespace

Entry::Entry(QByteArray data, int offset)
    : m_data{std::move(data)}
    , m_offset{offset}
{}

quint16 Entry::u16(int offset) const
{
    return readUInt16(m_data.constData() + offset);
}

void Entry::setU16(int offset, quint16 value)
{
    writeUInt16(m_data.data() + offset, value);
}

quint32 Entry::u32(int offset) const
{
    return readUInt32(m_data.constData() + offset);
}

QByteArray Label::labelText() const
{
    return data().mid(offset() + 1, labelLength());
}

int Label::size() const
{
    if (isLabel())
        return 1 + u8(offset());
    if (isPointer())
        return 2;

    return 1;
}

Name::Name(QList<QByteArray> labels)
    : Entry{makeByteArray(makeLabelList(std::move(labels))), 0}
{}

QByteArray Name::toByteArray() const
{
    QByteArray name;

    for (int i = 0;; ++i) {
        const auto l = label(i);

        if (l.isPointer()) {
            name.append(Name{data(), l.pointer()}.toByteArray());
            break;
        }

        if (l.labelLength() == 0)
            break;

        name.append(l.labelText());
        name.append('.');
    }

    return name;
}

Label Name::label(int i) const
{
    return Label{data(), i > 0 ? label(i - 1).nextOffset() : offset()};
}

int Name::size() const
{
    int size = 0;

    for (int i = 0;; ++i) {
        const auto l = label(i);
        size += l.size();

        if (l.labelLength() == 0) // empty label, or pointer
            break;
    }

    return size;
}

Question::Question(QByteArray name, Message::Type type, Message::NetworkClass networkClass, bool flush) noexcept
    : Entry{Name{name.split('.')}.data() + QByteArray{4, Qt::Uninitialized}, 0}
{
    setU16(fieldsOffset() + TypeOffset, type);
    setU16(fieldsOffset() + FlagsOffset, (networkClass & 0x7f) | (flush ? 0x80 : 0x00));
}

QHostAddress Resource::address() const
{
    if (type() == Message::A && dataSize() == 4)
        return QHostAddress{u32(dataOffset())};
    else if (type() == Message::AAAA && dataSize() == 16)
        return QHostAddress{reinterpret_cast<const quint8 *>(data().constData() + dataOffset())};
    else
        return {};
}

Name Resource::pointer() const
{
    if (type() == Message::PTR && dataSize() > 0)
        return Name{data(), dataOffset()};
    else
        return Name{};
}

QByteArray Resource::text() const
{
    if (type() == Message::TXT)
        return {data().constData() + dataOffset(), dataSize()};
    else
        return {};
}

ServiceRecord Resource::service() const
{
    if (type() == Message::SRV && dataSize() >= 8)
        return ServiceRecord{data(), dataOffset()};
    else
        return ServiceRecord{};
}

Question Message::question(int i) const
{
    if (i == 0) {
        return Question{m_data, SizeOfFields};
    } else if (i < 0) {
        return question(questionCount() + i);
    } else if (i < questionCount()) {
        return Question{m_data, question(i - 1).nextOffset()};
    } else {
        return Question{};
    }
}

Resource Message::answer(int i) const
{
    if (i == 0) {
        if (questionCount() > 0)
            return Resource{m_data, question(-1).nextOffset()};
        else
            return Resource{m_data, SizeOfFields};
    } else if (i < 0) {
        return answer(answerCount() + i);
    } else if (i < answerCount()) {
        return Resource{m_data, answer(i - 1).nextOffset()};
    } else {
        return Resource{};
    }
}

Resource Message::authority(int i) const
{
    if (i == 0) {
        if (answerCount() > 0)
            return Resource{m_data, answer(-1).nextOffset()};
        else if (questionCount() > 0)
            return Resource{m_data, question(-1).nextOffset()};
        else
            return Resource{m_data, SizeOfFields};
    } else if (i < 0) {
        return authority(authorityCount() + i);
    } else if (i < authorityCount()) {
        return Resource{m_data, authority(i - 1).nextOffset()};
    } else {
        return Resource{};
    }
}

Resource Message::additional(int i) const
{
    if (i == 0) {
        if (authorityCount() > 0)
            return Resource{m_data, authority(-1).nextOffset()};
        else if (answerCount() > 0)
            return Resource{m_data, answer(-1).nextOffset()};
        else if (questionCount() > 0)
            return Resource{m_data, question(-1).nextOffset()};
        else
            return Resource{m_data, SizeOfFields};
    } else if (i < 0) {
        return additional(additionalCount() + i);
    } else if (i < additionalCount()) {
        return Resource{m_data, additional(i - 1).nextOffset()};
    } else {
        return Resource{};
    }
}

Message &Message::addQuestion(Question question)
{
    Q_ASSERT(answerCount() == 0);
    Q_ASSERT(authorityCount() == 0);
    Q_ASSERT(additionalCount() == 0);

    m_data.append(question.data());
    setU16(QuestionCountOffset, questionCount() + 1);

    return *this;
}

quint16 Message::u16(int offset) const
{
    return readUInt16(m_data.constData() + offset);
}

void Message::setU16(int offset, quint16 value)
{
    writeUInt16(m_data.data() + offset, value);
}

} // namespace MDNS

QDebug operator<<(QDebug debug, const MDNS::Message &message)
{
    const QDebugStateSaver saver{debug};

    if (debug.verbosity() >= QDebug::DefaultVerbosity)
        debug.nospace() << message.staticMetaObject.className();

    debug.nospace().verbosity(QDebug::MinimumVerbosity)
            << "(serial=" << message.serial()
            << ", flags=" << Qt::hex << message.flags() << Qt::dec
            << ", #questions=" << message.questionCount()
            << ", #anwers=" << message.answerCount()
            << ", #authorities=" << message.authorityCount()
            << ", #additionals=" << message.additionalCount()
            << "," << Qt::endl
            << "  questions=(";

    for (int i = 0; i < message.questionCount(); ++i) {
        if (i > 0) debug << "," << Qt::endl << "             ";
        debug << message.question(i);
    }

    debug << ")," << Qt::endl << "  answers=(";

    for (int i = 0; i < message.answerCount(); ++i) {
        if (i > 0) debug << "," << Qt::endl << "           ";
        debug << message.answer(i);
    }

    debug << ")," << Qt::endl << "  authorities=(";

    for (int i = 0; i < message.authorityCount(); ++i) {
        if (i > 0) debug << "," << Qt::endl << "               ";
        debug << message.authority(i);
    }

    debug << ")" << Qt::endl << "  additionals=(";

    for (int i = 0; i < message.additionalCount(); ++i) {
        if (i > 0) debug << "," << Qt::endl << "               ";
        debug << message.additional(i);
    }

    debug << "))";
    return debug;
}

QDebug operator<<(QDebug debug, const MDNS::Name &name)
{
    return debug << name.toByteArray();
}

QDebug operator<<(QDebug debug, const MDNS::Question &question)
{
    const QDebugStateSaver saver{debug};

    if (debug.verbosity() >= QDebug::DefaultVerbosity)
        debug.nospace() << question.staticMetaObject.className();

    debug.nospace().verbosity(QDebug::MinimumVerbosity)
            << "(name=" << question.name()
            << ", type=" << question.type()
            << ", class=" << question.networkClass()
            << ", flush=" << question.flush();

    if (debug.verbosity() > QDebug::DefaultVerbosity)
        debug << ", offset=" << question.offset()
              << ", size=" << question.size();

    return debug << ")";
}

QDebug operator<<(QDebug debug, const MDNS::Resource &resource)
{
    const QDebugStateSaver saver{debug};

    if (debug.verbosity() >= QDebug::DefaultVerbosity)
        debug.nospace() << resource.staticMetaObject.className();

    debug.nospace().verbosity(QDebug::MinimumVerbosity)
            << "(name=" << resource.name()
            << ", type=" << resource.type()
            << ", class=" << resource.networkClass()
            << ", flush=" << resource.flush()
            << ", ttl=" << resource.timeToLife()
            << ", dataSize=" << resource.dataSize();

    switch (resource.type()) {
    case MDNS::Message::A:
    case MDNS::Message::AAAA:
        debug << ", address=" << resource.address();
        break;

    case MDNS::Message::PTR:
        debug << ", pointer=" << resource.pointer();
        break;

    case MDNS::Message::TXT:
        debug << ", text=" << resource.text();
        break;

    case MDNS::Message::SRV:
        debug << ", service=" << resource.service();
        break;

    case MDNS::Message::ANY:
    case MDNS::Message::CNAME:
    case MDNS::Message::NSEC:
    case MDNS::Message::MX:
    case MDNS::Message::NS:
        break;
    }

    if (debug.verbosity() > QDebug::DefaultVerbosity)
        debug << ", offset=" << resource.offset()
              << ", size=" << resource.size();

    return debug << ")";
}

QDebug operator<<(QDebug debug, const MDNS::ServiceRecord &service)
{
    const QDebugStateSaver saver{debug};

    if (debug.verbosity() >= QDebug::DefaultVerbosity)
        debug.nospace() << service.staticMetaObject.className();

    return debug.nospace().verbosity(QDebug::MinimumVerbosity)
            << "(priority=" << service.priority()
            << ", weight=" << service.weight()
            << ", port=" << service.port()
            << ", target=" << service.target()
            << ")";
}