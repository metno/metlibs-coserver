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

// Qt-includes

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ClientButton.h"
#include "CoClient.h"
#include "miMessage.h"

#include <QPixmap>
#include <qtooltip.h>

#include "conn.xpm"
#include "disconn.xpm"
#include "unconn.xpm"

#ifdef __WIN32__
#define METLIBS_LOG_SCOPE(x) /* emtpy */
#define METLIBS_LOG_ERROR(x) /* emtpy */
#define METLIBS_LOG_INFO(x)  /* emtpy */
#define METLIBS_LOG_DEBUG(x) /* emtpy */
#else
#define MILOGGER_CATEGORY "coserver.ClientButton"
#include <miLogger/miLogging.h>
#endif /* __WIN32__ */

using namespace std;

ClientButton::ClientButton(const QString & name, const QString & server, QWidget * parent)
  : QPushButton(name, parent)
  , coclient(new CoClient(name.toLatin1(), "localhost", server.toLatin1()))
  , isMyClient(true)
  , uselabel(false)
{
  initialize();
}

ClientButton::ClientButton(CoClient* client, QWidget * parent)
  : QPushButton(QString::fromStdString(client->getClientType()), parent)
  , coclient(client)
  , isMyClient(false)
  , uselabel(false)
{
  initialize();
}

void ClientButton::initialize()
{
  connect(this, SIGNAL(clicked()), SLOT(connectToServer()));
  
  connect(coclient, SIGNAL(newClient(const std::string&)), SLOT(setLabel(const std::string&)));
  connect(coclient, SIGNAL(connected()), SLOT(connected()));
  connect(coclient, SIGNAL(unableToConnect()), SLOT(unableToConnect()));
  connect(coclient, SIGNAL(disconnected()), SLOT(disconnected()));
  connect(coclient, SIGNAL(receivedMessage(const miMessage&)), SIGNAL(receivedMessage(const miMessage&)));
  connect(coclient, SIGNAL(addressListChanged()), SIGNAL(addressListChanged()));

  setText("");
  if (coclient->notConnected())
    disconnected();
  else
    connected();
}

ClientButton::~ClientButton()
{
  METLIBS_LOG_SCOPE();
  if (isMyClient)
    delete coclient;
}

void ClientButton::connectToServer()
{
  METLIBS_LOG_SCOPE();
  
  if (coclient->notConnected()) {
    METLIBS_LOG_DEBUG("not connected -> connecting");
    setIcon(QPixmap(disconn_xpm));
    setToolTip("Connecting...");
    coclient->connectToServer();
  } else {
    METLIBS_LOG_DEBUG("connected -> disconnecting");
    coclient->disconnectFromServer();
    setToolTip("Disconnected");
    setLabel("noClient");
    /*emit*/ connectionClosed();
  }
}

void ClientButton::connected()
{
  METLIBS_LOG_SCOPE();
  setIcon(QPixmap(conn_xpm));
  setToolTip("Connected");
  /*emit*/ connectedToServer();
}

void ClientButton::disconnected()
{
  METLIBS_LOG_SCOPE();
  setIcon(QPixmap(unconn_xpm));
  setToolTip("Disconnected from CoServer");
  /*emit*/ connectionClosed();
}

void ClientButton::unableToConnect()
{
  METLIBS_LOG_SCOPE();
  setLabel("portBusy");
  setToolTip("Unable to connect");
}

void ClientButton::setLabel(const std::string& name)
{
  if (name == "noClient") {
    setIcon(QPixmap(disconn_xpm));
    setText("");
  } else if (name == "myself") {
    setIcon(QPixmap(conn_xpm));
    setText("");
  } else if (name == "portBusy") {
    setIcon(QPixmap(unconn_xpm));
    setText("");
  } else if (uselabel ) {
    setIcon(QPixmap(conn_xpm));
    
    /// not useful anymore.. needs refactoring
    setText("");
    //setText(name.c_str());
  }
}

void ClientButton::sendMessage(miMessage& msg)
{
  coclient->sendMessage(msg);
}

const std::string& ClientButton::getClientName(int id)
{
  return coclient->getClientName(id);
}

bool ClientButton::clientTypeExist(const std::string& type)
{
  return coclient->clientTypeExist(type);
}

void ClientButton::useLabel(bool label)
{
  uselabel = label;
}
