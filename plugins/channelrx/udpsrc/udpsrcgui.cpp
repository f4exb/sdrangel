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

#include "../../channelrx/udpsrc/udpsrcgui.h"

#include <device/devicesourceapi.h>
#include <dsp/downchannelizer.h>
#include "../../../sdrbase/dsp/threadedbasebandsamplesink.h"
#include "plugin/pluginapi.h"
#include "dsp/spectrumvis.h"
#include "dsp/dspengine.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "gui/basicchannelsettingswidget.h"
#include "ui_udpsrcgui.h"
#include "mainwindow.h"

#include "../../channelrx/udpsrc/udpsrc.h"

const QString UDPSrcGUI::m_channelID = "sdrangel.channel.udpsrc";

UDPSrcGUI* UDPSrcGUI::create(PluginAPI* pluginAPI, DeviceSourceAPI *deviceAPI)
{
	UDPSrcGUI* gui = new UDPSrcGUI(pluginAPI, deviceAPI);
	return gui;
}

void UDPSrcGUI::destroy()
{
	delete this;
}

void UDPSrcGUI::setName(const QString& name)
{
	setObjectName(name);
}

qint64 UDPSrcGUI::getCenterFrequency() const
{
	return m_channelMarker.getCenterFrequency();
}

void UDPSrcGUI::setCenterFrequency(qint64 centerFrequency)
{
	m_channelMarker.setCenterFrequency(centerFrequency);
	applySettings();
}

QString UDPSrcGUI::getName() const
{
	return objectName();
}

void UDPSrcGUI::resetToDefaults()
{
	blockApplySettings(true);

	ui->sampleFormat->setCurrentIndex(0);
	ui->sampleRate->setText("48000");
	ui->rfBandwidth->setText("32000");
	ui->fmDeviation->setText("2500");
	ui->udpAddress->setText("127.0.0.1");
	ui->udpPort->setText("9999");
	ui->audioPort->setText("9999");
	ui->spectrumGUI->resetToDefaults();
	ui->boost->setValue(1);
	ui->volume->setValue(20);
	ui->audioActive->setChecked(false);
	ui->audioStereo->setChecked(false);

	blockApplySettings(false);
	applySettingsImmediate();
	applySettings();
}

QByteArray UDPSrcGUI::serialize() const
{
	SimpleSerializer s(1);
	s.writeBlob(1, saveState());
	s.writeS32(2, m_channelMarker.getCenterFrequency());
	s.writeS32(3, m_sampleFormat);
	s.writeReal(4, m_outputSampleRate);
	s.writeReal(5, m_rfBandwidth);
	s.writeS32(6, m_udpPort);
	s.writeBlob(7, ui->spectrumGUI->serialize());
	s.writeS32(8, (qint32)m_boost);
	s.writeS32(9, m_channelMarker.getCenterFrequency());
	s.writeString(10, m_udpAddress);
	s.writeBool(11, m_audioActive);
	s.writeS32(12, (qint32)m_volume);
	s.writeS32(13, m_audioPort);
	s.writeBool(14, m_audioStereo);
	s.writeS32(15, m_fmDeviation);
	return s.final();
}

bool UDPSrcGUI::deserialize(const QByteArray& data)
{
	SimpleDeserializer d(data);

	if (!d.isValid())
	{
		resetToDefaults();
		return false;
	}

	if (d.getVersion() == 1)
	{
		QByteArray bytetmp;
		QString strtmp;
		qint32 s32tmp;
		Real realtmp;
		bool booltmp;

		blockApplySettings(true);
		m_channelMarker.blockSignals(true);

		d.readBlob(1, &bytetmp);
		restoreState(bytetmp);
		d.readS32(2, &s32tmp, 0);
		m_channelMarker.setCenterFrequency(s32tmp);
		d.readS32(3, &s32tmp, UDPSrc::FormatS16LE);
		switch(s32tmp) {
			case UDPSrc::FormatS16LE:
				ui->sampleFormat->setCurrentIndex(0);
				break;
			case UDPSrc::FormatNFM:
				ui->sampleFormat->setCurrentIndex(1);
				break;
			case UDPSrc::FormatNFMMono:
				ui->sampleFormat->setCurrentIndex(2);
				break;
			case UDPSrc::FormatLSB:
				ui->sampleFormat->setCurrentIndex(3);
				break;
			case UDPSrc::FormatUSB:
				ui->sampleFormat->setCurrentIndex(4);
				break;
			case UDPSrc::FormatLSBMono:
				ui->sampleFormat->setCurrentIndex(5);
				break;
			case UDPSrc::FormatUSBMono:
				ui->sampleFormat->setCurrentIndex(6);
				break;
			case UDPSrc::FormatAMMono:
				ui->sampleFormat->setCurrentIndex(7);
				break;
			default:
				ui->sampleFormat->setCurrentIndex(0);
				break;
		}
		d.readReal(4, &realtmp, 48000);
		ui->sampleRate->setText(QString("%1").arg(realtmp, 0));
		d.readReal(5, &realtmp, 32000);
		ui->rfBandwidth->setText(QString("%1").arg(realtmp, 0));
		d.readS32(6, &s32tmp, 9999);
		ui->udpPort->setText(QString("%1").arg(s32tmp));
		d.readBlob(7, &bytetmp);
		ui->spectrumGUI->deserialize(bytetmp);
		d.readS32(8, &s32tmp, 1);
		ui->boost->setValue(s32tmp);
		d.readS32(9, &s32tmp, 0);
		m_channelMarker.setCenterFrequency(s32tmp);
		d.readString(10, &strtmp, "127.0.0.1");
		ui->udpAddress->setText(strtmp);
		d.readBool(11, &booltmp, false);
		ui->audioActive->setChecked(booltmp);
		d.readS32(12, &s32tmp, 20);
		ui->volume->setValue(s32tmp);
		d.readS32(13, &s32tmp, 9998);
		ui->audioPort->setText(QString("%1").arg(s32tmp));
		d.readBool(14, &booltmp, false);
		ui->audioStereo->setChecked(booltmp);
		d.readS32(15, &s32tmp, 2500);
		ui->fmDeviation->setText(QString("%1").arg(s32tmp));

		blockApplySettings(false);
		m_channelMarker.blockSignals(false);

		applySettingsImmediate();
		applySettings();
		return true;
	}
	else
	{
		resetToDefaults();
		return false;
	}
}

bool UDPSrcGUI::handleMessage(const Message& message)
{
	qDebug() << "UDPSrcGUI::handleMessage";
	return false;
}

void UDPSrcGUI::channelMarkerChanged()
{
	applySettings();
}

void UDPSrcGUI::tick()
{
    double powDb = CalcDb::dbPower(m_udpSrc->getMagSq());
	m_channelPowerDbAvg.feed(powDb);
	ui->channelPower->setText(QString::number(m_channelPowerDbAvg.average(), 'f', 1));
}

UDPSrcGUI::UDPSrcGUI(PluginAPI* pluginAPI, DeviceSourceAPI *deviceAPI, QWidget* parent) :
	RollupWidget(parent),
	ui(new Ui::UDPSrcGUI),
	m_pluginAPI(pluginAPI),
	m_deviceAPI(deviceAPI),
	m_udpSrc(0),
	m_channelMarker(this),
	m_channelPowerDbAvg(40,0),
	m_basicSettingsShown(false),
	m_doApplySettings(true),
	m_boost(1),
	m_volume(20)
{
	ui->setupUi(this);
	connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
	connect(this, SIGNAL(menuDoubleClickEvent()), this, SLOT(onMenuDoubleClicked()));
	setAttribute(Qt::WA_DeleteOnClose, true);

	m_spectrumVis = new SpectrumVis(ui->glSpectrum);
	m_udpSrc = new UDPSrc(m_pluginAPI->getMainWindowMessageQueue(), this, m_spectrumVis);
	m_channelizer = new DownChannelizer(m_udpSrc);
	m_threadedChannelizer = new ThreadedBasebandSampleSink(m_channelizer, this);
	m_deviceAPI->addThreadedSink(m_threadedChannelizer);

	ui->fmDeviation->setEnabled(false);

	ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
	ui->deltaFrequency->setValueRange(7, 0U, 9999999U);

	ui->glSpectrum->setCenterFrequency(0);
	ui->glSpectrum->setSampleRate(ui->sampleRate->text().toInt());
	ui->glSpectrum->setDisplayWaterfall(true);
	ui->glSpectrum->setDisplayMaxHold(true);
	m_spectrumVis->configure(m_spectrumVis->getInputMessageQueue(), 64, 10, FFTWindow::BlackmanHarris);

	ui->glSpectrum->connectTimer(m_pluginAPI->getMainWindow()->getMasterTimer());
	connect(&m_pluginAPI->getMainWindow()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

	//m_channelMarker = new ChannelMarker(this);
	m_channelMarker.setBandwidth(16000);
	m_channelMarker.setCenterFrequency(0);
	m_channelMarker.setColor(Qt::green);
	m_channelMarker.setVisible(true);

	connect(&m_channelMarker, SIGNAL(changed()), this, SLOT(channelMarkerChanged()));

	m_deviceAPI->registerChannelInstance(m_channelID, this);
	m_deviceAPI->addChannelMarker(&m_channelMarker);
	m_deviceAPI->addRollupWidget(this);

	ui->spectrumGUI->setBuddies(m_spectrumVis->getInputMessageQueue(), m_spectrumVis, ui->glSpectrum);

	applySettingsImmediate();
	applySettings();
}

UDPSrcGUI::~UDPSrcGUI()
{
    m_deviceAPI->removeChannelInstance(this);
	m_deviceAPI->removeThreadedSink(m_threadedChannelizer);
	delete m_threadedChannelizer;
	delete m_channelizer;
	delete m_udpSrc;
	delete m_spectrumVis;
	//delete m_channelMarker;
	delete ui;
}

void UDPSrcGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void UDPSrcGUI::applySettingsImmediate()
{
	if (m_doApplySettings)
	{
		m_audioActive = ui->audioActive->isChecked();
		m_audioStereo = ui->audioStereo->isChecked();
		m_boost = ui->boost->value();
		m_volume = ui->volume->value();

		m_udpSrc->configureImmediate(m_udpSrc->getInputMessageQueue(),
			m_audioActive,
		    m_audioStereo,
			m_boost,
			m_volume);
	}
}


void UDPSrcGUI::applySettings()
{
	if (m_doApplySettings)
	{
		bool ok;

		Real outputSampleRate = ui->sampleRate->text().toDouble(&ok);

		if((!ok) || (outputSampleRate < 1000))
		{
			outputSampleRate = 48000;
		}

		Real rfBandwidth = ui->rfBandwidth->text().toDouble(&ok);

		if((!ok) || (rfBandwidth > outputSampleRate))
		{
			rfBandwidth = outputSampleRate;
		}

		m_udpAddress = ui->udpAddress->text();

		int udpPort = ui->udpPort->text().toInt(&ok);

		if((!ok) || (udpPort < 1024) || (udpPort > 65535))
		{
			udpPort = 9999;
		}

		int audioPort = ui->audioPort->text().toInt(&ok);

		if((!ok) || (audioPort < 1) || (audioPort > 65535) || (audioPort == udpPort))
		{
			audioPort = udpPort - 1;
		}

		int fmDeviation = ui->fmDeviation->text().toInt(&ok);

		if ((!ok) || (fmDeviation < 1))
		{
			fmDeviation = 2500;
		}

		int boost = ui->boost->value();

		setTitleColor(m_channelMarker.getColor());
		ui->deltaFrequency->setValue(abs(m_channelMarker.getCenterFrequency()));
		ui->deltaMinus->setChecked(m_channelMarker.getCenterFrequency() < 0);
		ui->sampleRate->setText(QString("%1").arg(outputSampleRate, 0));
		ui->rfBandwidth->setText(QString("%1").arg(rfBandwidth, 0));
		//ui->udpAddress->setText(m_udpAddress);
		ui->udpPort->setText(QString("%1").arg(udpPort));
		ui->audioPort->setText(QString("%1").arg(audioPort));
		ui->fmDeviation->setText(QString("%1").arg(fmDeviation));
		ui->boost->setValue(boost);
		m_channelMarker.disconnect(this, SLOT(channelMarkerChanged()));
		m_channelMarker.setBandwidth((int)rfBandwidth);
		connect(&m_channelMarker, SIGNAL(changed()), this, SLOT(channelMarkerChanged()));
		ui->glSpectrum->setSampleRate(outputSampleRate);

		m_channelizer->configure(m_channelizer->getInputMessageQueue(),
			outputSampleRate,
			m_channelMarker.getCenterFrequency());

		UDPSrc::SampleFormat sampleFormat;

		switch(ui->sampleFormat->currentIndex())
		{
			case 0:
				sampleFormat = UDPSrc::FormatS16LE;
				ui->fmDeviation->setEnabled(false);
				break;
			case 1:
				sampleFormat = UDPSrc::FormatNFM;
				ui->fmDeviation->setEnabled(true);
				break;
			case 2:
				sampleFormat = UDPSrc::FormatNFMMono;
				ui->fmDeviation->setEnabled(true);
				break;
			case 3:
				sampleFormat = UDPSrc::FormatLSB;
				ui->fmDeviation->setEnabled(false);
				break;
			case 4:
				sampleFormat = UDPSrc::FormatUSB;
				ui->fmDeviation->setEnabled(false);
				break;
			case 5:
				sampleFormat = UDPSrc::FormatLSBMono;
				ui->fmDeviation->setEnabled(false);
				break;
			case 6:
				sampleFormat = UDPSrc::FormatUSBMono;
				ui->fmDeviation->setEnabled(false);
				break;
			case 7:
				sampleFormat = UDPSrc::FormatAMMono;
				ui->fmDeviation->setEnabled(false);
				break;
			default:
				sampleFormat = UDPSrc::FormatS16LE;
				ui->fmDeviation->setEnabled(false);
				break;
		}

		m_sampleFormat = sampleFormat;
		m_outputSampleRate = outputSampleRate;
		m_rfBandwidth = rfBandwidth;
		m_fmDeviation = fmDeviation;
		m_udpPort = udpPort;
		m_audioPort = audioPort;
		m_boost = boost;

		m_udpSrc->configure(m_udpSrc->getInputMessageQueue(),
			sampleFormat,
			outputSampleRate,
			rfBandwidth,
			fmDeviation,
			m_udpAddress,
			udpPort,
			audioPort);

		ui->applyBtn->setEnabled(false);
	}
}

void UDPSrcGUI::on_deltaMinus_toggled(bool minus)
{
	int deltaFrequency = m_channelMarker.getCenterFrequency();
	bool minusDelta = (deltaFrequency < 0);

	if (minus ^ minusDelta) // sign change
	{
		m_channelMarker.setCenterFrequency(-deltaFrequency);
	}
}

void UDPSrcGUI::on_deltaFrequency_changed(quint64 value)
{
	if (ui->deltaMinus->isChecked()) {
		m_channelMarker.setCenterFrequency(-value);
	} else {
		m_channelMarker.setCenterFrequency(value);
	}
}

void UDPSrcGUI::on_sampleFormat_currentIndexChanged(int index)
{
	if ((index == 1) || (index == 2)) {
		ui->fmDeviation->setEnabled(true);
	} else {
		ui->fmDeviation->setEnabled(false);
	}

	ui->applyBtn->setEnabled(true);
}

void UDPSrcGUI::on_sampleRate_textEdited(const QString& arg1)
{
	ui->applyBtn->setEnabled(true);
}

void UDPSrcGUI::on_rfBandwidth_textEdited(const QString& arg1)
{
	ui->applyBtn->setEnabled(true);
}

void UDPSrcGUI::on_fmDeviation_textEdited(const QString& arg1)
{
	ui->applyBtn->setEnabled(true);
}

void UDPSrcGUI::on_udpAddress_textEdited(const QString& arg1)
{
	ui->applyBtn->setEnabled(true);
}

void UDPSrcGUI::on_udpPort_textEdited(const QString& arg1)
{
	ui->applyBtn->setEnabled(true);
}

void UDPSrcGUI::on_audioPort_textEdited(const QString& arg1)
{
	ui->applyBtn->setEnabled(true);
}

void UDPSrcGUI::on_applyBtn_clicked()
{
	applySettings();
}

void UDPSrcGUI::on_audioActive_toggled(bool active)
{
	applySettingsImmediate();
}

void UDPSrcGUI::on_audioStereo_toggled(bool stereo)
{
	applySettingsImmediate();
}

void UDPSrcGUI::on_boost_valueChanged(int value)
{
	ui->boost->setValue(value);
	ui->boostText->setText(QString("%1").arg(value));
	applySettingsImmediate();
}

void UDPSrcGUI::on_volume_valueChanged(int value)
{
	ui->volume->setValue(value);
	ui->volumeText->setText(QString("%1").arg(value));
	applySettingsImmediate();
}

void UDPSrcGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
	if ((widget == ui->spectrumBox) && (m_udpSrc != 0))
	{
		m_udpSrc->setSpectrum(m_udpSrc->getInputMessageQueue(), rollDown);
	}
}

void UDPSrcGUI::onMenuDoubleClicked()
{
	if (!m_basicSettingsShown)
	{
		m_basicSettingsShown = true;
		BasicChannelSettingsWidget* bcsw = new BasicChannelSettingsWidget(&m_channelMarker, this);
		bcsw->show();
	}
}

void UDPSrcGUI::leaveEvent(QEvent*)
{
	blockApplySettings(true);
	m_channelMarker.setHighlighted(false);
	blockApplySettings(false);
}

void UDPSrcGUI::enterEvent(QEvent*)
{
	blockApplySettings(true);
	m_channelMarker.setHighlighted(true);
	blockApplySettings(false);
}


