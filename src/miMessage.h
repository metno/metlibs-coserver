// -*- c++ -*-
/*
  libqUtilities - Diverse Qt-classes and coserver base

  Copyright (C) 2015 met.no

  Contact information:
  Norwegian Meteorological Institute
  Box 43 Blindern
  0313 OSLO
  NORWAY
  email: diana@met.no

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef METLIBS_COSERVER_MIMESSAGE_H
#define METLIBS_COSERVER_MIMESSAGE_H 1

#include <QString>
#include <QStringList>

#include <iosfwd>
#include <set>
#include <string>
#include <vector>

class miMessage {
public:
  miMessage();
  miMessage(int to, int from, const char *command,
            const char *description);

  int to,from;
  std::string command,description,commondesc,common;
  std::vector <std::string> data;

  std::string content() const;
};

typedef std::set<int> ClientIds;

inline ClientIds clientId(int id)
{ ClientIds ids; ids.insert(id); return ids; }

// ========================================================================

class miQMessage {
public:
    miQMessage();
    explicit miQMessage(const QString& command);

    const QString& command() const
        { return mCommand; }

    void setCommand(const QString& cmd)
        { mCommand = cmd; }

    miQMessage& addCommon(const QString& desc, const QString& value);
    miQMessage& addCommon(const QString& desc, int value);
    void setCommon(const QStringList& desc, const QStringList& values);

    int countCommon() const
        { return commonDesc.count(); }
    const QString& getCommonDesc(int idx) const
        { return commonDesc.at(idx); }
    const QString& getCommonValue(int idx) const
        { return commonValues.at(idx); }
    int findCommonDesc(const QString& desc) const;
    const QString& getCommonValue(const QString& desc) const;

    const QStringList& getCommonDesc() const
        { return commonDesc; }
    const QStringList& getCommonValues() const
        { return commonValues; }

    miQMessage& addDataDesc(const QString& desc);
    miQMessage& addDataValues(const QStringList& values);
    void setData(const QStringList& desc, const QList<QStringList>& rows);

    int countDataRows() const
        { return dataRows.count(); }
    int countDataColumns() const
        { return dataDesc.count(); }
    const QString& getDataDesc(int column) const
        { return dataDesc.at(column); }
    const QString& getDataValue(int row, int column) const
        { return dataRows.at(row).at(column); }
    int findDataDesc(const QString& desc) const;

    const QStringList& getDataDesc() const
        { return dataDesc; }
    const QStringList& getDataValues(int row) const
        { return dataRows.at(row); }

private:
    QString mCommand;
    QStringList commonDesc, commonValues;
    QStringList dataDesc;
    QList<QStringList> dataRows;
};

void convert(int from, int to, const miQMessage& qmsg, miMessage& msg);
void convert(const miMessage& msg, int& from, int& to, miQMessage& qmsg);

std::ostream& operator<< (std::ostream& out, const miQMessage& qmsg);

#endif // METLIBS_COSERVER_MIMESSAGE_H
