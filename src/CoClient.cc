/**
 * coclient - coserver client file
 * @author Martin Lilleeng S�tra <martinls@met.no>
 *
 * Copyright (C) 2013 met.no
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

// TODO: Add support for multiple servers active on different ports (on the same node)
// TODO: Add support for multiple clients per server

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif  /* HAVE_CONFIG_H */

#include "CoClient.h"
#include "QLetterCommands.h"

#ifdef __WIN32__
// GetUserName()
#include <windows.h>
#include <lmcons.h>
#else
// uid_t and getpwnam()
#include <sys/types.h>
#include <pwd.h>
#endif

// Qt-includes
//#include <QtNetwork>
#include <QtCore/QProcess>
#include <QtCore/QStringList>
#include <QtCore/QTimer>
#include <QtNetwork/QTcpSocket>

#ifdef HAVE_LOG4CXX   /* Defined in config.h */
#include <log4cxx/logger.h>
static log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("coclient.CoClient");
#else  /* ! HAVE_LOG4CXX */
#include <miLogger/logger.h>
#endif /* HAVE_LOG4CXX */

#include <boost/algorithm/string.hpp>

#include <fstream>
#include <sstream>
#include <unistd.h>

//#define _DEBUG
#ifdef _DEBUG
#include <iostream>
#define DEBUG_MSG(x) std::cerr << x << std::endl
#else
#define DEBUG_MSG(x)
#endif

namespace /* anonymous */ {
std::string safe_getenv(const char* var)
{
    const char* v = getenv(var);
    if (not v)
        return std::string();
    return v;
}

const int SERVER_START_RETRY_DELAY = 2;
} // namespace anonymous

CoClient::CoClient(const char *name, const char *h, const char *sc, quint16 p)
    : tcpSocket(new QTcpSocket(this))
    , clientType(name)
    , serverCommand(QString::fromStdString(sc))
    , host(h)
    , blockSize(0)
    , port(p)
    , mNextAttemptToStartServer(QDateTime::currentDateTime().addSecs(-30))
{
#if 0
    if (port == 0)
        port = readPortFromFile_Services();
#else
    if (port == 0)
        port = readPortFromFile();
#endif
    if (port == 0)
        port = qmstrings::port;

    if (getenv("COSERVER_HOST") != NULL) {
        host = getenv("COSERVER_HOST");
    }

    if (getenv("USER") != NULL) {
        userid = getenv("USER");
    } else {
        userid = safe_getenv("USERNAME");
    }
    if (getenv("HOSTNAME") != NULL) {
        userid += "@" + safe_getenv("HOSTNAME");
    } else {
        userid += "@" + safe_getenv("COMPUTERNAME");
    }
#ifdef __WIN32__
    {
        DWORD size = UNLEN + 1;
        CHAR name[size];
        if (!GetUserNameA(name, &size)) {
            std::cerr << "GetUserNameA() failed" << std::endl;
            userid = "UnknownWin32User";
        } else {
            userid = name;
        }
    }
#else
    {
        uid_t uid = getuid();
        struct passwd *pw = getpwuid(uid);
        if (pw) {
            userid = pw->pw_name;
        } else {
            std::stringstream ss;
            ss << "UnknownUser" << uid;
            userid = ss.str();
        }
    }
#endif
    
    // connected to coserver
    connect(tcpSocket, SIGNAL(connected()), this, SLOT(connectionEstablished()));
    // anything new to read?
    connect(tcpSocket, SIGNAL(readyRead()), this, SLOT(readNew()));
    // connection to server closed
    connect(tcpSocket, SIGNAL(disconnected()), this, SLOT(connectionClosed()));
    // socket error
    connect(tcpSocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(socketError(QAbstractSocket::SocketError)));
    
#ifdef _DEBUG
    connect(tcpSocket, SIGNAL(bytesWritten(qint64)), this, SLOT(printBytesWritten(qint64)));
#endif
}

CoClient::~CoClient()
{
    DEBUG_MSG("CoClient::~CoClient()");
}

void CoClient::setBroadcastClient()
{
    userid = "";
}

int CoClient::readPortFromFile()
{
    DEBUG_MSG("CoClient::readPortFromFile()");
    
    const std::string homePath = safe_getenv("HOME");
    DEBUG_MSG("homePath: " << homePath);
    
    std::ifstream pfile((homePath + "/.coserver.port").c_str());
    if (not pfile.is_open()) {
        DEBUG_MSG("Could not read coserver.port from file");
        return 0;
    }

    int filePort = 0;
    pfile >> filePort;
    if (not pfile) {
        DEBUG_MSG("Could not read port number from file");
        return 0;
    }
    
    DEBUG_MSG("Port: " << filePort << " read from file.");
    return filePort;
}

// etc/services: diana-<username>		<port>/tcp		# comment
int CoClient::readPortFromFile_Services()
{
    DEBUG_MSG("CoClient::readPortFromFile_Services()");
    
    const char filename[] = "/etc/services";
    std::ifstream file(filename);
    if (not file.is_open()) {
        DEBUG_MSG("Could not open file '" << filename << "'");
        return 0;
    }
    
    const char* user = getenv("USER");
    if (not user) {
        DEBUG_MSG("Environment variable 'USER' not set.");
        return 0;
    }

    int filePort = 0;
    std::string line;
    while(std::getline(file, line)) {
        if (line.empty() or line.at(0) == '#' or line.at(0) == '\n' or line.at(0) == ' ')
            continue;
        
        boost::replace_all(line, "\t", " ");
        boost::replace_all(line, "/", " ");
        std::vector<std::string> tokens;
        boost::split(tokens, line, boost::is_any_of(" "));
        if (tokens.size() < 2 or tokens[1].empty())
            continue;
        std::vector<std::string> tokens2;
        boost::split(tokens2, tokens[1], boost::is_any_of("-"));
        if (tokens2.size() != 2 or tokens2[1].empty())
            continue;
        if (tokens2[1] != user)
            continue;
        filePort = std::atoi(tokens2[1].c_str());
    }
    LOG4CXX_DEBUG(logger, "Port: " << filePort << " read from '" << filename << "'");
    return filePort;
}

void CoClient::printBytesWritten(qint64 written)
{
    (void)written;
    LOG4CXX_DEBUG(logger, "Written " << written << " bytes to socket");
}

bool CoClient::notConnected()
{
    return (QAbstractSocket::UnconnectedState == tcpSocket->state());
}

void CoClient::connectionClosed()
{
    LOG4CXX_DEBUG(logger, "CoClient disconnected from server");
    /*emit*/ disconnected();
}

void CoClient::connectToServer()
{
    if (port > 0) {
        LOG4CXX_INFO(logger, "CoClient::connectToServer(): connecting to port " << port);
        tcpSocket->connectToHost(host, port);
    } else {
        LOG4CXX_ERROR(logger, "CoClient::connectToServer(): bad port " << port);
    }
}

void CoClient::disconnectFromServer()
{
    tcpSocket->disconnectFromHost();
}

void CoClient::readNew()
{
    QDataStream in(tcpSocket);
    in.setVersion(QDataStream::Qt_4_0);
    
    // make sure that the whole message has been written
    if (blockSize == 0) {
        if (tcpSocket->bytesAvailable() < (int)sizeof(quint16))
            return;
        
        in >> blockSize;
    }

    if (tcpSocket->bytesAvailable() < blockSize)
        return;
    
    // read incoming message
    miMessage msg;
    QString tmpcommand, tmpdescription, tmpcommondesc, tmpcommon,
        tmpclientType, tmpco, tmpdata;
    int size = 0;
    
    in >> msg.to;
    in >> msg.from;
    in >> tmpcommand;
    msg.command = tmpcommand.toStdString();
    in >> tmpdescription;
    msg.description = tmpdescription.toStdString();
    in >> tmpcommondesc;
    msg.commondesc = tmpcommondesc.toStdString();
    in >> tmpcommon;
    msg.common = tmpcommon.toStdString();
    in >> tmpclientType;
    msg.clientType = tmpclientType.toStdString();
    in >> tmpco;
    msg.co = tmpco.toStdString();
    in >> size; // NOT A FIELD IN MIMESSAGE (METADATA ONLY)
    for (int i = 0; i < size; i++) {
        in >> tmpdata;
        msg.data.push_back(tmpdata.toStdString());
    }
    
    DEBUG_MSG("miMessage in CoClient::readNew() (RECV)" << std::endl << msg.content());
   
    // if origin is the server itself, then it will always be a request to
    // change (add/delete) entries in the list of clients
    if (msg.from == 0)
        editClients(msg);
    
    /*emit*/ receivedMessage(msg);

    blockSize = 0;
    
    // more unread messages on socket?
    if (tcpSocket->bytesAvailable() > 0)
        readNew();
}

void CoClient::editClients(const miMessage& msg)
{
    std::vector<std::string> common;
    boost::split(common, msg.common, boost::is_any_of(":"));
    if (common.size() < 2)
        return;
    
    const int id = atoi(common[0].c_str());
    const std::string& type = common[1];

    if (msg.command == qmstrings::newclient) {
        clients.erase(id);
        clients[id] = type;
        LOG4CXX_INFO(logger, "Added new client of type " << type << " and id " << id << " to the list of clients");
        
        /*emit*/ newClient(type);
        /*emit*/ addressListChanged();
    } else if (msg.command == qmstrings::removeclient) {
        clients.erase(id);
        LOG4CXX_INFO(logger, "Removed client of type " << type << " and id " << id << " from the list of clients");
        
        /*emit*/ newClient("myself");
        /*emit*/ addressListChanged();
    } else {
        LOG4CXX_ERROR(logger, "Error editing client list");
    }
}

void CoClient::connectionEstablished()
{
    LOG4CXX_INFO(logger, "CoClient connected to server");
    
    miMessage msg(0, 0, "SETTYPE", "INTERNAL");
    msg.data.push_back(clientType);
    msg.commondesc = "userId";
    msg.common = userid;
    sendMessage(msg);
    /*emit*/ connected();
}

bool CoClient::sendMessage(miMessage &msg)
{
    bool hasReceiver = false;
    
    if (tcpSocket->state() == QTcpSocket::ConnectedState) {
        std::map<int, std::string>::iterator it;
        if(msg.to != 0 && msg.to != -1) { ///< if the message is for the server or is a broadcast, do not do anything
            
            for(it = clients.begin(); it != clients.end(); ++it) {
                if((int)it->first == msg.to) {
                    hasReceiver = true;
                    break;
                }
            }
        }
        
        /// if msg does not contain a valid receiver address, broadcast it to all clients
        if(not hasReceiver)
            msg.to = -1;
        
        DEBUG_MSG("miMessage in CoClient::sendMessage() (SEND)" << std::endl << msg.content());

        QByteArray block;
        QDataStream out(&block, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_4_0);

        // send message to server
        out << (quint32)0;
        
        out << msg.to;
        // msg.from is set by server-side socket
        out << QString::fromStdString(msg.command);
        out << QString::fromStdString(msg.description);
        out << QString::fromStdString(msg.commondesc);
        out << QString::fromStdString(msg.common);
        out << QString::fromStdString(msg.clientType);
        out << QString::fromStdString(msg.co);
        out << quint32(msg.data.size()); // NOT A FIELD IN MIMESSAGE (TEMP ONLY)
        DEBUG_MSG("Size of data in last sent msg: " << msg.data.size());
        for (unsigned int i = 0; i < msg.data.size(); i++) {
            out << QString::fromStdString(msg.data[i]);
        }

        out.device()->seek(0);
        out << (quint32)(block.size() - sizeof(quint32));
        
        tcpSocket->write(block);
        tcpSocket->waitForBytesWritten(250);
        return true;
    } else {
        LOG4CXX_ERROR(logger, "Error sending message");
        return false;
    }
}

const std::string& CoClient::getClientName(int id)
{
    std::map<int,std::string>::const_iterator it = clients.find(id);
    if (it == clients.end()) {
        static const std::string empty;
        return empty;
    }
    return it->second;
}

bool CoClient::clientTypeExist(const std::string& type)
{
    std::map<int, std::string>::iterator it;
    for (it = clients.begin(); it != clients.end(); it++)
        if ((*it).second == type)
            return true;
    return false;
}

void CoClient::socketError(QAbstractSocket::SocketError e)
{
    if (QAbstractSocket::ConnectionRefusedError == e) {
        LOG4CXX_INFO(logger, "CoClient: could not connect to coserver, trying to start");
        tryToStartCoServer();
    } else if (QAbstractSocket::RemoteHostClosedError == e) {
        LOG4CXX_INFO(logger, "CoClient: connection to coserver closed unexpectedly, trying to restart");
        tryToStartCoServer();
    } else {
        LOG4CXX_INFO(logger, "CoClient: error when contacting coserver: " << e);
    }
}

void CoClient::tryToStartCoServer()
{
    const QDateTime now = QDateTime::currentDateTime();
    if (now < mNextAttemptToStartServer)
        return;

    mNextAttemptToStartServer = now.addSecs(SERVER_START_RETRY_DELAY);

    LOG4CXX_INFO(logger, "CoClient: starting coserver...");
    QStringList args = QStringList("-d"); ///< -d for dynamicMode
    if (port > 0)
        args << "-p" << QString::number(port);
    
    if (not QProcess::startDetached(serverCommand, args)) {
        LOG4CXX_ERROR(logger, "CoClient: could not start server. Make sure the path of coserver4"
                      " is correctly set in the setup of your client, and try again.");
        /*emit*/ unableToConnect();
        return;
    }
    
    LOG4CXX_INFO(logger, "CoClient: coserver probably started, will try to connect soon...");
    QTimer::singleShot(500, this, SLOT(connectToServer()));
}
