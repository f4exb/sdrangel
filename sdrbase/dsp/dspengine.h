///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#ifndef INCLUDE_DSPENGINE_H
#define INCLUDE_DSPENGINE_H

#include <QObject>
#include <QTimer>
#include <vector>

#include "audio/audiodevicemanager.h"
#include "audio/audiooutput.h"
#include "audio/audioinput.h"
#include "export.h"
#ifdef DSD_USE_SERIALDV
#include "dsp/dvserialengine.h"
#endif

class DSPDeviceSourceEngine;
class DSPDeviceSinkEngine;

class SDRBASE_API DSPEngine : public QObject {
	Q_OBJECT
public:
	DSPEngine();
	~DSPEngine();

	static DSPEngine *instance();

	unsigned int getDefaultAudioSampleRate() const { return AudioDeviceManager::m_defaultAudioSampleRate; }

	DSPDeviceSourceEngine *addDeviceSourceEngine();
	void removeLastDeviceSourceEngine();

	DSPDeviceSinkEngine *addDeviceSinkEngine();
	void removeLastDeviceSinkEngine();

	AudioDeviceManager *getAudioDeviceManager() { return &m_audioDeviceManager; }

    DSPDeviceSourceEngine *getDeviceSourceEngineByIndex(uint deviceIndex) { return m_deviceSourceEngines[deviceIndex]; }
    DSPDeviceSourceEngine *getDeviceSourceEngineByUID(uint uid);

    DSPDeviceSinkEngine *getDeviceSinkEngineByIndex(uint deviceIndex) { return m_deviceSinkEngines[deviceIndex]; }
    DSPDeviceSinkEngine *getDeviceSinkEngineByUID(uint uid);

	// Serial DV methods:

	bool hasDVSerialSupport();
	void setDVSerialSupport(bool support);
	void getDVSerialNames(std::vector<std::string>& deviceNames);
	void pushMbeFrame(
	        const unsigned char *mbeFrame,
	        int mbeRateIndex,
	        int mbeVolumeIndex,
	        unsigned char channels,
	        bool useHP,
	        int upsampling,
	        AudioFifo *audioFifo);

    const QTimer& getMasterTimer() const { return m_masterTimer; }

private:
	std::vector<DSPDeviceSourceEngine*> m_deviceSourceEngines;
	uint m_deviceSourceEnginesUIDSequence;
	std::vector<DSPDeviceSinkEngine*> m_deviceSinkEngines;
	uint m_deviceSinkEnginesUIDSequence;
    AudioDeviceManager m_audioDeviceManager;
    int m_audioInputDeviceIndex;
    int m_audioOutputDeviceIndex;
    QTimer m_masterTimer;
	bool m_dvSerialSupport;
#ifdef DSD_USE_SERIALDV
	DVSerialEngine m_dvSerialEngine;
#endif
};

#endif // INCLUDE_DSPENGINE_H
