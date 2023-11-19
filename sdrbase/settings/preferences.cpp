///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany     //
// written by Christian Daniel                                                       //
// Copyright (C) 2015-2017, 2019, 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com> //
// Copyright (C) 2020, 2022-2023 Jon Beniston, M7RCE <jon@beniston.com>              //
//                                                                                   //
// This program is free software; you can redistribute it and/or modify              //
// it under the terms of the GNU General Public License as published by              //
// the Free Software Foundation as version 3 of the License, or                      //
// (at your option) any later version.                                               //
//                                                                                   //
// This program is distributed in the hope that it will be useful,                   //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                    //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                      //
// GNU General Public License V3 for more details.                                   //
//                                                                                   //
// You should have received a copy of the GNU General Public License                 //
// along with this program. If not, see <http://www.gnu.org/licenses/>.              //
///////////////////////////////////////////////////////////////////////////////////////
#include "settings/preferences.h"
#include "util/simpleserializer.h"

Preferences::Preferences()
{
	resetToDefaults();
}

void Preferences::resetToDefaults()
{
	m_sourceDevice.clear();
	m_audioType.clear();
	m_audioDevice.clear();
	m_sourceIndex = 0;
	m_sourceItemIndex = 0;
    m_stationName = "Home";
	m_latitude = 49.012423; // Set an interesting location so map doesn't open up in the middle of the ocean
	m_longitude = 8.418125;
    m_altitude = 0.0f;
    m_autoUpdatePosition = true;
	m_useLogFile = false;
	m_logFileName = "sdrangel.log";
	m_consoleMinLogLevel = QtDebugMsg;
    m_fileMinLogLevel = QtDebugMsg;
    m_multisampling = 0;
    m_mapMultisampling = 0;
    m_mapSmoothing = true;
    m_fftEngine = "FFTW";
}

QByteArray Preferences::serialize() const
{
	SimpleSerializer s(1);
	s.writeString((int) SourceDevice, m_sourceDevice);
	s.writeString((int) AudioType, m_audioType);
	s.writeString((int) AudioDevice, m_audioDevice);
	s.writeS32((int) SourceIndex, m_sourceIndex);
	s.writeFloat((int) Latitude, m_latitude);
	s.writeFloat((int) Longitude, m_longitude);
	s.writeS32((int) ConsoleMinLogLevel, (int) m_consoleMinLogLevel);
	s.writeBool((int) UseLogFile, m_useLogFile);
	s.writeString((int) LogFileName, m_logFileName);
    s.writeS32((int) FileMinLogLevel, (int) m_fileMinLogLevel);
    s.writeString((int) StationName, m_stationName);
    s.writeFloat((int) Altitude, m_altitude);
	s.writeS32((int) SourceItemIndex, m_sourceItemIndex);
    s.writeS32((int) Multisampling, m_multisampling);
    s.writeBool((int) AutoUpdatePosition, m_autoUpdatePosition);
    s.writeS32((int) MapMultisampling, m_mapMultisampling);
    s.writeBool((int) MapSmoothing, m_mapSmoothing);
    s.writeString((int) FFTEngine, m_fftEngine);
	return s.final();
}

bool Preferences::deserialize(const QByteArray& data)
{
    int tmpInt;

	SimpleDeserializer d(data);

	if(!d.isValid()) {
		resetToDefaults();
		return false;
	}

	if(d.getVersion() == 1)
	{
		d.readString((int) SourceDevice, &m_sourceDevice);
		d.readString((int) AudioType, &m_audioType);
		d.readString((int) AudioDevice, &m_audioDevice);
		d.readS32((int) SourceIndex, &m_sourceIndex, 0);
		d.readFloat((int) Latitude, &m_latitude, 0.0f);
		d.readFloat((int) Longitude, &m_longitude, 0.0f);

		d.readS32((int) ConsoleMinLogLevel, &tmpInt, (int) QtDebugMsg);

		if ((tmpInt == (int) QtDebugMsg) ||
		    (tmpInt == (int) QtInfoMsg) ||
		    (tmpInt == (int) QtWarningMsg) ||
		    (tmpInt == (int) QtCriticalMsg) ||
		    (tmpInt == (int) QtFatalMsg)) {
            m_consoleMinLogLevel = (QtMsgType) tmpInt;
		} else {
		    m_consoleMinLogLevel = QtDebugMsg;
		}

		d.readBool((int) UseLogFile, &m_useLogFile, false);
		d.readString((int) LogFileName, &m_logFileName, "sdrangel.log");

        d.readS32((int) FileMinLogLevel, &tmpInt, (int) QtDebugMsg);
        d.readString((int) StationName, &m_stationName, "Home");
        d.readFloat((int) Altitude, &m_altitude, 0.0f);
		d.readS32((int) SourceItemIndex, &m_sourceItemIndex, 0);

        if ((tmpInt == (int) QtDebugMsg) ||
            (tmpInt == (int) QtInfoMsg) ||
            (tmpInt == (int) QtWarningMsg) ||
            (tmpInt == (int) QtCriticalMsg) ||
            (tmpInt == (int) QtFatalMsg)) {
            m_fileMinLogLevel = (QtMsgType) tmpInt;
        } else {
            m_fileMinLogLevel = QtDebugMsg;
        }

        d.readS32((int) Multisampling, &m_multisampling, 0);
        d.readBool((int) AutoUpdatePosition, &m_autoUpdatePosition, true);
        d.readS32((int) MapMultisampling, &m_mapMultisampling, 0);
        d.readBool((int) MapSmoothing, &m_mapSmoothing, true);

        d.readString((int) FFTEngine, &m_fftEngine, "FFTW");

		return true;
	}
    else
	{
		resetToDefaults();
		return false;
	}
}
