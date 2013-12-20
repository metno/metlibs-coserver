// -*- c++ -*-
/** @file ClientButton.h
 * @author Martin Lilleeng S�tra <martinls@met.no>
 *
 * qUtilities - coserver client file
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

#ifndef _CLIENTBUTTON
#define _CLIENTBUTTON

#include <qpushbutton.h>
#include <memory>

class CoClient;
class miMessage;

class ClientButton : public QPushButton
{ Q_OBJECT
public:
    /**
     * This button starts a new client and connects to the running coserver.
     * It will start a new coserver if one is not already running.
     * Changes in interface:\n
     * -Constructor have changed (dropped 4. parameter)\n
     * -receivedLetter(miMessage &) --> receivedMessage(miMessage &) (SLOT)\n
     * -connectClient(QString) --> clientTypeExist(std::string)
     * @param text Text to display with button
     * @param server Which server to use
     * @param parent Parent widget
     */
    ClientButton(const QString & text, const QString & server, QWidget * parent);
    ~ClientButton();

    /**
     * Sends a message.
     * @param msg The message to be sent
     */
    void sendMessage(miMessage &msg);

    const std::string& getClientName(int id);

    /**
     * Sends a request to the server to search for a
     * specific type of client among the active connected
     * clients.
     * @param type The type of client(s) to search for
     */
    bool clientTypeExist(const std::string &type);

    /**
     * Sets the text-label on the button to label.
     * @param label To use/not use label
     */
    void useLabel(bool label);

    public Q_SLOTS:
    /**
     * Connects to the running coserver if not connected,
     * disconnects if already connected.
     */
    void connectToServer();

    /**
     * Sets the name of the other connected client on the connect button.
     * @param name Name of connecting client
     */
    void setLabel(const std::string& name);

    /**
     * Displays "Tilkoblet" as tooltip to coserver client button
     * when connected.
     */
    void connected();
    void disconnected();

    void unableToConnect();


Q_SIGNALS:
    void receivedMessage(const miMessage&);
    void addressListChanged();
    void connectionClosed();
    void connectedToServer();

private:
    std::auto_ptr<CoClient> coclient;
    bool uselabel;
};

#endif
