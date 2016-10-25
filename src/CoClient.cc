/**
 * coclient - coserver client file
 * @author Martin Lilleeng Saetra <martinls@met.no>
 *
 * Copyright (C) 2013-2015 met.no
 *
 * Contact information:
 * Norwegian Meteorological Institute
 * Box 43 Blindern
 * 0313 OSLO
 * NORWAY
 * email: diana@met.no
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "CoClient.h"

#include "miMessage.h"
#include "miMessageIO.h"
#include "QLetterCommands.h"

#include <QtCore/QProcess>
#include <QtCore/QStringList>
#include <QtCore/QTimer>

#define MILOGGER_CATEGORY "coserver.CoClient"
#include <qUtilities/miLoggingQt.h>

#ifdef __WIN32__
// GetUserName()
#include <windows.h>
#include <lmcons.h>
#else
// uid_t and getpwnam()
#include <sys/types.h>
#include <pwd.h>
#endif

#include <boost/algorithm/string.hpp>

#include <fstream>
#include <sstream>
#include <unistd.h>

namespace /* anonymous */ {

QString safe_getenv(const char* var)
{
    const char* v = getenv(var);
    if (not v)
        return QString();
    return v;
}

const int SERVER_START_RETRY_DELAY = 15;

QString getUserId()
{
#ifdef __WIN32__
    DWORD size = UNLEN + 1;
    CHAR name[size];
    if (!GetUserNameA(name, &size)) {
        std::cerr << "GetUserNameA() failed" << std::endl;
        return "UnknownWin32User";
    } else {
        return name;
    }
#else
    uid_t uid = getuid();
    struct passwd *pw = getpwuid(uid);
    if (pw) {
        return pw->pw_name;
    } else {
        return QString("UnknownUser") + QString::number(uid);
    }
#endif
}

} // namespace anonymous

CoClient::CoClient(const QString& ct, const QString& h, quint16 port, QObject* parent)
    : QObject(parent)
{
    initialize(ct);

    if (port == 0)
        port = qmstrings::port;
    serverUrl.setScheme("co4");
    if (getenv("COSERVER_HOST") != NULL) {
        serverUrl.setHost(getenv("COSERVER_HOST"));
    } else {
        serverUrl.setHost(h);
    }
    serverUrl.setPort(port);
}

CoClient::CoClient(const QString& ct, const QUrl& url, QObject* parent)
    : QObject(parent)
    , serverUrl(url)
{
    initialize(ct);
}

CoClient::~CoClient()
{
    METLIBS_LOG_SCOPE();
}

void CoClient::initialize(const QString& ct)
{
    tcpSocket = 0;
    localSocket = 0;
    mId = -1;
    name = clientType = ct;
    serverCommand = "coserver4";
    mAttemptToStartServer = true;
    mNextAttemptToStartServer = QDateTime::currentDateTime().addSecs(-30);

    userid = safe_getenv("COSERVER_USER");
    if (userid.isEmpty())
        userid = getUserId();
}

void CoClient::setUserId(const QString& user)
{
    userid = user;
}

void CoClient::createSocket()
{
    QObject* device;
    if (serverUrl.scheme() == "co4") {
        tcpSocket = new QTcpSocket(this);
        io.reset(new miMessageIO(tcpSocket, false));
        device = tcpSocket;

        connect(tcpSocket, SIGNAL(error(QAbstractSocket::SocketError)),
                SLOT(tcpError(QAbstractSocket::SocketError)));
    } else if (serverUrl.scheme() == "local") {
        localSocket = new QLocalSocket(this);
        io.reset(new miMessageIO(localSocket, false));
        device = localSocket;

        connect(localSocket, SIGNAL(error(QLocalSocket::LocalSocketError)),
                SLOT(localError(QLocalSocket::LocalSocketError)));
    } else {
        METLIBS_LOG_ERROR("bad server url '" << serverUrl.toString(QUrl::RemovePassword) << "'");
        return;
    }
    // signals are name based, so it is not a problem to mix classes
    connect(device, SIGNAL(connected()),
            SLOT(connectionEstablished()));
    connect(device, SIGNAL(disconnected()),
            SLOT(connectionClosed()));
    connect(device, SIGNAL(readyRead()),
            SLOT(readNew()));
}

void CoClient::connectToServer()
{
    METLIBS_LOG_SCOPE();
    if (!io.get())
        createSocket();
    if (tcpSocket) {
        METLIBS_LOG_INFO("connecting to host '" << serverUrl.host() << "' port " << serverUrl.port());
        tcpSocket->connectToHost(serverUrl.host(), serverUrl.port());
    } else if (localSocket) {
        METLIBS_LOG_INFO("connecting to server '" << serverUrl.path() << "'");
        localSocket->connectToServer(serverUrl.path());
    }
}

void CoClient::disconnectFromServer()
{
    tcpSocket->disconnectFromHost();
}

bool CoClient::notConnected()
{
    if (tcpSocket)
        return (QAbstractSocket::UnconnectedState == tcpSocket->state());
    else if (localSocket)
        return (QLocalSocket::UnconnectedState == localSocket->state());
    else
        return true;
}

bool CoClient::isConnected()
{
    if (tcpSocket)
        return (QAbstractSocket::ConnectedState == tcpSocket->state());
    else if (localSocket)
        return (QLocalSocket::ConnectedState == localSocket->state());
    else
        return false;
}

void CoClient::connectionClosed()
{
    METLIBS_LOG_SCOPE();

    for (clients_t::const_iterator it = clients.begin(); it != clients.end(); ++it) {
        METLIBS_LOG_DEBUG(LOGVAL(it->first));
        Q_EMIT clientChange(it->first, CLIENT_GONE);
        Q_EMIT clientChange(it->first, CLIENT_UNREGISTERED);
    }
    clients.clear();
    mId = -1;

    Q_EMIT disconnected();
}

void CoClient::readNew()
{
    METLIBS_LOG_SCOPE();
    int from;
    ClientIds to;
    miQMessage qmsg;
    while (io->read(from, to, qmsg)) {
        bool send = true;
        if (from == 0)
            send = messageFromServer(qmsg);
        if (send)
            emitMessage(from, qmsg);
    }
}

void CoClient::emitMessage(int fromId, const miQMessage& qmsg)
{
    METLIBS_LOG_SCOPE(qmsg);
    Q_EMIT receivedMessage(fromId, qmsg);

    miMessage msg;
    convert(fromId, mId, qmsg, msg);
    Q_EMIT receivedMessage(msg);
}

void CoClient::setName(const QString& n)
{
    METLIBS_LOG_SCOPE();
    if (n == name)
        return;

    name = n;

    miQMessage qmsg("SETNAME");
    qmsg.addCommon("name", name);
    sendMessageToServer(qmsg);
}

ClientIds CoClient::getClientIds() const
{
    ClientIds ids;
    for (clients_t::const_iterator it = clients.begin(); it != clients.end(); ++it)
        ids.insert(it->first);
    return ids;
}

QString CoClient::getClientType(int id) const
{
    clients_t::const_iterator it = clients.find(id);
    if (it != clients.end())
        return it->second.type;
    else
        return QString();
}

bool CoClient::hasClientOfType(const QString& type) const
{
    for (clients_t::const_iterator it = clients.begin(); it != clients.end(); ++it) {
        if (it->second.type == type) {
            return true;
        }
    }
    return false;
}

QString CoClient::getClientName(int id) const
{
    clients_t::const_iterator it = clients.find(id);
    if (it != clients.end())
        return it->second.name;
    else
        return QString();
}

void CoClient::setSelectedPeerNames(const QStringList& names)
{
    METLIBS_LOG_SCOPE();
    mSelectedPeerNames = names;
    sendSetPeers();
}

void CoClient::sendSetPeers()
{
    METLIBS_LOG_SCOPE();
    miQMessage setpeers("SETPEERS");
    setpeers.addDataDesc("peer_ids");
    for (clients_t::const_iterator it = clients.begin(); it != clients.end(); ++it) {
        if (mSelectedPeerNames.isEmpty() || mSelectedPeerNames.contains(it->second.name))
            setpeers.addDataValues(QStringList(QString::number(it->first)));
    }
    sendMessageToServer(setpeers);
}

bool CoClient::messageFromServer(const miQMessage& qmsg)
{
    METLIBS_LOG_SCOPE(LOGVAL(qmsg.command()));

    if (qmsg.command() == qmstrings::registeredclient) {
        handleRegisteredClient(qmsg);
    } else if (qmsg.command() == qmstrings::newclient) {
        handleNewClient(qmsg);
    } else if (qmsg.command() == qmstrings::renameclient) {
        handleRenameClient(qmsg);
    } else if (qmsg.command() == qmstrings::removeclient) {
        handleRemoveClient(qmsg);
    } else if (qmsg.command() == qmstrings::unregisteredclient) {
        handleUnregisteredClient(qmsg);
    } else {
        METLIBS_LOG_ERROR("Error editing client list, unknown command '" << qmsg.command() << "'");
    }
    return true;
}

void CoClient::handleRegisteredClient(const miQMessage& qmsg)
{
    METLIBS_LOG_SCOPE();

    // this client's id is sent in the first message from the server
    const int idxMyId = qmsg.findCommonDesc("id");
    if (idxMyId >= 0) {
        bool idOk = false;
        const int myId = qmsg.getCommonValue(idxMyId).toInt(&idOk);
        if (idOk) {
            mId = myId;
            METLIBS_LOG_INFO("received my id " << mId);
            Q_EMIT receivedId(mId);
        } else {
            METLIBS_LOG_WARN("could not parse id from '" << qmsg.getCommonValue(idxMyId) << "'");
        }
    }

    // other clients' ids are sent in the first message or later
    const int idxId = qmsg.findDataDesc("id"),
            idxType = qmsg.findDataDesc("type"),
            idxName = qmsg.findDataDesc("name");
    if (idxId >= 0 && idxName >= 0 && idxType >= 0) {
        for (int i=0; i<qmsg.countDataRows(); ++i) {
            const QString& pIdText = qmsg.getDataValue(i, idxId);
            const int pId = pIdText.toInt();
            const QString& pName = qmsg.getDataValue(i, idxName);
            const QString& pType = qmsg.getDataValue(i, idxType);

            clients_t::iterator it = clients.find(pId);
            if (it == clients.end()) {
                clients.insert(std::make_pair(pId, Client(pType, pName)));
                METLIBS_LOG_INFO("registered client " << pId << " of type " << pType);
                Q_EMIT clientChange(pId, CLIENT_REGISTERED);
            } else {
                METLIBS_LOG_WARN("bad registered message for known client " << pId);
            }
        }
        sendSetPeers();
    }
}

void CoClient::handleUnregisteredClient(const miQMessage& qmsg)
{
    METLIBS_LOG_SCOPE();
    const int idxId = qmsg.findDataDesc("id");
    if (idxId >= 0 && qmsg.countDataRows() > 0) {
        for (int i=0; i<qmsg.countDataRows(); ++i) {
            const QString& pIdText = qmsg.getDataValue(i, idxId);
            const int pId = pIdText.toInt();

            clients_t::iterator it = clients.find(pId);
            if (it != clients.end()) {
                Q_EMIT clientChange(pId, CLIENT_UNREGISTERED);
                clients.erase(pId);
                METLIBS_LOG_INFO("unregistered client " << pId);
            } else {
                METLIBS_LOG_WARN("bad unregistered message for client " << pId);
            }
        }
        sendSetPeers();
    }
}

void CoClient::handleNewClient(const miQMessage& qmsg)
{
    METLIBS_LOG_SCOPE();

    const int id = qmsg.getCommonValue("id").toInt();
    clients_t::iterator it = clients.find(id);
    if (it != clients.end() && !it->second.connected) {
        METLIBS_LOG_INFO("connected with client " << id);
        it->second.connected = true;
        Q_EMIT clientChange(id, CLIENT_NEW);
        Q_EMIT newClient(it->second.type);
        Q_EMIT newClient(it->second.type.toStdString());
        Q_EMIT addressListChanged();
    } else {
        METLIBS_LOG_WARN("bad connect message for client " << id);
    }
}

void CoClient::handleRemoveClient(const miQMessage& qmsg)
{
    METLIBS_LOG_SCOPE();

    const int id = qmsg.getCommonValue("id").toInt();
    clients_t::iterator it = clients.find(id);
    if (it != clients.end() && it->second.connected) {
        it->second.connected = false;
        METLIBS_LOG_INFO("diconnected from client " << id);

        Q_EMIT clientChange(id, CLIENT_GONE);
        Q_EMIT newClient(std::string("myself"));
        Q_EMIT addressListChanged();
    } else {
        METLIBS_LOG_WARN("bad disconnect message for client " << id);
    }
}

void CoClient::handleRenameClient(const miQMessage& qmsg)
{
    METLIBS_LOG_SCOPE();

    const int id = qmsg.getCommonValue("id").toInt();
    const QString name = qmsg.getCommonValue("name");

    clients_t::iterator it = clients.find(id);
    if (it != clients.end()) {
        bool renamedPeer = false;
        for (int i=0; i<mSelectedPeerNames.count(); ++i) {
            if (mSelectedPeerNames[i] == it->second.name) {
                mSelectedPeerNames[i] = name;
                renamedPeer = true;
                break;
            }
        }
        it->second.name = name;
        if (renamedPeer)
            sendSetPeers();
        Q_EMIT clientChange(it->first, CLIENT_RENAME);
    } else {
        METLIBS_LOG_WARN("bad rename message for unknown client " << id);
    }
}

void CoClient::connectionEstablished()
{
    METLIBS_LOG_SCOPE();
    sendClientType();
    Q_EMIT connected();
}

void CoClient::sendClientType()
{
    METLIBS_LOG_SCOPE();
    miQMessage qmsg("SETTYPE");
    qmsg.addCommon("type", clientType);
    qmsg.addCommon("userId", userid);
    qmsg.addCommon("name", name);
    qmsg.addCommon("protocolVersion", 1);

    sendMessageToServer(qmsg);
}

void CoClient::sendMessageToServer(const miQMessage& qmsg)
{
    sendMessage(qmsg, clientId(0));
}

bool CoClient::sendMessage(const miMessage &msg)
{
    if (!isConnected())
        return false;

    int from, to;
    miQMessage qmsg;
    convert(msg, from, to, qmsg);
    ClientIds receivers;
    if (to != qmstrings::all/* && to != qmstrings::default_id*/)
        receivers.insert(to);
    return sendMessage(qmsg, receivers);
}

bool CoClient::sendMessage(const miQMessage& qmsg, const ClientIds& to)
{
    METLIBS_LOG_SCOPE(qmsg);
    if (!isConnected())
        return false;

    METLIBS_LOG_INFO(LOGVAL(mId) << " protocolVersion=" << io->protocolVersion());

    io->write(-1 /*ignored*/, to, qmsg);
    if (tcpSocket)
        tcpSocket->waitForBytesWritten(250);
    else if (localSocket)
        localSocket->waitForBytesWritten(250);
    return true;
}

void CoClient::tcpError(QAbstractSocket::SocketError e)
{
    METLIBS_LOG_SCOPE();
    if (QAbstractSocket::ConnectionRefusedError == e) {
        METLIBS_LOG_INFO("could not connect to coserver, trying to start");
        tryToStartCoServer();
    } else if (QAbstractSocket::RemoteHostClosedError == e) {
        METLIBS_LOG_INFO("connection to coserver closed unexpectedly, trying to restart");
        tryToStartCoServer();
    } else {
        METLIBS_LOG_INFO("error when contacting tcp coserver: " << e);
    }
}

void CoClient::localError(QLocalSocket::LocalSocketError e)
{
    METLIBS_LOG_SCOPE();

    if (QLocalSocket::PeerClosedError == e) {
        METLIBS_LOG_INFO("client disconnect");
    } else {
        METLIBS_LOG_INFO("error when contacting local coserver: " << e);
    }
}

void CoClient::tryToStartCoServer()
{
    if (!mAttemptToStartServer)
        return;

    const QDateTime now = QDateTime::currentDateTime();
    if (now < mNextAttemptToStartServer)
        return;

    int delay_ms = 1000*SERVER_START_RETRY_DELAY + (qrand() & 0x7FF);
    mNextAttemptToStartServer = now.addMSecs(delay_ms);

    METLIBS_LOG_INFO("try starting coserver...");
    QStringList args = QStringList("-d"); ///< -d for dynamicMode
    args << "-u" << serverUrl.toString(QUrl::RemovePassword);

    if (not QProcess::startDetached(serverCommand, args)) {
        METLIBS_LOG_ERROR("could not run server '" << serverCommand << "'");
        Q_EMIT unableToConnect();
        return;
    }

    METLIBS_LOG_INFO("coserver probably started, will try to connect soon...");
    QTimer::singleShot(500, this, SLOT(connectToServer()));
}
