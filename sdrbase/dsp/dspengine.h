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
#include <vector>
#include "audio/audiooutput.h"
#include "audio/audioinput.h"
#include "util/export.h"
#ifdef DSD_USE_SERIALDV
#include "dsp/dvserialengine.h"
#endif

class DSPDeviceSourceEngine;
class DSPDeviceSinkEngine;

class SDRANGEL_API DSPEngine : public QObject {
	Q_OBJECT
public:
	DSPEngine();
	~DSPEngine();

	static DSPEngine *instance();

	uint getAudioSampleRate() const { return m_audioOutputSampleRate; }

	DSPDeviceSourceEngine *addDeviceSourceEngine();
	void removeLastDeviceSourceEngine();

	DSPDeviceSinkEngine *addDeviceSinkEngine();
	void removeLastDeviceSinkEngine();

	void startAudioOutput();
	void stopAudioOutput();
    void startAudioOutputImmediate();
    void stopAudioOutputImmediate();

    void startAudioInput();
    void stopAudioInput();
    void startAudioInputImmediate();
    void stopAudioInputImmediate();

    DSPDeviceSourceEngine *getDeviceSourceEngineByIndex(uint deviceIndex) { return m_deviceSourceEngines[deviceIndex]; }
    DSPDeviceSourceEngine *getDeviceSourceEngineByUID(uint uid);

    DSPDeviceSinkEngine *getDeviceSinkEngineByIndex(uint deviceIndex) { return m_deviceSinkEngines[deviceIndex]; }
    DSPDeviceSinkEngine *getDeviceSinkEngineByUID(uint uid);

    void addAudioSink(AudioFifo* audioFifo); //!< Add the audio sink
	void removeAudioSink(AudioFifo* audioFifo); //!< Remove the audio sink

	void addAudioSource(AudioFifo* audioFifo); //!< Add an audio source
    void removeAudioSource(AudioFifo* audioFifo); //!< Remove an audio source

	// Serial DV methods:

	bool hasDVSerialSupport()
	{
#ifdef DSD_USE_SERIALDV
	    return m_dvSerialSupport;
#else
        return false;
#endif
	}

	void setDVSerialSupport(bool support);

	void getDVSerialNames(std::vector<std::string>& deviceNames)
	{
#ifdef DSD_USE_SERIALDV
	    m_dvSerialEngine.getDevicesNames(deviceNames);
#endif
	}

	void pushMbeFrame(const unsigned char *mbeFrame, int mbeRateIndex, int mbeVolumeIndex, unsigned char channels, AudioFifo *audioFifo)
	{
#ifdef DSD_USE_SERIALDV
	    m_dvSerialEngine.pushMbeFrame(mbeFrame, mbeRateIndex, mbeVolumeIndex, channels, audioFifo);
#endif
	}

private:
	std::vector<DSPDeviceSourceEngine*> m_deviceSourceEngines;
	uint m_deviceSourceEnginesUIDSequence;
	std::vector<DSPDeviceSinkEngine*> m_deviceSinkEngines;
	uint m_deviceSinkEnginesUIDSequence;
	AudioOutput m_audioOutput;
	AudioInput m_audioInput;
	uint m_audioOutputSampleRate;
    uint m_audioInputSampleRate;
	bool m_dvSerialSupport;
#ifdef DSD_USE_SERIALDV
	DVSerialEngine m_dvSerialEngine;
#endif
};

#endif // INCLUDE_DSPENGINE_H
