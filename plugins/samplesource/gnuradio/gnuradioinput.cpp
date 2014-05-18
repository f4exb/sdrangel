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

#include <string.h>
#include <errno.h>
#include "util/simpleserializer.h"
#include "gnuradioinput.h"
#include "gnuradiothread.h"
#include "gnuradiogui.h"

MESSAGE_CLASS_DEFINITION(GNURadioInput::MsgConfigureGNURadio, Message)
MESSAGE_CLASS_DEFINITION(GNURadioInput::MsgReportGNURadio, Message)

GNURadioInput::Settings::Settings() :
	m_args(""),
	m_freqCorr(0),
	m_sampRate(0),
	m_antenna(""),
	m_dcoff(""),
	m_iqbal(""),
	m_bandwidth(0)
{
}

void GNURadioInput::Settings::resetToDefaults()
{
	m_args = "";
	m_sampRate = 0;
	m_freqCorr = 0;
	m_antenna = "";
	m_dcoff = "";
	m_iqbal = "";
	m_bandwidth = 0;
}

QByteArray GNURadioInput::Settings::serialize() const
{
	SimpleSerializer s(1);
//	s.writeString(1, m_args);
//	s.writeDouble(2, m_freqCorr);
//	s.writeDouble(3, m_sampRate);
//	s.writeString(4, m_antenna);
//	s.writeString(5, m_dcoff);
//	s.writeString(5, m_iqbal);
	return s.final();
}

bool GNURadioInput::Settings::deserialize(const QByteArray& data)
{
	SimpleDeserializer d(data);

	if(!d.isValid()) {
		resetToDefaults();
		return false;
	}

	if(d.getVersion() == 1) {
//		d.readString(1, &m_args, "");
//		d.readDouble(2, &m_freqCorr, 0);
//		d.readDouble(3, &m_sampRate, 0);
//		d.readString(4, &m_antenna, "");
//		d.readString(5, &m_dcoff, "");
//		d.readString(5, &m_iqbal, "");
		return true;
	} else {
		resetToDefaults();
		return false;
	}
}

GNURadioInput::GNURadioInput(MessageQueue* msgQueueToGUI) :
	SampleSource(msgQueueToGUI),
	m_settings(),
	m_GnuradioThread(NULL),
	m_deviceDescription()
{
}

GNURadioInput::~GNURadioInput()
{
	stopInput();
}

bool GNURadioInput::startInput(int device)
{
	double freqMin = 0, freqMax = 0, freqCorr = 0;
	std::vector< std::pair< QString, std::vector<double> > > namedGains;
	std::vector< double > sampRates;
	std::vector< QString > antennas;
	std::vector< double > bandwidths;

	QMutexLocker mutexLocker(&m_mutex);

	if(m_GnuradioThread != NULL)
		stopInput();

	if(!m_sampleFifo.setSize( 2 * 1024 * 1024 )) {
		qCritical("Could not allocate SampleFifo");
		return false;
	}

	m_deviceDescription = m_settings.m_args;

	// pass device arguments from the gui
	m_GnuradioThread = new GnuradioThread(m_settings.m_args, &m_sampleFifo);
	if(m_GnuradioThread == NULL) {
		qFatal("out of memory");
		goto failed;
	}
	m_GnuradioThread->startWork();

	mutexLocker.unlock();
	applySettings(m_generalSettings, m_settings, true);

	if(m_GnuradioThread != NULL) {
		osmosdr::source::sptr radio = m_GnuradioThread->radio();

		try {
			osmosdr::freq_range_t freq_rage = radio->get_freq_range();
			freqMin = freq_rage.start();
			freqMax = freq_rage.stop();
		} catch ( std::exception &ex ) {
			qDebug("%s", ex.what());
		}

		freqCorr = radio->get_freq_corr();

		namedGains.clear();
		m_settings.m_namedGains.clear();
		std::vector< std::string > gain_names = radio->get_gain_names();
		for ( int i = 0; i < gain_names.size(); i++ )
		{
			std::string gain_name = gain_names[i];

			try {
				std::vector< double > gain_values = \
						radio->get_gain_range( gain_name ).values();

				std::pair< QString, std::vector<double> > pair( gain_name.c_str(),
										gain_values );

				namedGains.push_back( pair );

				QPair< QString, double > pair2( gain_name.c_str(), 0 );

				m_settings.m_namedGains.push_back( pair2 );
			} catch ( std::exception &ex ) {
				qDebug("%s", ex.what());
			}
		}

		try {
			sampRates = radio->get_sample_rates().values();
		} catch ( std::exception &ex ) {
			qDebug("%s", ex.what());
		}

		antennas.clear();
		std::vector< std::string > ant = radio->get_antennas();
		for ( int i = 0; i < ant.size(); i++ )
			antennas.push_back( QString( ant[i].c_str() ) );

		m_dcoffs.clear();
		m_dcoffs.push_back( "Off" );
		m_dcoffs.push_back( "Keep" );
		m_dcoffs.push_back( "Auto" );

		m_iqbals.clear();
		m_iqbals.push_back( "Off" );
		m_iqbals.push_back( "Keep" );
		m_iqbals.push_back( "Auto" );

		try {
			bandwidths = radio->get_bandwidth_range().values();
		} catch ( std::exception &ex ) {
			qDebug("%s", ex.what());
		}
	}

	qDebug("GnuradioInput: start");
	MsgReportGNURadio::create(freqMin, freqMax, freqCorr, namedGains,
				  sampRates, antennas, m_dcoffs, m_iqbals,
				  bandwidths)
			->submit(m_guiMessageQueue);

	return true;

failed:
	stopInput();
	return false;
}

void GNURadioInput::stopInput()
{
	QMutexLocker mutexLocker(&m_mutex);

	if(m_GnuradioThread != NULL) {
		m_GnuradioThread->stopWork();
		delete m_GnuradioThread;
		m_GnuradioThread = NULL;
	}

	m_deviceDescription.clear();
}

const QString& GNURadioInput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int GNURadioInput::getSampleRate() const
{
	return m_settings.m_sampRate;
}

quint64 GNURadioInput::getCenterFrequency() const
{
	return m_generalSettings.m_centerFrequency;
}

bool GNURadioInput::handleMessage(Message* message)
{
	if(MsgConfigureGNURadio::match(message)) {
		MsgConfigureGNURadio* conf = (MsgConfigureGNURadio*)message;
		if(!applySettings(conf->getGeneralSettings(), conf->getSettings(), false))
			qDebug("Gnuradio config error");
		return true;
	} else {
		return false;
	}
}

bool GNURadioInput::applySettings(const GeneralSettings& generalSettings,
				  const Settings& settings,
				  bool force)
{
	QMutexLocker mutexLocker(&m_mutex);

	m_settings.m_args = settings.m_args;

	if ( NULL == m_GnuradioThread )
		return true;

	osmosdr::source::sptr radio = m_GnuradioThread->radio();

	try {

		if((m_settings.m_freqCorr != settings.m_freqCorr) || force) {
			m_settings.m_freqCorr = settings.m_freqCorr;
			radio->set_freq_corr( m_settings.m_freqCorr );
		}

		if((m_generalSettings.m_centerFrequency != generalSettings.m_centerFrequency) || force) {
			m_generalSettings.m_centerFrequency = generalSettings.m_centerFrequency;
			radio->set_center_freq( m_generalSettings.m_centerFrequency );
		}

		for ( int i = 0; i < settings.m_namedGains.size(); i++ )
		{
			if((m_settings.m_namedGains[i].second != settings.m_namedGains[i].second) || force) {
				m_settings.m_namedGains[i].second = settings.m_namedGains[i].second;
				radio->set_gain( settings.m_namedGains[i].second,
						 settings.m_namedGains[i].first.toStdString() );
			}
		}

		if((m_settings.m_sampRate != settings.m_sampRate) || force) {
			m_settings.m_sampRate = settings.m_sampRate;
			radio->set_sample_rate( m_settings.m_sampRate );
		}

		if((m_settings.m_antenna != settings.m_antenna) || force) {
			m_settings.m_antenna = settings.m_antenna;
			radio->set_antenna( m_settings.m_antenna.toStdString() );
		}

		if((m_settings.m_dcoff != settings.m_dcoff) || force) {
			m_settings.m_dcoff = settings.m_dcoff;

			for ( int i = 0; i < m_dcoffs.size(); i++ )
			{
				if ( m_dcoffs[i] !=  m_settings.m_dcoff )
					continue;

				radio->set_dc_offset_mode( i );
				break;
			}
		}

		if((m_settings.m_iqbal != settings.m_iqbal) || force) {
			m_settings.m_iqbal = settings.m_iqbal;

			for ( int i = 0; i < m_iqbals.size(); i++ )
			{
				if ( m_iqbals[i] !=  m_settings.m_iqbal )
					continue;

				radio->set_iq_balance_mode( i );
				break;
			}
		}

		if((m_settings.m_bandwidth != settings.m_bandwidth) ||
				(0.0f == settings.m_bandwidth) || force) {
			m_settings.m_bandwidth = settings.m_bandwidth;
			/* setting the BW to 0.0 triggers automatic bandwidth
			 * selection when supported by device */
			radio->set_bandwidth( m_settings.m_bandwidth );
		}

	} catch ( std::exception &ex ) {
		qDebug("%s", ex.what());
		return false;
	}

	return true;
}
