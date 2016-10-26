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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "CoClient.h"

#include "miMessage.h"
#include "miMessageIO.h"
#include "QLetterCommands.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QProcess>
#include <QtCore/QSettings>
#include <QtCore/QStringList>
#include <QtCore/QTimer>

#include <QtNetwork/QHostInfo>

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

const char COSERVER_HOST[] = "COSERVER_HOST";
const char COSERVER_URLS[] = "COSERVER_URLS";
const QString SCHEME_CO4 = "co4";
const QString SCHEME_LOCAL = "local";
const QString LOCALHOST = "localhost";
const QString CLIENT_INI = "client.ini";
const QString KEY_SERVER_COMMAND = "client/server_command";
const QString KEY_ATTEMPT_START = "client/attempt_to_start_server";
const QString KEY_USER_ID = "client/user_id";
const int TIMEOUT_RECONNECT_MS = 1000;

QString joinArgs(const QStringList& args)
{
    QString joined;
    for (int i=0; i<args.count(); ++i) {
        if (i>0)
            joined += " ";
        joined += "\"";
        const QString& a = args.at(i);
        if (a.contains("\"")) {
            QString quoted = a;
            quoted.replace("\"", "\\\"");
            joined += quoted;
        } else {
            joined += a;
        }
        joined += "\"";
    }
    return joined;
}

QVariant value(QSettings& settings1, QSettings& settings2, const QString& key, const QVariant& fallback=QVariant())
{
    if (settings1.contains(key))
        return settings1.value(key).toString();
    else
        return settings2.value(key, fallback).toString();
}

QUrl serverUrlFromHostname(const QString& h="", quint16 port=0)
{
    QUrl serverUrl;
    serverUrl.setScheme(SCHEME_CO4);
    if (getenv(COSERVER_HOST) != NULL) {
        serverUrl.setHost(getenv(COSERVER_HOST));
    } else if (!h.isEmpty()) {
        serverUrl.setHost(h);
    } else {
        serverUrl.setHost(LOCALHOST);
    }
    if (port != 0)
        serverUrl.setPort(port);
    return serverUrl;
}

void addServerUrlsFromIniFile(CoClient::QUrlList& urls, const QString& path)
{
    METLIBS_LOG_SCOPE("trying to read servers from file '" << path << "'");
    QSettings settings(path, QSettings::IniFormat);
    for (int i=0; true; ++i) {
        const QString key = QString("servers/server_%1").arg(i);
        if (settings.contains(key)) {
            const QString u = settings.value(key).toString().trimmed();
            METLIBS_LOG_DEBUG("adding server url '" << u << "' from key '" << key << "'");
            urls << QUrl(u);
        } else if (i > 16) {
            break;
        }
    }
}

void addServerUrlsFromUrlString(CoClient::QUrlList& urls, const QString& coserver_urls, const QString& sep = " ")
{
    const QStringList coserver_url_list = coserver_urls.split(sep);
    for (int i=0; i<coserver_url_list.size(); ++i) {
        const QUrl u(coserver_url_list.at(i));
        if (u.isValid())
            urls << u;
    }
}

void addServerUrlsFromHostString(CoClient::QUrlList& urls, const QString& coserver_host)
{
    if (!coserver_host.isEmpty())
        urls << QUrl(coserver_host);
}

CoClient::QUrlList serverUrlsFromEnviroment()
{
    CoClient::QUrlList urls;
    addServerUrlsFromUrlString(urls, safe_getenv(COSERVER_URLS));
    addServerUrlsFromHostString(urls, safe_getenv(COSERVER_HOST));
    return urls;
}

QString userClientIni()
{
    QDir dot_coserver = QDir::home();
    dot_coserver.cd("." PACKAGE_NAME);
    return QFileInfo(dot_coserver, CLIENT_INI).filePath();
}

QString systemClientIni()
{
#if defined(PKGCONFDIR)
    QDir etc_coserver(PKGCONFDIR);
    return QFileInfo(etc_coserver, CLIENT_INI).filePath();
#else  // !PKGCONFDIR
    return QString();
#endif // !PKGCONFDIR
}

CoClient::QUrlList defaultServerUrls()
{
    CoClient::QUrlList urls = serverUrlsFromEnviroment();
    if (urls.isEmpty()) {
        addServerUrlsFromIniFile(urls, userClientIni());
    }
    if (urls.isEmpty()) {
        addServerUrlsFromIniFile(urls, systemClientIni());
    }
    if (urls.isEmpty()) {
        urls << serverUrlFromHostname();
    }
    return urls;
}

} // namespace anonymous

CoClient::CoClient(const QString& ct, QObject* parent)
    : QObject(parent)
{
    initialize(ct);

    setServerUrls(defaultServerUrls());
}

CoClient::CoClient(const QString& ct, const QString& h, quint16 port, QObject* parent)
    : QObject(parent)
{
    initialize(ct);

    QUrlList urls = serverUrlsFromEnviroment();
    if (urls.isEmpty())
        urls << serverUrlFromHostname(h, port);
    setServerUrls(urls);
}

CoClient::CoClient(const QString& ct, const QUrl& url, QObject* parent)
    : QObject(parent)
{
    initialize(ct);

    QUrlList urls = serverUrlsFromEnviroment();
    if (urls.isEmpty())
        urls << url;
    setServerUrls(urls);
}

CoClient::CoClient(const QString& ct, const QList<QUrl>& urls, QObject* parent)
    : QObject(parent)
{
    initialize(ct);

    const QUrlList envUrls = serverUrlsFromEnviroment();
    if (!envUrls.isEmpty())
        setServerUrls(envUrls);
    else
        setServerUrls(urls);
}

CoClient::~CoClient()
{
    METLIBS_LOG_SCOPE();
}

void CoClient::initialize(const QString& ct)
{
    METLIBS_LOG_SCOPE();

    tcpSocket = 0;
    localSocket = 0;

    mId = -1;
    name = clientType = ct;

    QSettings userIni(userClientIni(), QSettings::IniFormat);
    QSettings systemIni(systemClientIni(), QSettings::IniFormat);
    serverCommand = value(userIni, systemIni, KEY_SERVER_COMMAND, "coserver4").toString();
    mAttemptToStartServer = value(userIni, systemIni, KEY_ATTEMPT_START, true).toBool();

    userid = safe_getenv("COSERVER_USER").trimmed();
    if (userid.isEmpty())
        userid = userIni.value(KEY_USER_ID, "").toString();
    if (userid.isEmpty())
        userid = getUserId();

    METLIBS_LOG_DEBUG(LOGVAL(serverCommand) << LOGVAL(userid) << LOGVAL(mAttemptToStartServer));
}

void CoClient::setUserId(const QString& user)
{
    userid = user;
}

void CoClient::rewindServerList()
{
    METLIBS_LOG_SCOPE();
    serverIndex = 0;
    serverStarting = -1;
}

void CoClient::destroySocket()
{
    METLIBS_LOG_SCOPE();
    io.reset(0);
    if (tcpSocket) {
        tcpSocket->deleteLater();
        tcpSocket = 0;
    }
    if (localSocket) {
        localSocket->deleteLater();
        localSocket = 0;
    }
}

void CoClient::createSocket(const QUrl& serverUrl)
{
    METLIBS_LOG_SCOPE(LOGVAL(serverUrl.toString()));
    if (serverUrl.scheme() == SCHEME_CO4) {
        createTcpSocket(serverUrl);
    } else if (serverUrl.scheme() == SCHEME_LOCAL) {
        createLocalSocket(serverUrl);
    } else {
        METLIBS_LOG_ERROR("bad server url '" << serverUrl.toString(QUrl::RemovePassword) << "'");
        return;
    }
}

void CoClient::createTcpSocket(const QUrl& serverUrl)
{
    METLIBS_LOG_SCOPE();
    tcpSocket = new QTcpSocket(this);
    connect(tcpSocket, SIGNAL(error(QAbstractSocket::SocketError)),
            SLOT(tcpError(QAbstractSocket::SocketError)));
    connect(tcpSocket, SIGNAL(connected()),
            SLOT(connectionEstablished()));
    connect(tcpSocket, SIGNAL(disconnected()),
            SLOT(connectionClosed()));
    connect(tcpSocket, SIGNAL(readyRead()),
            SLOT(readNew()));

    QString host = serverUrl.host();
    if (host.isEmpty())
        host = LOCALHOST;
    const int port = serverUrl.port(qmstrings::port);
    METLIBS_LOG_INFO("connecting to host '" << host << "' port " << port);
    tcpSocket->connectToHost(host, port);
}


void CoClient::createLocalSocket(const QUrl& serverUrl)
{
    METLIBS_LOG_SCOPE();
    localSocket = new QLocalSocket(this);
    connect(localSocket, SIGNAL(error(QLocalSocket::LocalSocketError)),
            SLOT(localError(QLocalSocket::LocalSocketError)));
    connect(localSocket, SIGNAL(connected()),
            SLOT(connectionEstablished()));
    connect(localSocket, SIGNAL(disconnected()),
            SLOT(connectionClosed()));
    connect(localSocket, SIGNAL(readyRead()),
            SLOT(readNew()));

    const QString& path = serverUrl.path();
    METLIBS_LOG_INFO("connecting to server '" << path << "'");
    localSocket->connectToServer(path);
}

void CoClient::connectToServer()
{
    METLIBS_LOG_SCOPE();
    if (tcpSocket || localSocket) {
        METLIBS_LOG_DEBUG("already connected / connecting");
        return;
    }
    rewindServerList();
    connectServer();
}

void CoClient::connectServer()
{
    METLIBS_LOG_SCOPE();
    destroySocket();
    if (serverIndex >= serverUrls.size()) {
        Q_EMIT unableToConnect();
        return;
    }
    const QUrl& serverUrl = serverUrls.at(serverIndex);
    createSocket(serverUrl);
}

void CoClient::disconnectFromServer()
{
    METLIBS_LOG_SCOPE();
    if (tcpSocket)
        tcpSocket->disconnectFromHost();
    else if (localSocket)
        localSocket->disconnectFromServer();
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

void CoClient::setServerUrls(const QUrlList& urls)
{
    const bool wasConnected = isConnected() || !notConnected();
    if (wasConnected)
        disconnectFromServer();

    rewindServerList();
    serverUrls.clear();
    for (int i=0; i<urls.size(); ++i) {
        QUrl u = urls.at(i);
        bool accept = !u.hasFragment() && !u.hasQuery();
        if (u.scheme().isEmpty()) {
            const QString& p = u.path(), h = u.host();
            if (h.isEmpty() && !p.isEmpty() && !p.contains("/")) {
                u.setScheme(SCHEME_CO4);
                u.setHost(p);
                u.setPath("");
            } else if (!p.isEmpty()) {
                u.setScheme(SCHEME_LOCAL);
            } else {
                accept = false;
            }
        }
        if (u.scheme() == SCHEME_LOCAL && u.port() != -1)
            accept = false;
        if (accept) {
            METLIBS_LOG_DEBUG("server url '" << u.toString() << "'");
            serverUrls << u;
        } else {
            METLIBS_LOG_WARN("invalid url '" << urls.at(i).toString() << "' skipped");
        }
    }

    if (wasConnected)
        tryReconnectAfterTimeout();
}

QUrl CoClient::getConnectedServerUrl()
{
    if (serverIndex >= 0 && serverIndex < serverUrls.size() && isConnected())
        return serverUrls.at(serverIndex);
    else
        return QUrl();
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
    destroySocket();
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
    METLIBS_LOG_INFO("start talking to '" << getConnectedServerUrl().toString() << "'");

    QIODevice* device = tcpSocket ? static_cast<QIODevice*>(tcpSocket) : static_cast<QIODevice*>(localSocket);
    if (!device)
        return;
    io.reset(new miMessageIO(device, false));

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

    METLIBS_LOG_DEBUG(LOGVAL(mId) << " protocolVersion=" << io->protocolVersion());

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
        METLIBS_LOG_INFO("could not connect to tcp coserver");
        tryToStartOrConnectNext();
    } else if (QAbstractSocket::RemoteHostClosedError == e) {
        METLIBS_LOG_INFO("connection to tcp coserver closed unexpectedly");
        tryReconnectAfterTimeout();
    } else {
        METLIBS_LOG_INFO("error when contacting tcp coserver: " << e);
        tryConnectNextServer();
    }
}

void CoClient::localError(QLocalSocket::LocalSocketError e)
{
    METLIBS_LOG_SCOPE();
    if (QLocalSocket::ConnectionRefusedError == e || QLocalSocket::ServerNotFoundError == e) {
        METLIBS_LOG_INFO("could not connect to local coserver");
        tryToStartOrConnectNext();
    } else if (QLocalSocket::PeerClosedError == e) {
        METLIBS_LOG_INFO("connection to local coserver closed unexpectedly");
        tryReconnectAfterTimeout();
    } else {
        METLIBS_LOG_INFO("error when contacting local coserver: " << e);
        tryConnectNextServer();
    }
}

void CoClient::tryReconnectAfterTimeout()
{
    METLIBS_LOG_SCOPE();
    QTimer::singleShot(TIMEOUT_RECONNECT_MS, this, SLOT(connectServer()));
}

void CoClient::tryConnectNextServer()
{
    METLIBS_LOG_SCOPE();
    serverIndex += 1;
    serverStarting = -1;
    connectServer();
}

void CoClient::tryToStartOrConnectNext()
{
    if (!tryToStartCoServer())
        tryConnectNextServer();
}

bool CoClient::tryToStartCoServer()
{
    METLIBS_LOG_SCOPE();
    if (!mAttemptToStartServer)
        return false;
    if (serverStarting != -1)
        return false;
    if (serverIndex < 0 || serverIndex >= serverUrls.size())
        return false;

    QUrl serverUrl = serverUrls.at(serverIndex);
    if (!isLocalServer(serverUrl)) {
        METLIBS_LOG_DEBUG("server URL " << serverUrl.toString() << " seems to specify a remote server,"
                " not trying to start");
        return false;
    }
#if 1
    // coserver4 < 5.1.2 does not use the default port if not specified
    if (serverUrl.port() == -1)
        serverUrl.setPort(qmstrings::port);
#endif

    // coserver4 >= 5.1.2 only listens on the specified address if host is not empty,
    // while coserver4 < 5.1.2 ignores the address;
    // we want coserver4 to listen on any address and clear the host name
    serverUrl.setHost("");

    serverStarting = serverIndex;

    METLIBS_LOG_INFO("try starting local coserver...");
    QStringList args = QStringList("-d"); ///< -d for dynamicMode
    args << "-u" << serverUrl.toString(QUrl::RemovePassword);

    METLIBS_LOG_DEBUG("starting command=\"" << serverCommand
            << "\" args=[" << joinArgs(args) << "]");
    if (QProcess::startDetached(serverCommand, args)) {
        tryReconnectAfterTimeout();
        return true;
    } else {
        METLIBS_LOG_ERROR("could not run server command=\"" << serverCommand
                << "\" args=[" << joinArgs(args) << "]");
        return false;
    }
}

bool CoClient::isLocalServer(const QUrl& url)
{
    if (url.scheme() == SCHEME_LOCAL)
        return true;
    if (url.scheme() == SCHEME_CO4) {
        const QString& host = url.host();
        if (host.isEmpty() || host == "127.0.0.1" || host == "[::1]")
            return true;
        if (host == LOCALHOST || host == QHostInfo::localHostName())
            return true;
    }
    return false;
}
