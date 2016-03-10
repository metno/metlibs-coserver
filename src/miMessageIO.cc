
#include "miMessageIO.h"

#include "miMessage.h"

#include <QDataStream>
#include <QIODevice>

#define MILOGGER_CATEGORY "coserver.MessageIO"
#include <qUtilities/miLoggingQt.h>

namespace {
QString readString(QDataStream& in)
{
    QString tmp;
    in >> tmp;
    return tmp;
}
QStringList readStringAndSplit(QDataStream& in, int count = 2)
{
    QString tmp;
    in >> tmp;
    if (count >= 2)
        return tmp.split(':');
    else
        return QStringList(tmp);
}

const qint32 MAGIC_COSERVER = -(0xC04C0DE);
} // namespace

miMessageIO::miMessageIO(QIODevice* d, bool server)
    : mDevice(d)
    , mIsServer(server)
    , mReadBlockSize(0)
    , mProtocolVersion(0)
{
}

bool miMessageIO::read(int& fromId, ClientIds& toIds, miQMessage& qmsg)
{
    METLIBS_LOG_SCOPE();
    QDataStream in(mDevice);
    in.setVersion(QDataStream::Qt_4_0);

    if (mReadBlockSize == 0) {
        if (mDevice->bytesAvailable() < (int)sizeof(mReadBlockSize))
            return false;
        in >> mReadBlockSize;
    }

    if (mDevice->bytesAvailable() < mReadBlockSize)
        return false;

    qint32 first;
    in >> first;
    if (first == MAGIC_COSERVER) {
        quint32 version;
        in >> version;
        if (mProtocolVersion < version)
            mProtocolVersion = version;
        if (version == 1) {
            readV1(in, fromId, toIds, qmsg);
        } else {
            METLIBS_LOG_ERROR("protocol version " << version << " not supported");
            in.skipRawData(mReadBlockSize - 4); // length - length(version)
        }
    } else {
        readV0(in, first, fromId, toIds, qmsg);
    }
    mReadBlockSize = 0;
    return true;
}

void miMessageIO::write(int from, const ClientIds& toIds, const miQMessage& qmsg)
{
    METLIBS_LOG_SCOPE();

    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_4_0);
    out << (quint32)0;

    if (protocolVersion() == 0) {
        writeV0(out, from, toIds, qmsg);
    } else {
        out << MAGIC_COSERVER;
        out << (quint32) protocolVersion();
        if (protocolVersion() == 1)
            writeV1(out, from, toIds, qmsg);
    }

    out.device()->seek(0);
    out << (quint32)(block.size() - sizeof(quint32)); // exclude 4 bytes with block size from length

    mDevice->write(block);
}

void miMessageIO::writeV0(QDataStream& out, int from, const ClientIds& toIds, const miQMessage& qmsg)
{
    METLIBS_LOG_SCOPE();
    qint32 fakeTo = (toIds.size() == 1) ? std::max(-1, *toIds.begin()) : -1;
    out << fakeTo;
    if (mIsServer)
        out << from;
    out << qmsg.command();

    out << qmsg.getDataDesc().join(":");
    out << qmsg.getCommonDesc().join(":");
    out << qmsg.getCommonValues().join(":");

    out << QString(); // clientType
    out << QString(); // co
    out << (quint32)qmsg.countDataRows(); // NOT A FIELD IN MIMESSAGE (TEMP ONLY)
    for (int r=0; r<qmsg.countDataRows(); r++)
        out << qmsg.getDataValues(r).join(":");
}

void miMessageIO::readV0(QDataStream& in, int first, int& fromId, ClientIds& toIds, miQMessage& qmsg)
{
    METLIBS_LOG_SCOPE();
    toIds.clear();
    if (first != -1)
        toIds.insert(first);

    if (!mIsServer)
        in >> fromId;
    qmsg.setCommand(readString(in));
    const QStringList dataDesc = readStringAndSplit(in);
    const QStringList commonDesc = readStringAndSplit(in);
    const QStringList commonValues = readStringAndSplit(in, commonDesc.count());
    readString(in); // clientType
    readString(in); // co
    int size = 0;
    in >> size; // NOT A FIELD IN MIMESSAGE (METADATA ONLY)
    QList<QStringList> dataRows;
    dataRows.reserve(size);
    for (int i = 0; i < size; i++)
        dataRows << readStringAndSplit(in, dataDesc.count());

    qmsg.setCommon(commonDesc, commonValues);
    qmsg.setData(dataDesc, dataRows);
}

void miMessageIO::writeV1(QDataStream& out, int fromId, const ClientIds& toIds, const miQMessage& qmsg)
{
    METLIBS_LOG_SCOPE();
    if (!mIsServer) {
        QList<qint32> to;
        for (ClientIds::const_iterator it = toIds.begin(); it != toIds.end(); ++it)
            to << *it;;
        out << to;
    } else {
        out << fromId;
    }
    out << qmsg.command()
        << qmsg.getCommonDesc()
        << qmsg.getCommonValues()
        << qmsg.getDataDesc();

    const quint32 rows = qmsg.countDataRows();
    out << rows;
    for (int i = 0; i < rows; i++)
        out << qmsg.getDataValues(i);
}

void miMessageIO::readV1(QDataStream& in, int& fromId, ClientIds& toIds, miQMessage& qmsg)
{
    METLIBS_LOG_SCOPE();
    if (mIsServer) {
        QList<qint32> to;
        in >> to;
        toIds = ClientIds(to.begin(), to.end());
    } else {
        in >> fromId;
    }
    qmsg.setCommand(readString(in));
    QStringList commonDesc, commonValues, dataDesc, dataRow;
    in >> commonDesc
       >> commonValues
       >> dataDesc;

    quint32 rows = 0;
    in >> rows;
    QList<QStringList> dataRows;
    dataRows.reserve(rows);
    for (int i = 0; i < rows; i++) {
        in >> dataRow;
        dataRows << dataRow;
    }

    qmsg.setCommon(commonDesc, commonValues);
    qmsg.setData(dataDesc, dataRows);
}
