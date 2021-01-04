#ifndef MDNS_MDNSMESSAGE_H
#define MDNS_MDNSMESSAGE_H

#include <QMetaType>
#include <QByteArray>

class QHostAddress;

namespace MDNS {

class Question;
class Resource;

class Entry
{
    Q_GADGET

public:
    Entry() noexcept  = default;
    explicit Entry(QByteArray data, int offset);

    int offset() const { return m_offset; }
    auto data() const { return m_data; }

protected:
    quint8 u8(int offset) const { return static_cast<quint8>(m_data.at(offset)); }
    quint16 u16(int offset) const;
    quint32 u32(int offset) const;

    void setU16(int offset, quint16 value);

private:
    QByteArray m_data;
    int m_offset = 0;
};

class Message
{
    Q_GADGET

    enum FieldOffset {
        SerialOffset = 0,
        FlagsOffset = 2,
        QuestionCountOffset = 4,
        AnswerCountOffset = 6,
        AuthorityCountOffset = 8,
        AdditionalCountOffset = 10,
        SizeOfFields = 12,
    };

public:
    enum Type
    {
        A = 1,
        AAAA = 28,
        ANY = 255,
        CNAME = 5,
        NSEC = 47,
        MX = 15,
        NS = 2,
        OPT = 41,
        PTR = 12,
        SRV = 33,
        TXT = 16,
    };

    Q_ENUM(Type)

    enum NetworkClass
    {
        IN = 1,
    };

    Q_ENUM(NetworkClass)

    enum Flag
    {
        IsResponse = (1 << 15),
        OperationCode = (15 << 11),
        AuthoritativeAnswer = (1 << 10),
        Truncated = (1 << 9),
        RecursionDesired = (1 << 8),
        RecursionAvailable = (1 << 7),
        ResponseCode = (15 << 0),
    };

    Q_FLAG(Flag)
    Q_DECLARE_FLAGS(Flags, Flag)

    enum Operation
    {
        Query,
        IQuery,
        Status,
    };

    Q_ENUM(Operation)

    enum Error
    {
        NoError,
        FormatError,
        ServerError,
        NameError,
        NotImplemented,
        Refused,
    };

    Q_ENUM(Error)

    Message() noexcept
        : m_data{SizeOfFields, '\0'} {}
    explicit Message(QByteArray data) noexcept
        : m_data{std::move(data)} {}

    int serial() const { return u16(SerialOffset); }
    Flags flags() const { return Flags{u16(FlagsOffset)}; }
    int questionCount() const { return u16(QuestionCountOffset); }
    int answerCount() const { return u16(AnswerCountOffset); }
    int authorityCount() const { return u16(AuthorityCountOffset); }
    int additionalCount() const { return u16(AdditionalCountOffset); }

    bool isQuery() const { return !flags().testFlag(Flag::IsResponse); }
    bool isResponse() const { return flags().testFlag(Flag::IsResponse); }
    Operation operation() const { return static_cast<Operation>((static_cast<int>(flags()) >> 11) & 15); }
    Error error() const { return static_cast<Error>(static_cast<int>(flags()) & 15); }

    Question question(int i) const;
    Resource answer(int i) const;
    Resource authority(int i) const;
    Resource additional(int i) const;

    Message &addQuestion(Question question);
    Message &addAnswer(Resource resource);
    Message &addAuthority(Resource resource);
    Message &addAdditional(Resource resource);

    auto data() const { return m_data; }

private:
    quint16 u16(int offset) const;
    void setU16(int offset, quint16 value);

    QByteArray m_data;
};

class Label : public Entry
{
    Q_GADGET

public:
    using Entry::Entry;

    bool isLabel() const { return (u8(offset()) & 0xc0) == 0x00; }
    int labelLength() const { return isLabel() ? u8(offset()) : 0; }
    QByteArray labelText() const;

    bool isPointer() const { return (u8(offset()) & 0xc0) == 0xc0; }
    int pointer() const { return u16(offset()) & 0x3fff; }

    int size() const;
    int nextOffset() const { return offset() + size(); }
};

class Name : public Entry
{
    Q_GADGET

public:
    using Entry::Entry;

    explicit Name(QList<QByteArray> labels);

    QByteArray toByteArray() const;
    Label label(int i) const;

    int size() const;
    int nextOffset() const { return offset() + size(); }
};

class Question : public Entry
{
    Q_GADGET

    enum FieldOffset {
        TypeOffset = 0,
        FlagsOffset = 2,
        SizeOfFields = 4,
    };

public:
    using Entry::Entry;

    Question(QByteArray name, Message::Type type, Message::NetworkClass networkClass, bool flush = false) noexcept;
    Question(QByteArray name, Message::Type type, bool flush = false) noexcept
        : Question{std::move(name), type, Message::IN, flush} {}

    auto name() const { return Name{data(), offset()}; }
    auto type() const { return static_cast<Message::Type>(u16(fieldsOffset() + TypeOffset)); }
    auto networkClass() const { return static_cast<Message::NetworkClass>(u16(fieldsOffset() + FlagsOffset) & 0x7ff); }
    bool flush() const { return u16(fieldsOffset() + FlagsOffset) & 0x8000; }
    int size() const { return name().size() + SizeOfFields; }

    int fieldsOffset() const { return offset() + name().size(); }
    int nextOffset() const { return offset() + size(); }
};

class ServiceRecord : public Entry
{
    Q_GADGET

public:
    using Entry::Entry;

    int priority() const { return u16(offset()); }
    int weight() const { return u16(offset() + 2); }
    int port() const { return u16(offset() + 4); }
    auto target() const { return Name{data(), offset() + 6}; }
};

class Resource : public Entry
{
    Q_GADGET

    enum FieldOffset {
        TypeOffset = 0,
        FlagsOffset = 2,
        TimeToLifeOffset = 4,
        DataSizeOffset = 8,
        SizeOfFields = 10,
    };

public:
    using Entry::Entry;

    auto name() const { return Name{data(), offset()}; }
    auto type() const { return static_cast<Message::Type>(u16(fieldsOffset() + TypeOffset)); }
    auto networkClass() const { return static_cast<Message::NetworkClass>(u16(fieldsOffset() + FlagsOffset) & 0x7ff); }
    bool flush() const { return u16(fieldsOffset() + FlagsOffset) & 0x8000; }
    qint64 timeToLife() const { return u32(fieldsOffset() + TimeToLifeOffset); }
    int dataSize() const { return u16(fieldsOffset() + DataSizeOffset); }
    int size() const { return name().size() + SizeOfFields + dataSize(); }

    QHostAddress address() const;
    Name pointer() const;
    QByteArray text() const;
    ServiceRecord service() const;

    int fieldsOffset() const { return offset() + name().size(); }
    int dataOffset() const { return fieldsOffset() + SizeOfFields; }
    int nextOffset() const { return offset() + size(); }
};

} // namespace MDNS

QDebug operator<<(QDebug, const MDNS::Label &);
QDebug operator<<(QDebug, const MDNS::Message &);
QDebug operator<<(QDebug, const MDNS::Name &);
QDebug operator<<(QDebug, const MDNS::Question &);
QDebug operator<<(QDebug, const MDNS::Resource &);
QDebug operator<<(QDebug, const MDNS::ServiceRecord &);

#endif // MDNS_MDNSMESSAGE_H
