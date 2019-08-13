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

#include "channelutils.h"

bool ChannelUtils::compareChannelURIs(const QString& registerdChannelURI, const QString& xChannelURI)
{
    return registerdChannelURI == getRegisteredChannelURI(xChannelURI);
}

QString ChannelUtils::getRegisteredChannelURI(const QString& xChannelURI)
{
    if ((xChannelURI == "sdrangel.channel.chanalyzerng")
     || (xChannelURI == "org.f4exb.sdrangelove.channel.chanalyzer")) {
        return "sdrangel.channel.chanalyzer";
    } else if (xChannelURI == "de.maintech.sdrangelove.channel.am") {
        return "sdrangel.channel.amdemod";
    } else if (xChannelURI == "de.maintech.sdrangelove.channel.nfm") {
        return "sdrangel.channel.nfmdemod";
    } else if (xChannelURI == "de.maintech.sdrangelove.channel.ssb") {
        return "sdrangel.channel.ssbdemod";
    } else if (xChannelURI == "de.maintech.sdrangelove.channel.wfm") {
        return "sdrangel.channel.wfmdemod";
    } else if (xChannelURI == "sdrangel.channel.udpsrc") {
        return "sdrangel.channel.udpsink";
    } else if (xChannelURI == "sdrangel.channeltx.udpsink") {
        return "sdrangel.channeltx.udpsource";
    } else  {
        return xChannelURI;
    }
}