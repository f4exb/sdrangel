///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#ifndef SDRBASE_CHANNEL_CHANNELWEBAPIUTILS_H_
#define SDRBASE_CHANNEL_CHANNELWEBAPIUTILS_H_

#include <QString>

#include "export.h"

class SDRBASE_API ChannelWebAPIUtils
{
public:
    static bool getCenterFrequency(unsigned int deviceIndex, double &frequencyInHz);
    static bool setCenterFrequency(unsigned int deviceIndex, double frequencyInHz);
    static bool run(unsigned int deviceIndex, int subsystemIndex=0);
    static bool stop(unsigned int deviceIndex, int subsystemIndex=0);
    static bool getFrequencyOffset(unsigned int deviceIndex, int channelIndex, int& offset);
    static bool setFrequencyOffset(unsigned int deviceIndex, int channelIndex, int offset);
    static bool startStopFileSinks(unsigned int deviceIndex, bool start);
    static bool satelliteAOS(const QString name, bool northToSouthPass);
    static bool satelliteLOS(const QString name);
};

#endif // SDRBASE_CHANNEL_CHANNELWEBAPIUTILS_H_
