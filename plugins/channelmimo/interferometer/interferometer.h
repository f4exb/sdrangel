///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_INTERFEROMETER_H
#define INCLUDE_INTERFEROMETER_H

#include <vector>

#include "channel/channelapi.h"
#include "interferometersink.h"
#include "interferometercorr.h"

class DeviceAPI;
class DownChannelizer;
class ThreadedBasebandSampleSink;

class Interferometer: public QObject, ChannelAPI
{
    Q_OBJECT
public:
    Interferometer(DeviceAPI *deviceAPI);
	virtual ~Interferometer();
	virtual void destroy() { delete this; }

    static const QString m_channelIdURI;
    static const QString m_channelId;

private:
    DeviceAPI *m_deviceAPI;
    InterferometerCorrelator m_correlator;
    std::vector<InterferometerSink> m_sinks;
    std::vector<DownChannelizer*> m_channelizers;
    std::vector<ThreadedBasebandSampleSink*> m_threadedBasebandSampleSinks;
    BasebandSampleSink* m_spectrumSink;
    BasebandSampleSink* m_scopeSink;
    InterferometerSettings m_settings;

private slots:
    void handleData(int start, int stop);
};

#endif // INCLUDE_INTERFEROMETER_H
