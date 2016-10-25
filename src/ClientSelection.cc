/**
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ClientSelection.h"

#include "CoClient.h"
#include "miMessage.h"

#include <QAction>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPixmap>
#include <QPushButton>
#include <QRegExpValidator>
#include <QVBoxLayout>
#include <QWidget>

#include "conn.xpm"
#include "disconn.xpm"
#include "unconn.xpm"

#define MILOGGER_CATEGORY "coserver.ClientSelection"
#include <qUtilities/miLoggingQt.h>

namespace { // anonymous
const QRegExp allowedSuffix("[A-Za-z0-9-]+");

QString buildClientName(const QString& prefix, const QString& suffix)
{
    if (suffix.isEmpty())
        return prefix;
    else
        return prefix + "-" + suffix;
}
} // anonymous namespace

// ========================================================================

ClientAction::ClientAction(int id, const QString& name, QObject* parent)
    : QAction(parent)
    , mId(id)
    , mName(name)
    , mConnected(false)
{
    updateText();
    setCheckable(true);
    setChecked(false);
    connect(this, SIGNAL(toggled(bool)), SLOT(onToggled(bool)));
}

void ClientAction::setId(int id)
{
    mId = id;
    updateText();
}

void ClientAction::setName(const QString& name)
{
    mName = name;
    updateText();
}

void ClientAction::setConnected(bool c)
{
    mConnected = c;
    updateText();
}

void ClientAction::updateText()
{
    QString t = mName;
    if (mId > 0) {
        t += " ";
        t += mConnected ? "[" : "(";
        t += QString::number(mId);
        t += mConnected ? "]" : ")";
    } else {
        t += " (?)";
    }
    setText(t);
}

void ClientAction::onToggled(bool checked)
{
    Q_EMIT toggled(this, checked);
}

// ########################################################################

ClientRenameDialog::ClientRenameDialog(const QString& prefix, const QString& suffix, QWidget* parent)
    : QDialog(parent)
    , mPrefix(prefix.trimmed())
{
    setModal(true);
    setWindowTitle(tr("Change name"));

    QLabel* label = new QLabel(tr("Enter new coserver name:"));
    mPrefixLabel = new QLabel("client", this);
    mSuffixEdit = new QLineEdit(this);
    mSuffixEdit->setMaxLength(128);
    mSuffixEdit->setValidator(new QRegExpValidator(allowedSuffix, this));
    const QString help = tr("Use A-Z, a-z, 0-9, -");
    mSuffixEdit->setPlaceholderText(help);
    mSuffixEdit->setToolTip(help);
    mButtons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, Qt::Horizontal, this);

    QGridLayout* layout = new QGridLayout();
    layout->addWidget(label, 0, 0, 1, 2);
    layout->addWidget(mPrefixLabel, 1, 0);
    layout->addWidget(mSuffixEdit, 1, 1);
    layout->addWidget(mButtons, 2, 0, 1, 2);
    setLayout(layout);

    connect(mSuffixEdit, SIGNAL(textChanged(const QString&)),
            SLOT(onTextChanged()));
    connect(mButtons, SIGNAL(accepted()),
            SLOT(accept()));
    connect(mButtons, SIGNAL(rejected()),
            SLOT(reject()));

    QString sfx = suffix.trimmed();
    if (!sfx.isEmpty() && !allowedSuffix.exactMatch(sfx))
        sfx = QString();
    mSuffixEdit->setText(sfx);
    onTextChanged(); // update mPrefixLabel
}

QString ClientRenameDialog::getClientNameSuffix() const
{
    return mSuffixEdit->text();
}

void ClientRenameDialog::onTextChanged()
{
    bool ok;
    const QString suffix = getClientNameSuffix();
    if (suffix.isEmpty()) {
        mPrefixLabel->setText(mPrefix);
        ok = true;
    } else {
        mPrefixLabel->setText(mPrefix + "-");
        ok = allowedSuffix.exactMatch(suffix);
    }
    mButtons->button(QDialogButtonBox::Ok)->setEnabled(ok);
}

// ########################################################################

ClientSelection::ClientSelection(const QString& clientType, QWidget* parent)
    : QObject(parent)
    , coclient(new CoClient(clientType, this))
{
    initialize();
}

ClientSelection::ClientSelection(CoClient* client, QWidget* parent)
    : QObject(parent)
    , coclient(client)
{
    initialize();
}

ClientAction* ClientSelection::createActionForClient(int id)
{
    return createActionForClient(id, coclient->getClientName(id));
}

ClientAction* ClientSelection::createActionForClient(int id, const QString& name)
{
    ClientAction* a = new ClientAction(id, name, this);
    QObject::connect(a, SIGNAL(toggled(ClientAction*, bool)),
            this, SLOT(onSendToClientToggled(ClientAction*, bool)));
    clientActions.push_back(a);
    return a;
}

void ClientSelection::initialize()
{
    mUpdatingClientActions = 0;

    // first create the actions for tool button and menu
    mActionRenameClient = new QAction(getClientName(), this);
    mActionRenameClient->setToolTip(tr("Rename this coserver client"));
    QObject::connect(mActionRenameClient, SIGNAL(triggered()),
            SLOT(onRenameRequested()));

    mActionSendToAll = new QAction(tr("All/None"), this);
    mActionSendToAll->setToolTip(tr("Send messages to all/none"));
    mActionSendToAll->setCheckable(false);
    QObject::connect(mActionSendToAll, SIGNAL(triggered()),
            SLOT(onSendToAllTriggered()));

    const ClientIds clients = coclient->getClientIds();
    for (ClientIds::const_iterator it = clients.begin(); it != clients.end(); ++it)
        createActionForClient(*it);

    // now prepare the tool button QAction
    mActionForToolButton = new QAction(this);
    mActionForToolButton->setIcon(QPixmap(unconn_xpm));
    mActionForToolButton->setToolTip(tr("Connect/disconnect to/from coserver"));
    QObject::connect(mActionForToolButton, SIGNAL(triggered()), SLOT(onConnectTriggered()));
    QMenu* toolMenu = new QMenu(0);
    toolMenu->addAction(mActionRenameClient);
    toolMenu->addSeparator();
    toolMenu->addAction(mActionSendToAll);
    toolMenu->addSeparator();
    for (clientActions_t::iterator it = clientActions.begin(); it != clientActions.end(); ++it)
        toolMenu->addAction(*it);
    mActionForToolButton->setMenu(toolMenu);

    // now prepare the menubar QMenu
    mActionForMenuBar = new QAction(tr("Coserver"), this);
    QMenu* menuMenu = new QMenu(0);
    mActionConnectFromMenu = menuMenu->addAction(tr("Connect"));
    QObject::connect(mActionConnectFromMenu, SIGNAL(triggered()),
            this, SLOT(onConnectTriggered()));
    menuMenu->addSeparator();
    menuMenu->addAction(mActionRenameClient);
    menuMenu->addSeparator();
    menuMenu->addAction(mActionSendToAll);
    menuMenu->addSeparator();
    for (clientActions_t::iterator it = clientActions.begin(); it != clientActions.end(); ++it)
        menuMenu->addAction(*it);
    mActionForMenuBar->setMenu(menuMenu);

    // connect signals from coclient
    QObject::connect(coclient, SIGNAL(clientChange(int, CoClient::ClientChange)),
            this, SLOT(onClientChange(int, CoClient::ClientChange)));
    QObject::connect(coclient, SIGNAL(connected()),
            this, SLOT(onConnected()));
    QObject::connect(coclient, SIGNAL(disconnected()),
            this, SLOT(onDisconnected()));
    QObject::connect(coclient, SIGNAL(unableToConnect()),
            this, SLOT(onUnableToConnect()));
    QObject::connect(coclient, SIGNAL(receivedMessage(int, const miQMessage&)),
            this, SLOT(onReceivedMessage(int, const miQMessage&)));
    QObject::connect(coclient, SIGNAL(receivedMessage(const miMessage&)),
            this, SLOT(onReceivedMessage(const miMessage&)));
    QObject::connect(coclient, SIGNAL(receivedId(int)),
            this, SLOT(onReceivedId(int)));

    if (coclient->isConnected())
        onConnected();
    else
        onDisconnected();
}

ClientSelection::~ClientSelection()
{
  METLIBS_LOG_SCOPE();
}

void ClientSelection::connect()
{
    METLIBS_LOG_SCOPE();
    if (coclient->notConnected()) {
        updateConnectActions(QPixmap(disconn_xpm), tr("Disconnect"), tr("Connecting...")); // we are not connected yet

        coclient->connectToServer();
    }
}

void ClientSelection::disconnect()
{
    METLIBS_LOG_SCOPE();
    if (coclient->isConnected()) {
        coclient->disconnectFromServer();

        updateConnectActions(QPixmap(disconn_xpm), tr("Connect"), tr("Disconnecting"));
    }
}

void ClientSelection::onConnected()
{
  METLIBS_LOG_SCOPE();

  const QUrl serverUrl = coclient->getConnectedServerUrl();
  const QString url = serverUrl.isValid() ? serverUrl.toString() : "?";
  updateConnectActions(QPixmap(conn_xpm), tr("Disconnect"), tr("Connected to %1").arg(url));

  Q_EMIT connected();
}

void ClientSelection::onDisconnected()
{
  METLIBS_LOG_SCOPE();

  updateRenameClientText();
  updateConnectActions(QPixmap(disconn_xpm), tr("Connect"), tr("Disconnected"));

  Q_EMIT disconnected();
}

void ClientSelection::onUnableToConnect()
{
  METLIBS_LOG_SCOPE();

  updateConnectActions(QPixmap(unconn_xpm), tr("Connect"), tr("Unable to connect"));
}

void ClientSelection::updateConnectActions(QIcon icon, QString text, QString toolTip)
{
  mActionForToolButton->setIcon(icon);
  mActionForToolButton->setToolTip(toolTip);

  mActionConnectFromMenu->setText(text);
  mActionConnectFromMenu->setIcon(icon);
  mActionConnectFromMenu->setToolTip(toolTip);
}

void ClientSelection::onConnectTriggered()
{
  METLIBS_LOG_SCOPE();
  if (coclient->notConnected())
      connect();
  else
      disconnect();
}

void ClientSelection::onSendToAllTriggered()
{
    METLIBS_LOG_SCOPE(LOGVAL(clientActions.size()));
    if (clientActions.empty())
        return;

    mUpdatingClientActions += 1;
    int delta = 0;
    for (clientActions_t::iterator it = clientActions.begin(); it != clientActions.end(); ++it)
        delta += (*it)->isChecked() ? +1 : -1;
    bool all = (delta < 0);
    METLIBS_LOG_DEBUG(LOGVAL(delta) << LOGVAL(all));
    for (clientActions_t::iterator it = clientActions.begin(); it != clientActions.end(); ++it)
        (*it)->setChecked(all);
    mUpdatingClientActions -= 1;
    sendUpdatedPeerList();
}

void ClientSelection::onSendToClientToggled(ClientAction* ca, bool checked)
{
    METLIBS_LOG_SCOPE();
    sendUpdatedPeerList();
}

void ClientSelection::sendUpdatedPeerList()
{
    METLIBS_LOG_SCOPE();
    if (mUpdatingClientActions > 0)
        return;

    QStringList peers;
    for (clientActions_t::iterator it = clientActions.begin(); it != clientActions.end(); ++it) {
        if ((*it)->isChecked())
            peers << (*it)->name();
    }
    coclient->setSelectedPeerNames(peers);
}

void ClientSelection::setSelectedClientNames(const QStringList& names)
{
    METLIBS_LOG_SCOPE();

    mUpdatingClientActions += 1;

    // TODO remove actions for clients that are not connected

    // add actions for clients
    for (int i=0; i<names.count(); ++i) {
        const QString& name = names.at(i);
        METLIBS_LOG_DEBUG(LOGVAL(name));
        bool found = false;
        for (clientActions_t::iterator it = clientActions.begin(); it != clientActions.end(); ++it) {
            const bool match = ((*it)->name() == name);
            METLIBS_LOG_DEBUG(LOGVAL((*it)->name()) << LOGVAL(match));
            (*it)->setChecked(match);
            found |= match;
        }
        if (!found) {
            METLIBS_LOG_DEBUG("new client action for '" << name << "'");
            ClientAction* ca = createActionForClient(-1, name);
            ca->setChecked(true);
            mActionForToolButton->menu()->addAction(ca);
            mActionForMenuBar->menu()->addAction(ca);
        }
    }
    mUpdatingClientActions -= 1;
    sendUpdatedPeerList();
}

QStringList ClientSelection::getSelectedClientNames()
{
    return coclient->getSelectedPeerNames();
}

ClientSelection::clientActions_t::iterator ClientSelection::findActionForClientId(int id)
{
    for (clientActions_t::iterator it = clientActions.begin(); it != clientActions.end(); ++it) {
        if ((*it)->id() == id)
            return it;
    }
    return clientActions.end();
}

ClientSelection::clientActions_t::iterator ClientSelection::findActionForClientName(const QString& name)
{
    for (clientActions_t::iterator it = clientActions.begin(); it != clientActions.end(); ++it) {
        if ((*it)->name() == name)
            return it;
    }
    return clientActions.end();
}

void ClientSelection::onClientChange(int id, CoClient::ClientChange what)
{
    METLIBS_LOG_SCOPE(LOGVAL(id) << LOGVAL(what));

    clientActions_t::iterator it = findActionForClientId(id);
    if (what == CoClient::CLIENT_REGISTERED) {
        if (it == clientActions.end()) {
            QString name = coclient->getClientName(id);
            it = findActionForClientName(name);
            if (it == clientActions.end()) {
                QAction* ca = createActionForClient(id, name);
                mActionForToolButton->menu()->addAction(ca);
                mActionForMenuBar->menu()->addAction(ca);
            } else {
                (*it)->setId(id);
            }
        } else {
            METLIBS_LOG_WARN("REGISTERED for known client " << id);
        }
    } else if (what == CoClient::CLIENT_NEW) {
        if (it == clientActions.end()) {
            METLIBS_LOG_WARN("NEW for unknown client " << id);
        } else {
            (*it)->setConnected(true);
        }
    } else if (what == CoClient::CLIENT_RENAME) {
        if (it != clientActions.end()) {
            (*it)->setName(coclient->getClientName(id));
        } else {
            METLIBS_LOG_WARN("RENAME for unknown client " << id);
        }
    } else if (what == CoClient::CLIENT_GONE) {
        if (it != clientActions.end()) {
            (*it)->setConnected(false);
        } else {
            METLIBS_LOG_WARN("GONE for unknown client " << id);
        }
    } else if (what == CoClient::CLIENT_UNREGISTERED) {
        if (it != clientActions.end()) {
            if ((*it)->isChecked()) {
                METLIBS_LOG_DEBUG("UNREGISTERED " << id << " => -1");
                (*it)->setId(-1);
            } else {
                METLIBS_LOG_DEBUG("UNREGISTERED " << id << ", removing actions");
                mActionForToolButton->menu()->removeAction(*it);
                mActionForMenuBar->menu()->removeAction(*it);
                clientActions.erase(it);
            }
        } else {
            METLIBS_LOG_WARN("UNREGISTERED for unknown client " << id);
        }
    }
}

void ClientSelection::onReceivedMessage(int fromId, const miQMessage& qmsg)
{
    Q_EMIT receivedMessage(fromId, qmsg);
}

void ClientSelection::onReceivedMessage(const miMessage& msg)
{
    Q_EMIT receivedMessage(msg);
}

void ClientSelection::sendMessage(const miQMessage &qmsg, const ClientIds& toIds)
{
    coclient->sendMessage(qmsg, toIds);
}

void ClientSelection::sendMessage(const miQMessage &qmsg, int toId)
{
    sendMessage(qmsg, clientId(toId));
}

void ClientSelection::onRenameRequested()
{
    METLIBS_LOG_SCOPE();

    const QString prefix = getClientNamePrefix();
    ClientRenameDialog rename(prefix, getClientNameSuffix(), static_cast<QWidget*>(parent()));
    if (rename.exec() == QDialog::Accepted) {
        setClientName(buildClientName(prefix, rename.getClientNameSuffix()));
    }
}

void ClientSelection::setClientName(const QString& name)
{
    METLIBS_LOG_SCOPE();
    if (name == getClientName())
        return;

    const QString prefix = getClientNamePrefix();
    if (!ClientSelection::isAllowedClientName(prefix, name)) {
        METLIBS_LOG_WARN("prefix change (from " << prefix << ") or illegal name (" << name << "), no rename");
        return;
    }

    coclient->setName(name);
    updateRenameClientText();
    Q_EMIT renamed(name);
}

void ClientSelection::onReceivedId(int id)
{
    updateRenameClientText();
}

void ClientSelection::updateRenameClientText()
{
    METLIBS_LOG_SCOPE();
    QString text = getClientName();
    int id = coclient->getClientId();
    if (id > 0) {
        text += " [";
        text += QString::number(id);
        text += "]";
    }

    mActionRenameClient->setText(text);
}

QString ClientSelection::getClientName(int id) const
{
    return coclient->getClientName(id);
}

QString ClientSelection::getClientName() const
{
    return coclient->getName();
}

QString ClientSelection::getClientNamePrefix() const
{
    return coclient->getClientType().toLower();
}

QString ClientSelection::getClientNameSuffix() const
{
    return ClientSelection::getClientNameSuffix(getClientNamePrefix(), getClientName());
}

// static
QString ClientSelection::getClientNameSuffix(const QString& prefix, const QString& name)
{
    if (name == prefix)
        return "";
    if (name.length() < prefix.length() + 2 || name.at(prefix.length()) != QChar('-')) {
        METLIBS_LOG_WARN("illegal name '" << name << "' for prefix '" << prefix << "'");
        return "";
    }
    return name.mid(prefix.length()+1);
}

// static
bool ClientSelection::isAllowedClientName(const QString& prefix, const QString& name)
{
    if (name == prefix)
        return true;
    if (name.length() < prefix.length() + 2)
        return false;
    if (name.at(prefix.length()) != QChar('-'))
        return false;
    const QString suffix = name.mid(prefix.length()+1);
    return allowedSuffix.exactMatch(suffix);
}
