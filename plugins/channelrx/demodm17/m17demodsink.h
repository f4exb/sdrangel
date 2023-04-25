///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_M17DEMODSINK_H
#define INCLUDE_M17DEMODSINK_H

#include <QVector>

#include "dsp/channelsamplesink.h"
#include "dsp/phasediscri.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/firfilter.h"
#include "dsp/afsquelch.h"
#include "dsp/afsquelch.h"
#include "audio/audiofifo.h"
#include "util/movingaverage.h"
#include "util/doublebufferfifo.h"

#include "m17demodsettings.h"
#include "m17demodprocessor.h"

class BasebandSampleSink;
class ChannelAPI;

class M17DemodSink : public ChannelSampleSink {
public:
    M17DemodSink();
	~M17DemodSink();

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

    void applyAudioSampleRate(int sampleRate);
    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
	void applySettings(const M17DemodSettings& settings, const QList<QString>& settingsKeys, bool force = false);
    AudioFifo *getAudioFifo() { return &m_audioFifo; }
    void setAudioFifoLabel(const QString& label) { m_audioFifo.setLabel("1:" + label); }
    int getAudioSampleRate() const { return m_audioSampleRate; }
    void setChannel(ChannelAPI *channel) { m_channel = channel; }

	void setScopeXYSink(BasebandSampleSink* scopeSink) { m_scopeXY = scopeSink; }
	void configureMyPosition(float myLatitude, float myLongitude);

	double getMagSq() { return m_magsq; }
	bool getSquelchOpen() const { return m_squelchOpen; }

    void getMagSqLevels(double& avg, double& peak, int& nbSamples)
    {
        if (m_magsqCount > 0)
        {
            m_magsq = m_magsqSum / m_magsqCount;
            m_magSqLevelStore.m_magsq = m_magsq;
            m_magSqLevelStore.m_magsqPeak = m_magsqPeak;
        }

        avg = m_magSqLevelStore.m_magsq;
        peak = m_magSqLevelStore.m_magsqPeak;
        nbSamples = m_magsqCount == 0 ? 1 : m_magsqCount;

        m_magsqSum = 0.0f;
        m_magsqPeak = 0.0f;
        m_magsqCount = 0;
    }

    void getDiagnostics(
        bool& dcd,
        float& evm,
        float& deviation,
        float& offset,
        int& status,
        int& sync_word_type,
        float& clock,
        int& sampleIndex,
        int& syncIndex,
        int& clockIndex,
        int& viterbiCost
    ) const
    {
        m_m17DemodProcessor.getDiagnostics(
            dcd,
            evm,
            deviation,
            offset,
            status,
            sync_word_type,
            clock,
            sampleIndex,
            syncIndex,
            clockIndex,
            viterbiCost
        );
    }

    void getBERT(uint32_t& bertErrors, uint32_t& bertBits) {
        m_m17DemodProcessor.getBERT(bertErrors, bertBits);
    }

    void resetPRBS() { m_m17DemodProcessor.resetPRBS(); }

    uint32_t getLSFCount() const { return m_m17DemodProcessor.getLSFCount(); }
    const QString& getSrcCall() const { return m_m17DemodProcessor.getSrcCall(); }
    const QString& getDestcCall() const { return m_m17DemodProcessor.getDestcCall(); }
    const QString& getTypeInfo() const { return m_m17DemodProcessor.getTypeInfo(); }
    const std::array<uint8_t, 14>& getMeta() const { return m_m17DemodProcessor.getMeta(); }
    bool getHasGNSS() const { return m_m17DemodProcessor.getHasGNSS(); }
    bool getStreamElsePacket() const { return m_m17DemodProcessor.getStreamElsePacket(); }
    uint16_t getCRC() const { return m_m17DemodProcessor.getCRC(); }
    int getStdPacketProtocol() const { return (int) m_m17DemodProcessor.getStdPacketProtocol(); }
    void setDemodInputMessageQueue(MessageQueue *messageQueue) { m_m17DemodProcessor.setDemodInputMessageQueue(messageQueue); }

private:
    struct MagSqLevelsStore
    {
        MagSqLevelsStore() :
            m_magsq(1e-12),
            m_magsqPeak(1e-12)
        {}
        double m_magsq;
        double m_magsqPeak;
    };

	enum RateState {
		RSInitialFill,
		RSRunning
	};

    int m_channelSampleRate;
	int m_channelFrequencyOffset;
	M17DemodSettings m_settings;
    ChannelAPI *m_channel;
    int m_audioSampleRate;
    QVector<qint16> m_demodBuffer;
    int m_demodBufferFill;

	NCO m_nco;
	Interpolator m_interpolator;
	Real m_interpolatorDistance;
	Real m_interpolatorDistanceRemain;
	int m_sampleCount;
	int m_squelchCount;
	int m_squelchGate;
	double m_squelchLevel;
	bool m_squelchOpen;
    bool m_squelchWasOpen;
    DoubleBufferFIFO<Real> m_squelchDelayLine;

    MovingAverageUtil<Real, double, 16> m_movingAverage;
    double m_magsq;
    double m_magsqSum;
    double m_magsqPeak;
    int  m_magsqCount;
    MagSqLevelsStore m_magSqLevelStore;

	SampleVector m_scopeSampleBuffer;
	AudioVector m_audioBuffer;
	FixReal *m_sampleBuffer; //!< samples ring buffer
	int m_sampleBufferIndex;
	int m_scaleFromShort;

	AudioFifo m_audioFifo;
	BasebandSampleSink* m_scopeXY;
	bool m_scopeEnabled;

    float m_latitude;
    float m_longitude;

    PhaseDiscriminators m_phaseDiscri;

    M17DemodProcessor m_m17DemodProcessor;
};

#endif // INCLUDE_DSDDEMODSINK_H
