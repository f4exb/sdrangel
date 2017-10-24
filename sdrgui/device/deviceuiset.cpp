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

#include <QFont>

#include "gui/glspectrum.h"
#include "dsp/spectrumvis.h"
#include "gui/glspectrumgui.h"
#include "gui/channelwindow.h"
#include "gui/samplingdevicecontrol.h"
#include "dsp/dspdevicesourceengine.h"
#include "dsp/dspdevicesinkengine.h"
#include "device/devicesourceapi.h"
#include "device/devicesinkapi.h"

#include "deviceuiset.h"

DeviceUISet::DeviceUISet(QTimer& timer)
{
    m_spectrum = new GLSpectrum;
    m_spectrumVis = new SpectrumVis(m_spectrum);
    m_spectrum->connectTimer(timer);
    m_spectrumGUI = new GLSpectrumGUI;
    m_spectrumGUI->setBuddies(m_spectrumVis->getInputMessageQueue(), m_spectrumVis, m_spectrum);
    m_channelWindow = new ChannelWindow;
    m_samplingDeviceControl = new SamplingDeviceControl;
    m_deviceSourceEngine = 0;
    m_deviceSourceAPI = 0;
    m_deviceSinkEngine = 0;
    m_deviceSinkAPI = 0;

    // m_spectrum needs to have its font to be set since it cannot be inherited from the main window
    QFont font;
    font.setFamily(QStringLiteral("Sans Serif"));
    font.setPointSize(9);
    m_spectrum->setFont(font);

}

DeviceUISet::~DeviceUISet()
{
    delete m_samplingDeviceControl;
    delete m_channelWindow;
    delete m_spectrumGUI;
    delete m_spectrumVis;
    delete m_spectrum;
}




