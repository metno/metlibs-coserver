/*
  libqUtilities - Diverse Qt-classes and coserver base

  $Id: miMessage.cc 4333 2013-12-19 11:11:48Z juergens $

  Copyright (C) 2006 met.no

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
