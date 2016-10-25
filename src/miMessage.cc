/*
  libqUtilities - Diverse Qt-classes and coserver base

  Copyright (C) 2006-2015 met.no

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "miMessage.h"
#include "QLetterCommands.h"

#include <sstream>

#define MILOGGER_CATEGORY "coserver.Message"
#include <qUtilities/miLoggingQt.h>

namespace {
QStringList split(const std::string& s, int count=2)
{
    QString q = QString::fromStdString(s);
    if (count >= 2)
        return q.split(':');
    else
        return QStringList(q);
}

std::string join(const QStringList& l)
{
    std::ostringstream s;
    for (int i=0; i<l.count(); ++i) {
        if (i>0)
            s << ':';
        s << l.at(i).toStdString();
    }
    return s.str();
}
const QString empty_QString;
} // namespace

// ########################################################################

miMessage::miMessage()
{
  to = qmstrings::default_id;
}


miMessage::miMessage(int t, int f, const char * c, const char *d)
{
  to = t;
  from = f;
  command = c;
  description = d;
}


std::string miMessage::content() const
{
  std::ostringstream os;
  os << "======================================" << std::endl
     << "from:        " << from                  << std::endl
     << "to:          " << to                    << std::endl
     << "commondesc:  " << commondesc            << std::endl
     << "common:      " << common                << std::endl
     << "description: " << description           << std::endl
     << "command:     " << command               << std::endl
     << (data.size() ? "DATA:"  : "NO DATA")     << std::endl;
  for (unsigned int i = 0; i < data.size(); i++)
    os << i  << " > " << data[i] << std::endl;
  os << "======================================" << std::endl;
  return os.str();
}

// ########################################################################

miQMessage::miQMessage()
{
}

miQMessage::miQMessage(const QString& c)
    : mCommand(c)
{
}

miQMessage& miQMessage::addCommon(const QString& desc, const QString& value)
{
    commonDesc << desc;
    commonValues << value;
    return *this;
}

miQMessage& miQMessage::addCommon(const QString& desc, int value)
{
    return addCommon(desc, QString::number(value));
}

void miQMessage::setCommon(const QStringList& desc, const QStringList& values)
{
    if (desc.count() == values.count()) {
        commonDesc = desc;
        commonValues = values;
    }
}

int miQMessage::findCommonDesc(const QString& desc) const
{
    for (int i=0; i<countCommon(); ++i)
        if (desc == commonDesc.at(i))
            return i;
    return -1;
}

const QString& miQMessage::getCommonValue(const QString& desc) const
{
    const int idx = findCommonDesc(desc);
    if (idx >= 0)
        return commonValues.at(idx);
    else
        return empty_QString;
}

miQMessage& miQMessage::addDataDesc(const QString& desc)
{
    if (dataRows.isEmpty())
        dataDesc << desc;
    else
        ; // ERROR
    return *this;
}

miQMessage& miQMessage::addDataValues(const QStringList& values)
{
    if (values.count() == dataDesc.count())
        dataRows << values;
    else
        ; // ERROR
    return *this;
}

void miQMessage::setData(const QStringList& desc, const QList<QStringList>& rows)
{
    dataDesc = desc;
    dataRows = rows;
}

int miQMessage::findDataDesc(const QString& desc) const
{
    for (int i=0; i<countDataColumns(); ++i)
        if (desc == dataDesc.at(i))
            return i;
    return -1;
}

// ########################################################################

void convert(int from, int to, const miQMessage& qmsg, miMessage& msg)
{
    msg.to = to;
    msg.from = from;
    msg.command = qmsg.command().toStdString();
    msg.commondesc = join(qmsg.getCommonDesc());
    msg.common = join(qmsg.getCommonValues());
    msg.description = join(qmsg.getDataDesc());

    msg.data.clear();
    for (int r=0; r<qmsg.countDataRows(); ++r)
        msg.data.push_back(join(qmsg.getDataValues(r)));
}

void convert(const miMessage& msg, int& from, int& to, miQMessage& qmsg)
{
    to = msg.to;
    from = msg.from;
    qmsg.setCommand(QString::fromStdString(msg.command));

    const QStringList commondesc = split(msg.commondesc);
    qmsg.setCommon(commondesc, split(msg.common, commondesc.count()));

    const QStringList dataDesc = split(msg.description);
    QList<QStringList> dataRows;
    dataRows.reserve(msg.data.size());
    for (std::vector<std::string>::const_iterator it = msg.data.begin(); it != msg.data.end(); ++it)
        dataRows << split(*it, dataDesc.count());
    qmsg.setData(dataDesc, dataRows);
}

std::ostream& operator<< (std::ostream& out, const miQMessage& qmsg)
{
    out << "command='" << qmsg.command() << "'\n"
        << " commondesc=" << qmsg.getCommonDesc() << '\n'
        << " common=" << qmsg.getCommonValues() << '\n'
        << " desc=" << qmsg.getDataDesc() << '\n';
    int n = std::min(10, qmsg.countDataRows());
    for (int i=0; i<n; ++i)
        out << " data[" << i << "]=" << qmsg.getDataValues(i) << '\n';
    if (n < qmsg.countDataRows())
        out << " skipped rows 10.." << qmsg.countDataRows() << '\n';
    return out;
}
