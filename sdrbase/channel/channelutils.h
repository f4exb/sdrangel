///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef SDRBASE_CHANNEL_CHANNELUTILS_H_
#define SDRBASE_CHANNEL_CHANNELUTILS_H_

#include <QString>
#include "export.h"

class SDRBASE_API ChannelUtils
{
public:
    static bool compareChannelURIs(const QString& registerdChannelURI, const QString& xChannelURI);
    static QString getRegisteredChannelURI(const QString& xChannelURI);
};

#endif // SDRBASE_CHANNEL_CHANNELUTILS_H_