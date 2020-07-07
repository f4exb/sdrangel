///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB.                                  //
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

#include "sigmffilesinksettings.h"

class FileRecordInterface;

class SigMFFileSinkSink : public ChannelSampleSink {
public:
    SigMFFileSinkSink();
	~SigMFFileSinkSink();

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

    SigMFFileRecord *getFileSink() { return &m_fileSink; }
    void startRecording();
    void stopRecording();
    void setDeviceHwId(const QString& hwId) { m_deviceHwId = hwId; }
    void setDeviceUId(int uid) { m_deviceUId = uid; }
    void setSampleRate(int sampleRate);
	void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void applySettings(const SigMFFileSinkSettings& settings, bool force = false);

private:
    int m_channelSampleRate;
    int m_channelFrequencyOffset;
    SigMFFileSinkSettings m_settings;
    SigMFFileRecord m_fileSink;
    bool m_record;
    QString m_deviceHwId;
    int m_deviceUId;
};

#endif // INCLUDE_SIFMFFILESINKSINK_H_
