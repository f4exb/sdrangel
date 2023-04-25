///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_UDPSINKSINK_H
#define INCLUDE_UDPSINKSINK_H

#include <QObject>

#include "dsp/channelsamplesink.h"
#include "dsp/nco.h"
#include "dsp/fftfilt.h"
#include "dsp/interpolator.h"
#include "dsp/phasediscri.h"
#include "dsp/movingaverage.h"
#include "dsp/agc.h"
#include "dsp/firfilter.h"
#include "util/udpsinkutil.h"
#include "audio/audiofifo.h"

#include "udpsinksettings.h"

class QUdpSocket;
class BasebandSampleSink;

class UDPSinkSink : public QObject, public ChannelSampleSink {
    Q_OBJECT
public:
    UDPSinkSink();
	~UDPSinkSink();

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = true);
    void applySettings(const UDPSinkSettings& settings, bool force = false);

    AudioFifo *getAudioFifo() { return &m_audioFifo; }
    void setAudioFifoLabel(const QString& label) { m_audioFifo.setLabel(label); }
	void setSpectrum(BasebandSampleSink* spectrum) { m_spectrum = spectrum; }
    void enableSpectrum(bool enable) { m_spectrumEnabled = enable; }
    void setSpectrumPositiveOnly(bool positiveOnly) { m_spectrumPositiveOnly = positiveOnly; }
	double getMagSq() const { return m_magsq; }
	double getInMagSq() const { return m_inMagsq; }
	bool getSquelchOpen() const { return m_squelchOpen; }

	static const int udpBlockSize = 512; // UDP block size in number of bytes

private slots:
    void audioReadyRead();

private:
	struct Sample16
	{
	    Sample16() : m_r(0), m_i(0) {}
	    Sample16(int16_t r, int16_t i) : m_r(r), m_i(i) {}
	    int16_t m_r;
	    int16_t m_i;
	};

    struct Sample24
    {
	    Sample24() : m_r(0), m_i(0) {}
	    Sample24(int32_t r, int32_t i) : m_r(r), m_i(i) {}
        int32_t m_r;
        int32_t m_i;
    };

    int m_channelSampleRate;
    int m_channelFrequencyOffset;
    UDPSinkSettings m_settings;

	QUdpSocket *m_audioSocket;

	double m_magsq;
    double m_inMagsq;
    MovingAverage<double> m_outMovingAverage;
    MovingAverage<double> m_inMovingAverage;
    MovingAverage<double> m_amMovingAverage;

	Real m_scale;
	Complex m_last, m_this;

	NCO m_nco;
	Interpolator m_interpolator;
	Real m_sampleDistanceRemain;
	fftfilt* UDPFilter;

	SampleVector m_sampleBuffer;
	UDPSinkUtil<Sample16> *m_udpBuffer16;
	UDPSinkUtil<int16_t> *m_udpBufferMono16;
    UDPSinkUtil<Sample24> *m_udpBuffer24;

	AudioVector m_audioBuffer;
	std::size_t m_audioBufferFill;
	AudioFifo m_audioFifo;

	BasebandSampleSink* m_spectrum;
	bool m_spectrumEnabled;
    bool m_spectrumPositiveOnly;

	quint32 m_nextSSBId;
	quint32 m_nextS16leId;

	char *m_udpAudioBuf;
	static const int m_udpAudioPayloadSize = 8192; //!< UDP audio samples buffer. No UDP block on Earth is larger than this
    static const Real m_agcTarget;

    PhaseDiscriminators m_phaseDiscri;

    double m_squelch;
    bool m_squelchOpen;
    int  m_squelchOpenCount;
    int  m_squelchCloseCount;
    int  m_squelchGate; //!< number of samples computed from given gate
    int  m_squelchRelease;

    MagAGC m_agc;
    Bandpass<double> m_bandpass;

    inline void calculateSquelch(double value)
    {
        if ((!m_settings.m_squelchEnabled) || (value > m_squelch))
        {
            if (m_squelchGate == 0)
            {
                m_squelchOpen = true;
            }
            else
            {
                if (m_squelchOpenCount < m_squelchGate)
                {
                    m_squelchOpenCount++;
                }
                else
                {
                    m_squelchCloseCount = m_squelchRelease;
                    m_squelchOpen = true;
                }
            }
        }
        else
        {
            if (m_squelchGate == 0)
            {
                m_squelchOpen = false;
            }
            else
            {
                if (m_squelchCloseCount > 0)
                {
                    m_squelchCloseCount--;
                }
                else
                {
                    m_squelchOpenCount = 0;
                    m_squelchOpen = false;
                }
            }
        }
    }

    inline void initSquelch(bool open)
    {
        if (open)
        {
            m_squelchOpen = true;
            m_squelchOpenCount = m_squelchGate;
            m_squelchCloseCount = m_squelchRelease;
        }
        else
        {
            m_squelchOpen = false;
            m_squelchOpenCount = 0;
            m_squelchCloseCount = 0;
        }
    }

    void udpWrite(FixReal real, FixReal imag)
    {
        if (SDR_RX_SAMP_SZ == 16)
        {
            if (m_settings.m_sampleFormat == UDPSinkSettings::FormatIQ16) {
                m_udpBuffer16->write(Sample16(real, imag));
            } else if (m_settings.m_sampleFormat == UDPSinkSettings::FormatIQ24) {
                m_udpBuffer24->write(Sample24(real<<8, imag<<8));
            } else {
                m_udpBuffer16->write(Sample16(real, imag));
            }
        }
        else if (SDR_RX_SAMP_SZ == 24)
        {
            if (m_settings.m_sampleFormat == UDPSinkSettings::FormatIQ16) {
                m_udpBuffer16->write(Sample16(real>>8, imag>>8));
            } else if (m_settings.m_sampleFormat == UDPSinkSettings::FormatIQ24) {
                m_udpBuffer24->write(Sample24(real, imag));
            } else {
                m_udpBuffer16->write(Sample16(real>>8, imag>>8));
            }
        }
    }

    void udpWriteMono(FixReal sample)
    {
        if (SDR_RX_SAMP_SZ == 16) {
            m_udpBufferMono16->write(sample);
        } else if (SDR_RX_SAMP_SZ == 24) {
            m_udpBufferMono16->write(sample>>8);
        }
    }

    void udpWriteNorm(Real real, Real imag) {
        m_udpBuffer16->write(Sample16(real*32768.0, imag*32768.0));
    }

    void udpWriteNormMono(Real sample) {
        m_udpBufferMono16->write(sample*32768.0);
    }
};

#endif // INCLUDE_UDPSINKSINK_H
