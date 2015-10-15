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
#include "dsp/dspdeviceengine.h"
#include "audio/audiooutput.h"
#include "util/export.h"

class DSPDeviceEngine;
class ThreadedSampleSink;

class SDRANGEL_API DSPEngine : public QObject {
	Q_OBJECT
public:
	DSPEngine();
	~DSPEngine();

	static DSPEngine *instance();

	MessageQueue* getInputMessageQueue();
	MessageQueue* getOutputMessageQueue();

	uint getAudioSampleRate() const { return m_audioSampleRate; }

	void start(); //!< Device engine(s) start
	void stop();  //!< Device engine(s) stop

	bool initAcquisition(); //!< Initialize acquisition sequence
	bool startAcquisition(); //!< Start acquisition sequence
	void stopAcquistion();   //!< Stop acquisition sequence

	void setSource(SampleSource* source); //!< Set the sample source type
	void setSourceSequence(int sequence); //!< Set the sample source sequence in type

	void addSink(SampleSink* sink); //!< Add a sample sink
	void removeSink(SampleSink* sink); //!< Remove a sample sink

	void addThreadedSink(ThreadedSampleSink* sink); //!< Add a sample sink that will run on its own thread
	void removeThreadedSink(ThreadedSampleSink* sink); //!< Remove a sample sink that runs on its own thread

	void addAudioSink(AudioFifo* audioFifo); //!< Add the audio sink
	void removeAudioSink(AudioFifo* audioFifo); //!< Remove the audio sink

 	void configureCorrections(bool dcOffsetCorrection, bool iqImbalanceCorrection); //!< Configure DSP corrections

 	DSPDeviceEngine::State state() const;

 	QString errorMessage(); //!< Return the current error message
	QString sourceDeviceDescription(); //!< Return the source device description

private:
	DSPDeviceEngine *m_deviceEngine;
	AudioOutput m_audioOutput;
	uint m_audioSampleRate;
};

#endif // INCLUDE_DSPENGINE_H
