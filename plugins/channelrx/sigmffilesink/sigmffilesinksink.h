///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2020 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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

#ifndef INCLUDE_SIFMFFILESINKSINK_H_
#define INCLUDE_SIFMFFILESINKSINK_H_

#include "dsp/channelsamplesink.h"
#include "dsp/sigmffilerecord.h"
#include "dsp/decimatorc.h"
#include "dsp/samplesimplefifo.h"
#include "dsp/ncof.h"

#include "sigmffilesinksettings.h"

class FileRecordInterface;
class SpectrumVis;

class SigMFFileSinkSink : public ChannelSampleSink {
public:
    SigMFFileSinkSink();
	~SigMFFileSinkSink();

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

    SigMFFileRecord *getFileSink() { return &m_fileSink; }
    void setSpectrumSink(SpectrumVis* spectrumSink) { m_spectrumSink = spectrumSink; }
    void startRecording();
    void stopRecording();
    void setDeviceHwId(const QString& hwId) { m_deviceHwId = hwId; }
    void setDeviceUId(int uid) { m_deviceUId = uid; }
    void applyChannelSettings(
        int channelSampleRate,
        int sinkSampleRate,
        int channelFrequencyOffset,
        int64_t centerFrequency,
        bool force = false);
    void applySettings(const SigMFFileSinkSettings& settings, bool force = false);
    uint64_t getMsCount() const { return m_msCount; }
    uint64_t getByteCount() const { return m_byteCount; }
    unsigned int getNbTracks() const { return m_fileSink.getNbCaptures(); }
    void setMessageQueueToGUI(MessageQueue *messageQueue) { m_msgQueueToGUI = messageQueue; }
    void squelchRecording(bool squelchOpen);
    int getSampleRate() const { return m_sinkSampleRate; }
    bool isRecording() const { return m_record; }

private:
    int m_channelSampleRate;
    int m_channelFrequencyOffset;
    int m_sinkSampleRate;
    int64_t m_centerFrequency;
	NCOF m_nco;
    DecimatorC m_decimator;
    SampleVector m_sampleBuffer;
    SigMFFileSinkSettings m_settings;
    SigMFFileRecord m_fileSink;
    SampleSimpleFifo m_preRecordBuffer;
    unsigned int m_preRecordFill;
    SpectrumVis* m_spectrumSink;
    MessageQueue *m_msgQueueToGUI;
    bool m_recordEnabled;
    bool m_record;
    bool m_squelchOpen;
    int m_postSquelchCounter;
    QString m_deviceHwId;
    int m_deviceUId;
    uint64_t m_msCount;
    uint64_t m_byteCount;
};

#endif // INCLUDE_SIFMFFILESINKSINK_H_
