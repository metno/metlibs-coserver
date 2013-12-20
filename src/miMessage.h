// -*- c++ -*-
/*
  libqUtilities - Diverse Qt-classes and coserver base

  Copyright (C) 2013 met.no

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

#ifndef _miMessage_h
#define _miMessage_h

/* Created by DNMI/FoU/PU: k.roysland@dnmi.no
   at Tue Mar  5 10:20:47 2002 */

#include <string>
#include <vector>

class miMessage {
public:
  miMessage();
  miMessage(int to, int from, const char *command,
            const char *description);

  int to,from;
  std::string command,description,commondesc,common,clientType,co;
  std::vector <std::string> data;

  std::string content() const;
};

#endif
