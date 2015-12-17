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

#ifndef METLIBS_COSERVER_COCLIENT
#define METLIBS_COSERVER_COCLIENT 1

#include "miMessage.h"

#include <QDateTime>
#include <QLocalSocket>
#include <QString>
#include <QTcpSocket>
#include <QUrl>

#include <map>
#include <memory>

class miMessageIO;

class CoClient : public QObject
{
    Q_OBJECT

public:
    CoClient(const QString& clientType, const QString& host, quint16 port = 0);
    CoClient(const QString& clientType, const QUrl& url);
    ~CoClient();

    void setUserId(const QString& user);
    void setBroadcastClient()
        { setUserId(QString()); }

    const QString& getClientType() const
      { return clientType; }

    const QString& getName() const
      { return name; }

    void setName(const QString& name);

    int getClientId() const
        { return mId; }

    bool notConnected();
    bool isConnected();

    void setServerUrl(const QUrl& url)
        { serverUrl = url; }

    void setServerCommand(const QString& sc)
        { serverCommand = sc; }

    bool sendMessage(const miMessage &msg);
    bool sendMessage(const miQMessage &qmsg, const ClientIds& to = ClientIds());

    void setSelectedPeerNames(const QStringList& names);
    const QStringList& getSelectedPeerNames()
        { return mSelectedPeerNames; }

    ClientIds getClientIds() const;
    QString getClientType(int id) const;
    QString getClientName(int id) const;

    enum ClientChange { CLIENT_REGISTERED, CLIENT_NEW, CLIENT_RENAME, CLIENT_GONE, CLIENT_UNREGISTERED };

public Q_SLOTS:
    void connectToServer();
    void disconnectFromServer();

Q_SIGNALS:
    void receivedMessage(int from, const miQMessage&);
    void receivedMessage(const miMessage&);

    void clientChange(int clientId, CoClient::ClientChange change);

    void addressListChanged();
    void connected();
    void newClient(const std::string&);
    void newClient(const QString&);
    void unableToConnect();
    void disconnected();

private Q_SLOTS:
    //! Read new incoming message.
    void readNew();

    void connectionEstablished();

    void connectionClosed();

    void tcpError(QAbstractSocket::SocketError e);
    void localError(QLocalSocket::LocalSocketError e);

private:
    struct Client {
        QString type;
        QString name;
        bool connected;
        Client(const QString& t, const QString& n)
            : type(t), name(n), connected(false) { }
    };

    // map id -> Client(name, type, connected)
    typedef std::map<int, Client> clients_t;

private:
    void initialize(const QString& clientType);
    void createSocket();
    void tryToStartCoServer();

    void sendClientType();
    void sendMessageToServer(const miQMessage& qmsg);

    bool messageFromServer(const miQMessage& qmsg);
    void handleRegisteredClient(const miQMessage& qmsg);
    void handleUnregisteredClient(const miQMessage& qmsg);
    void handleNewClient(const miQMessage& qmsg);
    void handleRemoveClient(const miQMessage& qmsg);
    void handleRenameClient(const miQMessage& qmsg);

    void emitMessage(int fromId, const miQMessage& qmsg);

    void sendSetPeers();

private:
    QTcpSocket *tcpSocket;
    QLocalSocket *localSocket;
    std::auto_ptr<miMessageIO> io;

    int mId;
    QString clientType;
    QString userid;
    QString name;

    QStringList mSelectedPeerNames;

    clients_t clients;

    QString serverCommand;
    QUrl serverUrl;

    QDateTime mNextAttemptToStartServer;
};

#endif // METLIBS_COSERVER_COCLIENT
