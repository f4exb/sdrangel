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

#include "gnuradiogui.h"
#include "ui_gnuradiogui.h"

#include <osmosdr/device.h>
#include <iostream>
#include <plugin/pluginapi.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>

GNURadioGui::GNURadioGui(PluginAPI* pluginAPI, QWidget* parent) :
	QWidget(parent),
	ui(new Ui::GNURadioGui),
	m_pluginAPI(pluginAPI),
	m_settings(),
	m_sampleSource(NULL)
{
	ui->setupUi(this);
	connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
	displaySettings();

	m_sampleSource = new GNURadioInput(m_pluginAPI->getMainWindowMessageQueue());
	m_pluginAPI->setSampleSource(m_sampleSource);
}

GNURadioGui::~GNURadioGui()
{
	delete ui;
}

void GNURadioGui::destroy()
{
	delete this;
}

void GNURadioGui::setName(const QString& name)
{
	setObjectName(name);
}

void GNURadioGui::resetToDefaults()
{
	m_generalSettings.resetToDefaults();
	m_settings.resetToDefaults();
	displaySettings();
	sendSettings();
}

QByteArray GNURadioGui::serializeGeneral() const
{
	return m_generalSettings.serialize();
}

bool GNURadioGui::deserializeGeneral(const QByteArray&data)
{
	if(m_generalSettings.deserialize(data)) {
		displaySettings();
		sendSettings();
		return true;
	} else {
		resetToDefaults();
		return false;
	}
}

quint64 GNURadioGui::getCenterFrequency() const
{
	return m_generalSettings.m_centerFrequency;
}

QByteArray GNURadioGui::serialize() const
{
	return m_settings.serialize();
}

bool GNURadioGui::deserialize(const QByteArray& data)
{
	if(m_settings.deserialize(data)) {
		displaySettings();
		sendSettings();
		return true;
	} else {
		resetToDefaults();
		return false;
	}
}

bool GNURadioGui::handleMessage(Message* message)
{
	if(GNURadioInput::MsgReportGNURadio::match(message)) {
		GNURadioInput::MsgReportGNURadio* rep = (GNURadioInput::MsgReportGNURadio*)message;
		m_namedGains = rep->getNamedGains();
		m_freqMin = rep->getFreqMin();
		m_freqMax = rep->getFreqMax();
		m_freqCorr = rep->getFreqCorr();
		m_sampRates = rep->getSampRates();
		m_antennas = rep->getAntennas();
		m_dcoffs = rep->getDCOffs();
		m_iqbals = rep->getIQBals();
		m_bandwidths = rep->getBandwidths();
		/* insert 0 which will become "Auto" in the combo box */
		m_bandwidths.insert(m_bandwidths.begin(), 0);
		displaySettings();
		return true;
	} else {
		return false;
	}
}

void GNURadioGui::displaySettings()
{
	int oldIndex = 0;

	oldIndex = ui->cboDevices->currentIndex();
	ui->cboDevices->clear();

	QString oldArgs = ui->txtDeviceArgs->text();

	osmosdr::devices_t devices = osmosdr::device::find();

	for ( int i = 0; i < devices.size(); i++ )
	{
		osmosdr::device_t dev = devices[i];

		QString label;

		if ( dev.count( "label" ) )
		{
			label = QString(dev[ "label" ].c_str());
			dev.erase("label");
		}

		QPair< QString, QString > pair(label, dev.to_string().c_str());
		m_devs.append(pair);

		ui->cboDevices->addItem(label);
	}

	if ( ui->cboDevices->count() && oldIndex >= 0 )
	{
		if ( oldIndex > ui->cboDevices->count() - 1 )
			oldIndex = 0;

		ui->cboDevices->setCurrentIndex(oldIndex);

		if ( oldArgs.length() == 0 )
			ui->txtDeviceArgs->setText( m_devs[oldIndex].second );
	}

	if ( oldArgs.length() )
		ui->txtDeviceArgs->setText( oldArgs );

	ui->centerFrequency->setValueRange(7,
					   unsigned(m_freqMin / 1000.0),
					   unsigned(m_freqMax / 1000.0));

	ui->centerFrequency->setValue(m_generalSettings.m_centerFrequency / 1000);

	ui->sldFreqCorr->setRange(-100, +100);
	ui->sldFreqCorr->setValue( m_freqCorr );
	ui->lblFreqCorr->setText(tr("%1").arg(ui->sldFreqCorr->value()));

	m_gainControls.clear();
	QVBoxLayout *layoutGains = ui->verticalLayoutGains;
	QLayoutItem *layoutItem;

	while ( ( layoutItem = layoutGains->takeAt( 0 ) ) != NULL )
	{
		QLayout *layout = layoutItem->layout();

		if ( !layout )
			continue;

		while ( ( layoutItem = layout->takeAt( 0 ) ) != NULL )
		{
			delete layoutItem->widget();
			delete layoutItem;
		}

		delete layout;
	}

	for ( int i = 0; i < m_namedGains.size(); i++ )
	{
		std::pair< QString, std::vector<double> > pair = m_namedGains[i];

		QHBoxLayout *layout = new QHBoxLayout();
		QLabel *gainName = new QLabel( pair.first + " Gain" );
		QSlider *gainSlider = new QSlider(Qt::Horizontal);
		QLabel *gainLabel = new QLabel("0");
		gainLabel->setMinimumWidth(30);
		gainLabel->setAlignment(Qt::AlignHCenter | Qt::AlignHCenter);

		QPair< QSlider*, QLabel* > pair2( gainSlider, gainLabel );
		m_gainControls.push_back( pair2 );

		connect(gainSlider, SIGNAL(valueChanged(int)),
			this, SLOT(on_sldGain_valueChanged(int)));

		layout->addWidget(gainName);
		layout->addWidget(gainSlider);
		layout->addWidget(gainLabel);

		layoutGains->addLayout(layout);

		std::vector<double> gain_values = pair.second;

		if ( gain_values.size() ) {
			gainSlider->setRange(0, gain_values.size() - 1);
			gainSlider->setValue(gain_values.size() / 4);
			gainSlider->setEnabled(true);
		} else {
			gainSlider->setEnabled(false);
		}
	}

	oldIndex = ui->cboSampleRate->currentIndex();
	ui->cboSampleRate->clear();

	for ( int i = 0; i < m_sampRates.size(); i++ )
		ui->cboSampleRate->addItem( QString::number(m_sampRates[i] / 1e6) );

	if ( oldIndex > ui->cboSampleRate->count() - 1 )
		oldIndex = 0;

	if ( ui->cboSampleRate->count() && oldIndex >= 0 )
		ui->cboSampleRate->setCurrentIndex(oldIndex);

	if ( ui->cboSampleRate->count() ) {
		ui->cboSampleRate->setEnabled(true);
	} else {
		ui->cboSampleRate->setEnabled(false);
	}

	oldIndex = ui->cboAntennas->currentIndex();
	ui->cboAntennas->clear();

	if ( m_antennas.size() ) {
		for ( int i = 0; i < m_antennas.size(); i++ )
			ui->cboAntennas->addItem( m_antennas[i] );

		if ( oldIndex > ui->cboAntennas->count() - 1 )
			oldIndex = 0;

		if ( ui->cboAntennas->count() && oldIndex >= 0 )
			ui->cboAntennas->setCurrentIndex(oldIndex);

		ui->cboAntennas->setEnabled(true);
	} else {
		ui->cboAntennas->setEnabled(false);
	}

	oldIndex = ui->cboDCOffset->currentIndex();
	ui->cboDCOffset->clear();

	if ( m_dcoffs.size() ) {
		for ( int i = 0; i < m_dcoffs.size(); i++ )
			ui->cboDCOffset->addItem( m_dcoffs[i] );

		if ( ui->cboDCOffset->count() && oldIndex >= 0 )
			ui->cboDCOffset->setCurrentIndex(oldIndex);

		ui->cboDCOffset->setEnabled(true);
	} else {
		ui->cboDCOffset->setEnabled(false);
	}

	oldIndex = ui->cboIQBalance->currentIndex();
	ui->cboIQBalance->clear();

	if ( m_iqbals.size() ) {
		for ( int i = 0; i < m_iqbals.size(); i++ )
			ui->cboIQBalance->addItem( m_iqbals[i] );

		if ( ui->cboIQBalance->count() && oldIndex >= 0 )
			ui->cboIQBalance->setCurrentIndex(oldIndex);

		ui->cboIQBalance->setEnabled(true);
	} else {
		ui->cboIQBalance->setEnabled(false);
	}

	oldIndex = ui->cboBandwidth->currentIndex();
	ui->cboBandwidth->clear();

	for ( int i = 0; i < m_bandwidths.size(); i++ )
		if ( 0.0 == m_bandwidths[i] )
			ui->cboBandwidth->addItem( "Auto" );
		else
			ui->cboBandwidth->addItem( QString::number(m_bandwidths[i] / 1e6) );

	if ( oldIndex > ui->cboBandwidth->count() - 1 )
		oldIndex = 0;

	if ( ui->cboBandwidth->count() && oldIndex >= 0 )
		ui->cboBandwidth->setCurrentIndex(oldIndex);

	if ( ui->cboBandwidth->count() ) {
		ui->cboBandwidth->setEnabled(true);
	} else {
		ui->cboBandwidth->setEnabled(false);
	}
}

void GNURadioGui::sendSettings()
{
	if(!m_updateTimer.isActive())
		m_updateTimer.start(100);
}

void GNURadioGui::updateHardware()
{
	m_updateTimer.stop();
	GNURadioInput::MsgConfigureGNURadio* msg = GNURadioInput::MsgConfigureGNURadio::create(m_generalSettings, m_settings);
	msg->submit(m_pluginAPI->getDSPEngineMessageQueue());
}

void GNURadioGui::on_cboDevices_currentIndexChanged(int index)
{
	if ( index < 0 || index >= m_devs.count() )
		return;

	ui->txtDeviceArgs->setText( m_devs[index].second );
}

void GNURadioGui::on_txtDeviceArgs_textChanged(const QString &arg1)
{
	m_settings.m_args = arg1;
	sendSettings();
}

void GNURadioGui::on_centerFrequency_changed(quint64 value)
{
	m_generalSettings.m_centerFrequency = value * 1000;
	sendSettings();
}

void GNURadioGui::on_sldFreqCorr_valueChanged(int value)
{
	ui->lblFreqCorr->setText(tr("%1").arg(value));
	m_settings.m_freqCorr = value;
	sendSettings();
}

void GNURadioGui::on_sldGain_valueChanged(int value)
{
	m_settings.m_namedGains.clear();

	for ( int i = 0; i < m_gainControls.size(); i++ )
	{
		QPair< QSlider*, QLabel* > controls = m_gainControls[i];

		QSlider *slider = controls.first;
		QLabel *label = controls.second;

		std::pair< QString, std::vector<double> > named_gain = m_namedGains[ i ];

		int index = slider->value();
		double gain = named_gain.second[index];
		label->setText(tr("%1").arg(gain));

		QPair< QString, double > named_gain2( named_gain.first, gain );
		m_settings.m_namedGains.push_back( named_gain2 );
	}

	sendSettings();
}

void GNURadioGui::on_cboSampleRate_currentIndexChanged(int index)
{
	if ( index < 0 || index >= m_sampRates.size() )
		return;

	m_settings.m_sampRate = m_sampRates[index];
	sendSettings();
}

void GNURadioGui::on_cboAntennas_currentIndexChanged(const QString &arg1)
{
	m_settings.m_antenna = arg1;
	sendSettings();
}

void GNURadioGui::on_cboDCOffset_currentIndexChanged(const QString &arg1)
{
	m_settings.m_dcoff = arg1;
	sendSettings();
}

void GNURadioGui::on_cboIQBalance_currentIndexChanged(const QString &arg1)
{
	m_settings.m_iqbal = arg1;
	sendSettings();
}

void GNURadioGui::on_cboBandwidth_currentIndexChanged(int index)
{
	if ( index < 0 || index >= m_bandwidths.size() )
		return;

	m_settings.m_bandwidth = m_bandwidths[index];
	sendSettings();
}
