///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2013 by Dimitri Stolnikov <horiz0n@gmx.net>                     //
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

#ifndef INCLUDE_GNURADIOINPUT_H
#define INCLUDE_GNURADIOINPUT_H

#include "dsp/samplesource/samplesource.h"
#include <QString>
#include <QPair>

class GnuradioThread;

class GNURadioInput : public SampleSource {
public:
	struct Settings {
		QString m_args;
		double m_freqMin;
		double m_freqMax;
		double m_freqCorr;
		QList< QPair< QString, double > > m_namedGains;
		double m_sampRate;
		QString m_antenna;
		QString m_dcoff;
		QString m_iqbal;
		double m_bandwidth;

		Settings();
		void resetToDefaults();
		QByteArray serialize() const;
		bool deserialize(const QByteArray& data);
	};

	class MsgConfigureGNURadio : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const GeneralSettings& getGeneralSettings() const { return m_generalSettings; }
		const Settings& getSettings() const { return m_settings; }

		static MsgConfigureGNURadio* create(const GeneralSettings& generalSettings, const Settings& settings)
		{
			return new MsgConfigureGNURadio(generalSettings, settings);
		}

	protected:
		GeneralSettings m_generalSettings;
		Settings m_settings;

		MsgConfigureGNURadio(const GeneralSettings& generalSettings, const Settings& settings) :
			Message(),
			m_generalSettings(generalSettings),
			m_settings(settings)
		{ }
	};

	class MsgReportGNURadio : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const std::vector< std::pair< QString, std::vector<double> > >& getNamedGains() const { return m_namedGains; }
		const double getFreqMin() const { return m_freqMin; }
		const double getFreqMax() const { return m_freqMax; }
		const double getFreqCorr() const { return m_freqCorr; }
		const std::vector<double>& getSampRates() const { return m_sampRates; }
		const std::vector<QString>& getAntennas() const { return m_antennas; }
		const std::vector<QString>& getDCOffs() const { return m_dcoffs; }
		const std::vector<QString>& getIQBals() const { return m_iqbals; }
		const std::vector<double>& getBandwidths() const { return m_bandwidths; }

		static MsgReportGNURadio* create(const double freqMin,
						 const double freqMax,
						 const double freqCorr,
						 const std::vector< std::pair< QString, std::vector<double> > >& namedGains,
						 const std::vector<double>& sampRates,
						 const std::vector<QString>& antennas,
						 const std::vector<QString>& dcoffs,
						 const std::vector<QString>& iqbals,
						 const std::vector<double>& bandwidths)
		{
			return new MsgReportGNURadio(freqMin, freqMax, freqCorr, namedGains, sampRates, antennas, dcoffs, iqbals, bandwidths);
		}

	protected:
		double m_freqMin;
		double m_freqMax;
		double m_freqCorr;
		std::vector< std::pair< QString, std::vector<double> > > m_namedGains;
		std::vector<double> m_sampRates;
		std::vector<QString> m_antennas;
		std::vector<QString> m_dcoffs;
		std::vector<QString> m_iqbals;
		std::vector<double> m_bandwidths;

		MsgReportGNURadio(const double freqMin,
				  const double freqMax,
				  const double freqCorr,
				  const std::vector< std::pair< QString, std::vector<double> > >& namedGains,
				  const std::vector<double>& sampRates,
				  const std::vector<QString>& antennas,
				  const std::vector<QString>& dcoffs,
				  const std::vector<QString>& iqbals,
				  const std::vector<double>& bandwidths) :
			Message(),
			m_freqMin(freqMin),
			m_freqMax(freqMax),
			m_freqCorr(freqCorr),
			m_namedGains(namedGains),
			m_sampRates(sampRates),
			m_antennas(antennas),
			m_dcoffs(dcoffs),
			m_iqbals(iqbals),
			m_bandwidths(bandwidths)
		{ }
	};

	GNURadioInput(MessageQueue* msgQueueToGUI);
	~GNURadioInput();

	bool startInput(int device);
	void stopInput();

	const QString& getDeviceDescription() const;
	int getSampleRate() const;
	quint64 getCenterFrequency() const;

	bool handleMessage(Message* message);

private:
	QMutex m_mutex;
	Settings m_settings;
	GnuradioThread* m_GnuradioThread;
	QString m_deviceDescription;
	std::vector< QString > m_dcoffs;
	std::vector< QString > m_iqbals;

	bool applySettings(const GeneralSettings& generalSettings,
			   const Settings& settings,
			   bool force);
};

#endif // INCLUDE_GNURADIOINPUT_H
