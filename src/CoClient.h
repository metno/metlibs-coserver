// -*- c++ -*-
/** @mainpage qUtilities - coserver client file
 * @author Martin Lilleeng Sætra <martinls@met.no>
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

#ifndef _COCLIENT
#define _COCLIENT

#include "miMessage.h"

#include <QAbstractSocket>
#include <QDateTime>

#include <map>

QT_BEGIN_NAMESPACE
class QTcpSocket;
QT_END_NAMESPACE

class CoClient : public QObject
{ Q_OBJECT
public:
    CoClient(const char* clientType, const char* host, const char* serverCommand, quint16 port = 0);
    ~CoClient();

    /**
     * Send message to all CoServer client
     */
    void setBroadcastClient();

    /**
     * Disconnects from the server.
     */
    void disconnectFromServer(void);

    /**
     * Sends a message to the server.
     * @param msg The message
     * @return Returns true upon successful sending, false otherwise
     */
    bool sendMessage(miMessage &msg);

    bool notConnected(void);
    const std::string& getClientName(int);
    bool clientTypeExist(const std::string &type);

public Q_SLOTS:
    /**
     * Connects to the server.
     */
    void connectToServer(void);

private:
    QTcpSocket *tcpSocket;

    std::string clientType;
    QString serverCommand;
    QString host;

    quint32 blockSize;
    quint16 port;
    std::string userid;

    QDateTime mNextAttemptToStartServer;

    std::map<int, std::string> clients;

    /**
     * Adds or removes entries in the list of clients as clients
     * connects and disconnects.
     * @param msg Message containing client info
     */
    void editClients(const miMessage& msg);

    /**
     * Read port from file. File has to be located in ~/.diana/diana.port and only contain the port number.
     */
    int readPortFromFile();

    /**
     * Parses /etc/services and extracts portnumber based on username. (env variable USER)
     * Format in /etc/services
     * diana-<username>    <port>/tcp    # comment
     */
    int readPortFromFile_Services();

Q_SIGNALS:
    void receivedMessage(const miMessage&);
    void addressListChanged();
    void connected();
    void newClient(const std::string&);
    void unableToConnect();
    void disconnected();

private Q_SLOTS:
    /**
     * Read new incoming message.
     */
    void readNew();

    void connectionEstablished();

    /**
     * Called when disconnected from server.
     */
    void connectionClosed();

    /**
     * Starts coserver if not already running.
     * @param e Error type
     */
    void socketError(QAbstractSocket::SocketError e);

    /**
     * Debugging function.
     * Only used if _DEBUG is defined.
     * @param written The number of bytes written to socket
     */
    void printBytesWritten(qint64 written);

private:
    void tryToStartCoServer();
};

#endif
