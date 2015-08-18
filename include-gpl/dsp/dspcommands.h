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

#ifndef INCLUDE_DSPCOMMANDS_H
#define INCLUDE_DSPCOMMANDS_H

#include <QString>
#include "util/message.h"
#include "fftwindow.h"
#include "util/export.h"

class SampleSource;
class SampleSink;
class AudioFifo;

class SDRANGELOVE_API DSPPing : public Message {
	MESSAGE_CLASS_DECLARATION
};

class SDRANGELOVE_API DSPExit : public Message {
	MESSAGE_CLASS_DECLARATION
};

class SDRANGELOVE_API DSPAcquisitionInit : public Message {
	MESSAGE_CLASS_DECLARATION
};

class SDRANGELOVE_API DSPAcquisitionStart : public Message {
	MESSAGE_CLASS_DECLARATION
};

class SDRANGELOVE_API DSPAcquisitionStop : public Message {
	MESSAGE_CLASS_DECLARATION
};

class SDRANGELOVE_API DSPGetSourceDeviceDescription : public Message {
	MESSAGE_CLASS_DECLARATION

public:
	void setDeviceDescription(const QString& text) { m_deviceDescription = text; }
	const QString& getDeviceDescription() const { return m_deviceDescription; }

private:
	QString m_deviceDescription;
};

class SDRANGELOVE_API DSPGetErrorMessage : public Message {
	MESSAGE_CLASS_DECLARATION

public:
	void setErrorMessage(const QString& text) { m_errorMessage = text; }
	const QString& getErrorMessage() const { return m_errorMessage; }

private:
	QString m_errorMessage;
};

class SDRANGELOVE_API DSPSetSource : public Message {
	MESSAGE_CLASS_DECLARATION

public:
	DSPSetSource(SampleSource* sampleSource) : Message(), m_sampleSource(sampleSource) { }

	SampleSource* getSampleSource() const { return m_sampleSource; }

private:
	SampleSource* m_sampleSource;
};

class SDRANGELOVE_API DSPAddSink : public Message {
	MESSAGE_CLASS_DECLARATION

public:
	DSPAddSink(SampleSink* sampleSink) : Message(), m_sampleSink(sampleSink) { }

	SampleSink* getSampleSink() const { return m_sampleSink; }

private:
	SampleSink* m_sampleSink;
};

class SDRANGELOVE_API DSPRemoveSink : public Message {
	MESSAGE_CLASS_DECLARATION

public:
	DSPRemoveSink(SampleSink* sampleSink) : Message(), m_sampleSink(sampleSink) { }

	SampleSink* getSampleSink() const { return m_sampleSink; }

private:
	SampleSink* m_sampleSink;
};

class SDRANGELOVE_API DSPAddThreadedSink : public Message {
	MESSAGE_CLASS_DECLARATION

public:
	DSPAddThreadedSink(SampleSink* sampleSink) : Message(), m_sampleSink(sampleSink) { }

	SampleSink* getSampleSink() const { return m_sampleSink; }

private:
	SampleSink* m_sampleSink;
};

class SDRANGELOVE_API DSPRemoveThreadedSink : public Message {
	MESSAGE_CLASS_DECLARATION

public:
	DSPRemoveThreadedSink(SampleSink* sampleSink) : Message(), m_sampleSink(sampleSink) { }

	SampleSink* getSampleSink() const { return m_sampleSink; }

private:
	SampleSink* m_sampleSink;
};

class SDRANGELOVE_API DSPAddAudioSink : public Message {
	MESSAGE_CLASS_DECLARATION

public:
	DSPAddAudioSink(AudioFifo* audioFifo) : Message(), m_audioFifo(audioFifo) { }

	AudioFifo* getAudioFifo() const { return m_audioFifo; }

private:
	AudioFifo* m_audioFifo;
};

class SDRANGELOVE_API DSPRemoveAudioSink : public Message {
	MESSAGE_CLASS_DECLARATION

public:
	DSPRemoveAudioSink(AudioFifo* audioFifo) : Message(), m_audioFifo(audioFifo) { }

	AudioFifo* getAudioFifo() const { return m_audioFifo; }

private:
	AudioFifo* m_audioFifo;
};

/*
class SDRANGELOVE_API DSPConfigureSpectrumVis : public Message {
	MESSAGE_CLASS_DECLARATION

public:
	DSPConfigureSpectrumVis(int fftSize, int overlapPercent, FFTWindow::Function window) :
		Message(),
		m_fftSize(fftSize),
		m_overlapPercent(overlapPercent),
		m_window(window)
	{ }

	int getFFTSize() const { return m_fftSize; }
	int getOverlapPercent() const { return m_overlapPercent; }
	FFTWindow::Function getWindow() const { return m_window; }

private:
	int m_fftSize;
	int m_overlapPercent;
	FFTWindow::Function m_window;
};*/

class SDRANGELOVE_API DSPConfigureCorrection : public Message {
	MESSAGE_CLASS_DECLARATION

public:
	DSPConfigureCorrection(bool dcOffsetCorrection, bool iqImbalanceCorrection) :
		Message(),
		m_dcOffsetCorrection(dcOffsetCorrection),
		m_iqImbalanceCorrection(iqImbalanceCorrection)
	{ }

	bool getDCOffsetCorrection() const { return m_dcOffsetCorrection; }
	bool getIQImbalanceCorrection() const { return m_iqImbalanceCorrection; }

private:
	bool m_dcOffsetCorrection;
	bool m_iqImbalanceCorrection;

};

class SDRANGELOVE_API DSPEngineReport : public Message {
	MESSAGE_CLASS_DECLARATION

public:
	DSPEngineReport(int sampleRate, quint64 centerFrequency) :
		Message(),
		m_sampleRate(sampleRate),
		m_centerFrequency(centerFrequency)
	{ }

	int getSampleRate() const { return m_sampleRate; }
	quint64 getCenterFrequency() const { return m_centerFrequency; }

private:
	int m_sampleRate;
	quint64 m_centerFrequency;
};

class SDRANGELOVE_API DSPConfigureScopeVis : public Message {
	MESSAGE_CLASS_DECLARATION

public:
	DSPConfigureScopeVis(int triggerChannel, Real triggerLevelHigh, Real triggerLevelLow) :
		Message(),
		m_triggerChannel(triggerChannel),
		m_triggerLevelHigh(triggerLevelHigh),
		m_triggerLevelLow(triggerLevelLow)
	{ }

	int getTriggerChannel() const { return m_triggerChannel; }
	Real getTriggerLevelHigh() const { return m_triggerLevelHigh; }
	Real getTriggerLevelLow() const { return m_triggerLevelLow; }

private:
	int m_triggerChannel;
	Real m_triggerLevelHigh;
	Real m_triggerLevelLow;
};

class SDRANGELOVE_API DSPSignalNotification : public Message {
	MESSAGE_CLASS_DECLARATION

public:
	DSPSignalNotification(int samplerate, qint64 frequencyOffset) :
		Message(),
		m_sampleRate(samplerate),
		m_frequencyOffset(frequencyOffset)
	{ }

	int getSampleRate() const { return m_sampleRate; }
	qint64 getFrequencyOffset() const { return m_frequencyOffset; }

private:
	int m_sampleRate;
	qint64 m_frequencyOffset;
};

class SDRANGELOVE_API DSPConfigureChannelizer : public Message {
	MESSAGE_CLASS_DECLARATION

public:
	DSPConfigureChannelizer(int sampleRate, int centerFrequency) :
		Message(),
		m_sampleRate(sampleRate),
		m_centerFrequency(centerFrequency)
	{ }

	int getSampleRate() const { return m_sampleRate; }
	int getCenterFrequency() const { return m_centerFrequency; }

private:
	int m_sampleRate;
	int m_centerFrequency;
};

#endif // INCLUDE_DSPCOMMANDS_H
