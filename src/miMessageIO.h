#ifndef METLIBS_COSERVER_MESSAGEIO_H
#define METLIBS_COSERVER_MESSAGEIO_H 1

#include "miMessage.h"

#include <QtGlobal> // quint32

class QIODevice;

class miMessageIO {
public:
    miMessageIO(QIODevice* device, bool server);

    // return true if complete
    bool read(int& from, ClientIds& to, miQMessage& qmsg);

    void write(int from, const ClientIds& to, const miQMessage& qmsg);

    int protocolVersion() const
        { return mProtocolVersion; }

    void setProtocolVersion(int pv)
        { mProtocolVersion = pv; }

private:
    void writeV0(QDataStream& out, int from, const ClientIds& toIds, const miQMessage& qmsg);
    void readV0(QDataStream& in, int first, int& fromId, ClientIds& toIds, miQMessage& qmsg);

    void writeV1(QDataStream& out, int fromId, const ClientIds& toIds, const miQMessage& qmsg);
    void readV1(QDataStream& in, int& fromId, ClientIds& toIds, miQMessage& qmsg);

private:
    QIODevice* mDevice;
    bool mIsServer;

    quint32 mReadBlockSize;
    int mProtocolVersion;
};

#endif // METLIBS_COSERVER_MESSAGEIO_H
