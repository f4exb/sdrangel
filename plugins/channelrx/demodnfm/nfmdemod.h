///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
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

#ifndef INCLUDE_NFMDEMOD_H
#define INCLUDE_NFMDEMOD_H

#include <QMutex>
#include <vector>

#include "dsp/basebandsamplesink.h"
#include "channel/channelsinkapi.h"
#include "dsp/phasediscri.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/lowpass.h"
#include "dsp/bandpass.h"
#include "dsp/afsquelch.h"
#include "dsp/agc.h"
#include "dsp/ctcssdetector.h"
#include "audio/audiofifo.h"
#include "util/message.h"

#include "nfmdemodsettings.h"

class DeviceSourceAPI;
class ThreadedBasebandSampleSink;
class DownChannelizer;

class NFMDemod : public BasebandSampleSink, public ChannelSinkAPI {
public:
    class MsgConfigureNFMDemod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const NFMDemodSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureNFMDemod* create(const NFMDemodSettings& settings, bool force)
        {
            return new MsgConfigureNFMDemod(settings, force);
        }

    private:
        NFMDemodSettings m_settings;
        bool m_force;

        MsgConfigureNFMDemod(const NFMDemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    class MsgConfigureChannelizer : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getSampleRate() const { return m_sampleRate; }
        int getCenterFrequency() const { return m_centerFrequency; }

        static MsgConfigureChannelizer* create(int sampleRate, int centerFrequency)
        {
            return new MsgConfigureChannelizer(sampleRate, centerFrequency);
        }

    private:
        int m_sampleRate;
        int  m_centerFrequency;

        MsgConfigureChannelizer(int sampleRate, int centerFrequency) :
            Message(),
            m_sampleRate(sampleRate),
            m_centerFrequency(centerFrequency)
        { }
    };

    class MsgReportCTCSSFreq : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        Real getFrequency() const { return m_freq; }

        static MsgReportCTCSSFreq* create(Real freq)
        {
            return new MsgReportCTCSSFreq(freq);
        }

    private:
        Real m_freq;

        MsgReportCTCSSFreq(Real freq) :
            Message(),
            m_freq(freq)
        { }
    };

    NFMDemod(DeviceSourceAPI *deviceAPI);
	~NFMDemod();
	virtual void destroy() { delete this; }

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool po);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& cmd);

    virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual void getTitle(QString& title) { title = m_settings.m_title; }
    virtual qint64 getCenterFrequency() const { return m_settings.m_inputFrequencyOffset; }

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual int webapiSettingsGet(
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage);

    virtual int webapiSettingsPutPatch(
            bool force,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage);

	const Real *getCtcssToneSet(int& nbTones) const {
		nbTones = m_ctcssDetector.getNTones();
		return m_ctcssDetector.getToneSet();
	}

	void setSelectedCtcssIndex(int selectedCtcssIndex) {
		m_ctcssIndexSelected = selectedCtcssIndex;
	}

	Real getMag() { return m_magsq; }
	bool getSquelchOpen() const { return m_squelchOpen; }

    void getMagSqLevels(double& avg, double& peak, int& nbSamples)
    {
        avg = m_magsqCount == 0 ? 1e-10 : m_magsqSum / m_magsqCount;
        m_magsq = avg;
        peak = m_magsqPeak == 0.0 ? 1e-10 : m_magsqPeak;
        nbSamples = m_magsqCount == 0 ? 1 : m_magsqCount;
        m_magsqSum = 0.0f;
        m_magsqPeak = 0.0f;
        m_magsqCount = 0;
    }

    static const QString m_channelIdURI;
    static const QString m_channelId;

private:
	enum RateState {
		RSInitialFill,
		RSRunning
	};

    DeviceSourceAPI* m_deviceAPI;
    ThreadedBasebandSampleSink* m_threadedChannelizer;
    DownChannelizer* m_channelizer;

    int m_inputSampleRate;
    int m_inputFrequencyOffset;
	NFMDemodSettings m_settings;

	NCO m_nco;
	Interpolator m_interpolator;
	Real m_interpolatorDistance;
	Real m_interpolatorDistanceRemain;
	Lowpass<Real> m_lowpass;
	Bandpass<Real> m_bandpass;
	CTCSSDetector m_ctcssDetector;
	int m_ctcssIndex; // 0 for nothing detected
	int m_ctcssIndexSelected;
	int m_sampleCount;
	int m_squelchCount;
	int m_squelchGate;
	bool m_audioMute;

	Real m_squelchLevel;
	bool m_squelchOpen;
	bool m_afSquelchOpen;
	double m_magsq; //!< displayed averaged value
	double m_magsqSum;
	double m_magsqPeak;
    int  m_magsqCount;

	Real m_lastArgument;
	//Complex m_m1Sample;
	//Complex m_m2Sample;
	MovingAverage<double> m_movingAverage;
	AFSquelch m_afSquelch;
	Real m_agcLevel; // AGC will aim to  this level
	Real m_agcFloor; // AGC will not go below this level

	Real m_fmExcursion;
	//Real m_fmScaling;

	AudioVector m_audioBuffer;
	uint m_audioBufferFill;

	AudioFifo m_audioFifo;
    UDPSink<qint16> *m_udpBufferAudio;

	QMutex m_settingsMutex;

    PhaseDiscriminators m_phaseDiscri;

    static const int m_udpBlockSize;

//    void apply(bool force = false);
    void applyChannelSettings(int inputSampleRate, int inputFrequencyOffset, bool force = false);
    void applySettings(const NFMDemodSettings& settings, bool force = false);
    void webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const NFMDemodSettings& settings);
};

#endif // INCLUDE_NFMDEMOD_H
