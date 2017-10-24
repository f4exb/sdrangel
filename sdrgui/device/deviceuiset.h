///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef SDRGUI_DEVICE_DEVICEUISET_H_
#define SDRGUI_DEVICE_DEVICEUISET_H_

#include <QTimer>
#include <QByteArray>

class SpectrumVis;
class GLSpectrum;
class GLSpectrumGUI;
class ChannelWindow;
class SamplingDeviceControl;
class DSPDeviceSourceEngine;
class DeviceSourceAPI;
class DSPDeviceSinkEngine;
class DeviceSinkAPI;

struct DeviceUISet
{
    SpectrumVis *m_spectrumVis;
    GLSpectrum *m_spectrum;
    GLSpectrumGUI *m_spectrumGUI;
    ChannelWindow *m_channelWindow;
    SamplingDeviceControl *m_samplingDeviceControl;
    DSPDeviceSourceEngine *m_deviceSourceEngine;
    DeviceSourceAPI *m_deviceSourceAPI;
    DSPDeviceSinkEngine *m_deviceSinkEngine;
    DeviceSinkAPI *m_deviceSinkAPI;
    QByteArray m_mainWindowState;

    DeviceUISet(QTimer& timer);
    ~DeviceUISet();
};




#endif /* SDRGUI_DEVICE_DEVICEUISET_H_ */
