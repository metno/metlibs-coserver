// -*- c++ -*-
/**
 * coserver client file
 *
 * Copyright (C) 2015 met.no
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

#ifndef METLIBS_COSERVER_CLIENTSELECTION_H
#define METLIBS_COSERVER_CLIENTSELECTION_H 1

#include "CoClient.h"

#include <QAction>
#include <QDialog>
#include <QObject>
#include <QRegExp>

#include <memory>

class miMessage;
class miQMessage;

class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QWidget;

class ClientAction : public QAction
{
    Q_OBJECT

public:
    ClientAction(int clientId, const QString& name, QObject* parent=0);

    void setId(int id);
    int id() const
        { return mId; }

    void setName(const QString& name);
    const QString& name() const
        { return mName; }

    void setConnected(bool c);
    bool isConnected() const
        { return mConnected; }

Q_SIGNALS:
    void toggled(ClientAction* a, bool checked);

private Q_SLOTS:
    void onToggled(bool checked);

private:
    void updateText();

private:
    int mId;
    QString mName;
    bool mConnected;
};

// ========================================================================

class ClientRenameDialog : public QDialog
{
    Q_OBJECT

public:
    ClientRenameDialog(const QString& prefix, const QString& suffix, QWidget* parent);

    QString getClientNameSuffix() const;

private Q_SLOTS:
    void onTextChanged();

private:
    QString mPrefix;
    QLabel* mPrefixLabel;
    QLineEdit* mSuffixEdit;
    QDialogButtonBox* mButtons;
};

// ========================================================================

class ClientSelection : public QObject
{
    Q_OBJECT

public:
    ClientSelection(const QString& clientType, QWidget* parent=0);
    ClientSelection(CoClient* client, QWidget* parent=0);

    ~ClientSelection();

    QAction* getToolButtonAction()
        { return mActionForToolButton; }

    QAction* getMenuBarAction()
        { return mActionForMenuBar; }

    CoClient* client()
        { return coclient; }

    /**
     * Sends a message.
     * @param msg The message to be sent
     */
    void sendMessage(const miQMessage &qmsg, const ClientIds& toIds = ClientIds());
    void sendMessage(const miQMessage &qmsg, int toId);

    /** Get this client's name. */
    QString getClientName() const;

    /** Get this client's name suffix. */
    QString getClientNameSuffix() const;

    /** Get the name of the client with the given id. */
    QString getClientName(int id) const;

    void setSelectedClientNames(const QStringList& names);
    QStringList getSelectedClientNames();

    /** Extract suffix from name given the prefix. */
    static QString getClientNameSuffix(const QString& prefix, const QString& name);

    /** Check if prefix and name are valid. */
    static bool isAllowedClientName(const QString& prefix, const QString& name);

public Q_SLOTS:
    void connect();
    void disconnect();

    /** Set this client's name, but only if it is valid. */
    void setClientName(const QString& name);

Q_SIGNALS:
    void receivedMessage(int fromId, const miQMessage&);
    void receivedMessage(const miMessage&);

    void connected();
    void disconnected();
    void renamed(const QString& name);

private Q_SLOTS:
    void onConnected();
    void onDisconnected();
    void onUnableToConnect();
    void onReceivedId(int id);
    void onClientChange(int id, CoClient::ClientChange what);
    void onReceivedMessage(int fromId, const miQMessage& qmsg);
    void onReceivedMessage(const miMessage&);

    void onConnectTriggered();
    void onRenameRequested();
    void onSendToAllTriggered();
    void onSendToClientToggled(ClientAction* ca, bool checked);

private:
    typedef std::vector<ClientAction*> clientActions_t;

private:
    void initialize();
    QString getClientNamePrefix() const;

    void sendUpdatedPeerList();

    ClientAction* createActionForClient(int id);
    ClientAction* createActionForClient(int id, const QString& name);

    clientActions_t::iterator findActionForClientId(int id);
    clientActions_t::iterator findActionForClientName(const QString& name);

    void updateConnectActions(QIcon icon, QString text, QString toolTip);
    void updateRenameClientText();

private:
    CoClient* coclient;

    QAction* mActionForToolButton;
    QAction* mActionForMenuBar;
    QAction* mActionRenameClient;
    QAction* mActionConnectFromMenu;
    QAction* mActionSendToAll;

    clientActions_t clientActions;

    int mUpdatingClientActions; //! protect against too many sendSetPeers
};

#endif // METLIBS_COSERVER_CLIENTSELECTION_H
