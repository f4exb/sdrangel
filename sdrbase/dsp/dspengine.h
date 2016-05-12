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
#include "dsp/dspdeviceengine.h"
#include "audio/audiooutput.h"
#include "util/export.h"
#ifdef DSD_USE_SERIALDV
#include "dsp/dvserialengine.h"
#endif

class DSPDeviceEngine;
class ThreadedSampleSink;

class SDRANGEL_API DSPEngine : public QObject {
	Q_OBJECT
public:
	DSPEngine();
	~DSPEngine();

	static DSPEngine *instance();

	uint getAudioSampleRate() const { return m_audioSampleRate; }

	void stopAllAcquisitions();
	void stopAllDeviceEngines();

	void startAudio();
	void stopAudio();

    DSPDeviceEngine *getDeviceEngineByIndex(uint deviceIndex) { return m_deviceEngines[deviceIndex]; }
    DSPDeviceEngine *getDeviceEngineByUID(uint uid);

	void addAudioSink(AudioFifo* audioFifo); //!< Add the audio sink
	void removeAudioSink(AudioFifo* audioFifo); //!< Remove the audio sink

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

	void pushMbeFrame(const unsigned char *mbeFrame, int mbeRateIndex, int mbeVolumeIndex, AudioFifo *audioFifo)
	{
#ifdef DSD_USE_SERIALDV
	    m_dvSerialEngine.pushMbeFrame(mbeFrame, mbeRateIndex, mbeVolumeIndex, audioFifo);
#endif
	}

private:
	std::vector<DSPDeviceEngine*> m_deviceEngines;
	AudioOutput m_audioOutput;
	uint m_audioSampleRate;
	bool m_dvSerialSupport;
	uint m_audioUsageCount;
#ifdef DSD_USE_SERIALDV
	DVSerialEngine m_dvSerialEngine;
#endif
};

#endif // INCLUDE_DSPENGINE_H
