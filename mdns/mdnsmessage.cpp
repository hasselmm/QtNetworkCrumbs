/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2021 Mathias Hasselmann
 */
#include "mdnsmessage.h"

// Qt headers
#include <QHostAddress>
#include <QLoggingCategory>

#include <QtEndian>

namespace MDNS {

namespace {

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

auto makeLabelList(QHostAddress address)
{
    if (address.protocol() == QAbstractSocket::IPv4Protocol) {
        const auto ipv4 = address.toIPv4Address();
        return makeLabelList({
                                 QByteArray::number((ipv4 >> 0) & 255),
                                 QByteArray::number((ipv4 >> 8) & 255),
                                 QByteArray::number((ipv4 >> 16) & 255),
                                 QByteArray::number((ipv4 >> 24) & 255),
                                 "in-addr", "arpa"
                             });
    }

    if (address.protocol() == QAbstractSocket::IPv6Protocol) {
        const auto ipv6 = address.toIPv6Address();

        QByteArrayList labels;
        labels.reserve(34);

        for (int i = 15; i >= 0; --i) {
            labels += QByteArray::number((ipv6[i] >> 0) & 15, 16);
            labels += QByteArray::number((ipv6[i] >> 4) & 15, 16);
        }

        labels += "ip6";
        labels += "arpa";

        return makeLabelList(labels);
    }

    return QList<Label>{};
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

QByteArray Label::toByteArray() const
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
    : Entry{makeByteArray(makeLabelList(std::move(labels))), 0} {}
Name::Name(QHostAddress address)
    : Entry{makeByteArray(makeLabelList(std::move(address))), 0} {}

QByteArray Name::toByteArray() const
{
    QByteArray name;

    for (const auto &label: *this) {
        if (!name.isEmpty())
            name.append('.');

        if (label.isPointer()) {
            name.append(Name{data(), label.pointer()}.toByteArray());
            break;
        }

        name.append(label.toByteArray());
    }

    return name;
}

int Name::labelCount() const
{
    for (int i = 0;; ++i) {
        const auto l = label(i);
        if (l.isPointer())
            return i + 1;
        if (label(i).labelLength() == 0)
            return i;
    }
}

Label Name::label(int i) const
{
    const auto dataOffset = i > 0 ? label(i - 1).nextOffset() : offset();
    return Label{data(), dataOffset};
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
    : Question{Name{name.split('.')}, type, networkClass, flush} {}
Question::Question(QHostAddress address, Message::Type type, Message::NetworkClass networkClass, bool flush) noexcept
    : Question{Name{std::move(address)}, type, networkClass, flush} {}
Question::Question(QHostAddress address, Message::Type type, bool flush) noexcept
    : Question{Name{std::move(address)}, type, Message::IN, flush} {}

Question::Question(Name name, Message::Type type, Message::NetworkClass networkClass, bool flush) noexcept
    : Entry{name.data() + QByteArray{4, Qt::Uninitialized}, 0}
{
    setU16(fieldsOffset() + TypeOffset, type);
    setU16(fieldsOffset() + FlagsOffset, static_cast<quint16>((networkClass & 0x7fU) | (flush ? 0x80U : 0x00U)));
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

Resource Message::response(int i) const
{
    if (i < 0)
        return response(responseCount() - 1);

    if (i < answerCount())
        return answer(i);

    i -= answerCount();

    if (i < authorityCount())
        return authority(i);

    i -= authorityCount();

    if (i < additionalCount())
        return additional(i);

    return {};
}

Message &Message::addQuestion(Question question)
{
    Q_ASSERT(answerCount() == 0);
    Q_ASSERT(authorityCount() == 0);
    Q_ASSERT(additionalCount() == 0);
    Q_ASSERT(questionCount() < 0xffff);

    m_data.append(question.data());
    setU16(QuestionCountOffset, static_cast<quint16>(questionCount() + 1));

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
    case MDNS::Message::OPT:
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

#include "moc_mdnsmessage.cpp"
