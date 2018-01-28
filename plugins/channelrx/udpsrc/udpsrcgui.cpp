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

#include "udpsrcgui.h"

#include "device/devicesourceapi.h"
#include "device/deviceuiset.h"
#include "plugin/pluginapi.h"
#include "dsp/spectrumvis.h"
#include "dsp/dspengine.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "gui/basicchannelsettingsdialog.h"
#include "ui_udpsrcgui.h"
#include "mainwindow.h"

#include "udpsrc.h"

UDPSrcGUI* UDPSrcGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
	UDPSrcGUI* gui = new UDPSrcGUI(pluginAPI, deviceUISet, rxChannel);
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
    m_settings.resetToDefaults();
    displaySettings();
    applySettingsImmediate(true);
    applySettings(true);
}

QByteArray UDPSrcGUI::serialize() const
{
    return m_settings.serialize();
}

bool UDPSrcGUI::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data))
    {
        displaySettings();
        applySettingsImmediate(true);
        applySettings(true);
        return true;
    } else {
        resetToDefaults();
        return false;
    }
}

bool UDPSrcGUI::handleMessage(const Message& message __attribute__((unused)))
{
	qDebug() << "UDPSrcGUI::handleMessage";
	return false;
}

void UDPSrcGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettingsImmediate();
}

void UDPSrcGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void UDPSrcGUI::tick()
{
    if (m_tickCount % 4 == 0)
    {
//        m_channelPowerAvg.feed(m_udpSrc->getMagSq());
//        double powDb = CalcDb::dbPower(m_channelPowerAvg.average());
        double powDb = CalcDb::dbPower(m_udpSrc->getMagSq());
        ui->channelPower->setText(QString::number(powDb, 'f', 1));
        m_inPowerAvg.feed(m_udpSrc->getInMagSq());
        double inPowDb = CalcDb::dbPower(m_inPowerAvg.average());
        ui->inputPower->setText(QString::number(inPowDb, 'f', 1));
    }

    if (m_udpSrc->getSquelchOpen()) {
        ui->squelchLabel->setStyleSheet("QLabel { background-color : green; }");
    } else {
        ui->squelchLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");
    }

	m_tickCount++;
}

UDPSrcGUI::UDPSrcGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
	RollupWidget(parent),
	ui(new Ui::UDPSrcGUI),
	m_pluginAPI(pluginAPI),
	m_deviceUISet(deviceUISet),
	m_udpSrc(0),
	m_channelMarker(this),
	m_channelPowerAvg(4, 1e-10),
    m_inPowerAvg(4, 1e-10),
	m_tickCount(0),
    m_doApplySettings(true),
    m_rfBandwidthChanged(false)
{
	ui->setupUi(this);
	connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));
	setAttribute(Qt::WA_DeleteOnClose, true);

	m_spectrumVis = new SpectrumVis(SDR_RX_SCALEF, ui->glSpectrum);
	m_udpSrc = (UDPSrc*) rxChannel; //new UDPSrc(m_deviceUISet->m_deviceSourceAPI);
	m_udpSrc->setSpectrum(m_spectrumVis);

	ui->fmDeviation->setEnabled(false);

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);
    ui->squelchLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");

	ui->glSpectrum->setCenterFrequency(0);
	ui->glSpectrum->setSampleRate(ui->sampleRate->text().toInt());
	ui->glSpectrum->setDisplayWaterfall(true);
	ui->glSpectrum->setDisplayMaxHold(true);
	m_spectrumVis->configure(m_spectrumVis->getInputMessageQueue(), 64, 10, FFTWindow::BlackmanHarris);

	ui->glSpectrum->connectTimer(MainWindow::getInstance()->getMasterTimer());
	connect(&MainWindow::getInstance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

	m_channelMarker.blockSignals(true);
	m_channelMarker.setBandwidth(16000);
	m_channelMarker.setCenterFrequency(0);
	m_channelMarker.setTitle("UDP Sample Source");
	m_channelMarker.setUDPAddress("127.0.0.1");
	m_channelMarker.setUDPSendPort(9999);
	m_channelMarker.setUDPReceivePort(9998);
    m_channelMarker.setColor(m_settings.m_rgbColor);
    m_channelMarker.blockSignals(false);
	m_channelMarker.setVisible(true); // activate signal on the last setting only

	setTitleColor(m_channelMarker.getColor());

	m_settings.setChannelMarker(&m_channelMarker);
	m_settings.setSpectrumGUI(ui->spectrumGUI);

	m_deviceUISet->registerRxChannelInstance(UDPSrc::m_channelIdURI, this);
	m_deviceUISet->addChannelMarker(&m_channelMarker);
	m_deviceUISet->addRollupWidget(this);

	connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()), this, SLOT(channelMarkerHighlightedByCursor()));

	ui->spectrumGUI->setBuddies(m_spectrumVis->getInputMessageQueue(), m_spectrumVis, ui->glSpectrum);

	displaySettings();
	applySettingsImmediate(true);
	applySettings(true);
}

UDPSrcGUI::~UDPSrcGUI()
{
    m_deviceUISet->removeRxChannelInstance(this);
	delete m_udpSrc; // TODO: check this: when the GUI closes it has to delete the demodulator
	delete m_spectrumVis;
	delete ui;
}

void UDPSrcGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void UDPSrcGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    displayUDPAddress();

    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());

    ui->sampleRate->setText(QString("%1").arg(m_settings.m_outputSampleRate, 0));
    setSampleFormatIndex(m_settings.m_sampleFormat);

    ui->squelch->setValue(m_settings.m_squelchdB);
    ui->squelchText->setText(tr("%1").arg(ui->squelch->value()*1.0, 0, 'f', 0));

    qDebug("UDPSrcGUI::deserialize: m_squelchGate: %d", m_settings.m_squelchGate);
    ui->squelchGate->setValue(m_settings.m_squelchGate);
    ui->squelchGateText->setText(tr("%1").arg(m_settings.m_squelchGate*10.0, 0, 'f', 0));

    ui->rfBandwidth->setText(QString("%1").arg(m_settings.m_rfBandwidth, 0));
    ui->fmDeviation->setText(QString("%1").arg(m_settings.m_fmDeviation, 0));

    ui->agc->setChecked(m_settings.m_agc);
    ui->audioActive->setChecked(m_settings.m_audioActive);
    ui->audioStereo->setChecked(m_settings.m_audioStereo);

    ui->volume->setValue(m_settings.m_volume);
    ui->volumeText->setText(QString("%1").arg(ui->volume->value()));

    ui->gain->setValue(m_settings.m_gain*10.0);
    ui->gainText->setText(tr("%1").arg(ui->gain->value()/10.0, 0, 'f', 1));

    ui->glSpectrum->setSampleRate(m_settings.m_outputSampleRate);

    displayUDPAddress();
}

void UDPSrcGUI::displayUDPAddress()
{
    ui->addressText->setText(tr("%1:%2/%3").arg(m_channelMarker.getUDPAddress()).arg(m_channelMarker.getUDPSendPort()).arg(m_channelMarker.getUDPReceivePort()));
}

void UDPSrcGUI::setSampleFormatIndex(const UDPSrcSettings::SampleFormat& sampleFormat)
{
    switch(sampleFormat)
    {
        case UDPSrcSettings::FormatIQ:
            ui->sampleFormat->setCurrentIndex(0);
            break;
        case UDPSrcSettings::FormatNFM:
            ui->sampleFormat->setCurrentIndex(1);
            break;
        case UDPSrcSettings::FormatNFMMono:
            ui->sampleFormat->setCurrentIndex(2);
            break;
        case UDPSrcSettings::FormatLSB:
            ui->sampleFormat->setCurrentIndex(3);
            break;
        case UDPSrcSettings::FormatUSB:
            ui->sampleFormat->setCurrentIndex(4);
            break;
        case UDPSrcSettings::FormatLSBMono:
            ui->sampleFormat->setCurrentIndex(5);
            break;
        case UDPSrcSettings::FormatUSBMono:
            ui->sampleFormat->setCurrentIndex(6);
            break;
        case UDPSrcSettings::FormatAMMono:
            ui->sampleFormat->setCurrentIndex(7);
            break;
        case UDPSrcSettings::FormatAMNoDCMono:
            ui->sampleFormat->setCurrentIndex(8);
            break;
        case UDPSrcSettings::FormatAMBPFMono:
            ui->sampleFormat->setCurrentIndex(9);
            break;
        default:
            ui->sampleFormat->setCurrentIndex(0);
            break;
    }
}

void UDPSrcGUI::setSampleFormat(int index)
{
    switch(index)
    {
        case 0:
            m_settings.m_sampleFormat = UDPSrcSettings::FormatIQ;
            ui->fmDeviation->setEnabled(false);
            break;
        case 1:
            m_settings.m_sampleFormat = UDPSrcSettings::FormatNFM;
            ui->fmDeviation->setEnabled(true);
            break;
        case 2:
            m_settings.m_sampleFormat = UDPSrcSettings::FormatNFMMono;
            ui->fmDeviation->setEnabled(true);
            break;
        case 3:
            m_settings.m_sampleFormat = UDPSrcSettings::FormatLSB;
            ui->fmDeviation->setEnabled(false);
            break;
        case 4:
            m_settings.m_sampleFormat = UDPSrcSettings::FormatUSB;
            ui->fmDeviation->setEnabled(false);
            break;
        case 5:
            m_settings.m_sampleFormat = UDPSrcSettings::FormatLSBMono;
            ui->fmDeviation->setEnabled(false);
            break;
        case 6:
            m_settings.m_sampleFormat = UDPSrcSettings::FormatUSBMono;
            ui->fmDeviation->setEnabled(false);
            break;
        case 7:
            m_settings.m_sampleFormat = UDPSrcSettings::FormatAMMono;
            ui->fmDeviation->setEnabled(false);
            break;
        case 8:
            m_settings.m_sampleFormat = UDPSrcSettings::FormatAMNoDCMono;
            ui->fmDeviation->setEnabled(false);
            break;
        case 9:
            m_settings.m_sampleFormat = UDPSrcSettings::FormatAMBPFMono;
            ui->fmDeviation->setEnabled(false);
            break;
        default:
            m_settings.m_sampleFormat = UDPSrcSettings::FormatIQ;
            ui->fmDeviation->setEnabled(false);
            break;
    }
}

void UDPSrcGUI::applySettingsImmediate(bool force)
{
	if (m_doApplySettings)
	{
        UDPSrc::MsgConfigureUDPSrc* message = UDPSrc::MsgConfigureUDPSrc::create( m_settings, force);
        m_udpSrc->getInputMessageQueue()->push(message);
	}
}

void UDPSrcGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
        UDPSrc::MsgConfigureChannelizer* channelConfigMsg = UDPSrc::MsgConfigureChannelizer::create(
                m_settings.m_outputSampleRate, m_channelMarker.getCenterFrequency());
        m_udpSrc->getInputMessageQueue()->push(channelConfigMsg);

        UDPSrc::MsgConfigureUDPSrc* message = UDPSrc::MsgConfigureUDPSrc::create( m_settings, force);
        m_udpSrc->getInputMessageQueue()->push(message);

		ui->applyBtn->setEnabled(false);
		ui->applyBtn->setStyleSheet("QPushButton { background:rgb(79,79,79); }");
	}
}

void UDPSrcGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void UDPSrcGUI::on_sampleFormat_currentIndexChanged(int index)
{
	if ((index == 1) || (index == 2)) {
		ui->fmDeviation->setEnabled(true);
	} else {
		ui->fmDeviation->setEnabled(false);
	}

	setSampleFormat(index);

	ui->applyBtn->setEnabled(true);
	ui->applyBtn->setStyleSheet("QPushButton { background-color : green; }");
}

void UDPSrcGUI::on_sampleSize_currentIndexChanged(int index)
{
    if ((index < 0) || (index >= UDPSrcSettings::SizeNone)) {
        return;
    }

    m_settings.m_sampleSize = (UDPSrcSettings::SampleSize) index;

    ui->applyBtn->setEnabled(true);
    ui->applyBtn->setStyleSheet("QPushButton { background-color : green; }");
}

void UDPSrcGUI::on_sampleRate_textEdited(const QString& arg1 __attribute__((unused)))
{
    bool ok;
    Real outputSampleRate = ui->sampleRate->text().toDouble(&ok);

    if((!ok) || (outputSampleRate < 1000))
    {
        m_settings.m_outputSampleRate = 48000;
        ui->sampleRate->setText(QString("%1").arg(outputSampleRate, 0));
    }
    else
    {
        m_settings.m_outputSampleRate = outputSampleRate;
    }

	ui->applyBtn->setEnabled(true);
	ui->applyBtn->setStyleSheet("QPushButton { background-color : green; }");
}

void UDPSrcGUI::on_rfBandwidth_textEdited(const QString& arg1 __attribute__((unused)))
{
    bool ok;
    Real rfBandwidth = ui->rfBandwidth->text().toDouble(&ok);

    if ((!ok) || (rfBandwidth > m_settings.m_outputSampleRate))
    {
        m_settings.m_rfBandwidth = m_settings.m_outputSampleRate;
        ui->rfBandwidth->setText(QString("%1").arg(m_settings.m_rfBandwidth, 0));
    }
    else
    {
        m_settings.m_rfBandwidth = rfBandwidth;
    }

    m_rfBandwidthChanged = true;

	ui->applyBtn->setEnabled(true);
	ui->applyBtn->setStyleSheet("QPushButton { background-color : green; }");
}

void UDPSrcGUI::on_fmDeviation_textEdited(const QString& arg1 __attribute__((unused)))
{
    bool ok;
    int fmDeviation = ui->fmDeviation->text().toInt(&ok);

    if ((!ok) || (fmDeviation < 1))
    {
        m_settings.m_fmDeviation = 2500;
        ui->fmDeviation->setText(QString("%1").arg(m_settings.m_fmDeviation));
    }
    else
    {
        m_settings.m_fmDeviation = fmDeviation;
    }

	ui->applyBtn->setEnabled(true);
	ui->applyBtn->setStyleSheet("QPushButton { background-color : green; }");
}

void UDPSrcGUI::on_applyBtn_clicked()
{
    if (m_rfBandwidthChanged)
    {
        m_channelMarker.setBandwidth((int) m_settings.m_rfBandwidth);
        m_rfBandwidthChanged = false;
    }

    ui->glSpectrum->setSampleRate(m_settings.m_outputSampleRate);

	applySettings();
}

void UDPSrcGUI::on_audioActive_toggled(bool active)
{
    m_settings.m_audioActive = active;
	applySettingsImmediate();
}

void UDPSrcGUI::on_audioStereo_toggled(bool stereo)
{
    m_settings.m_audioStereo = stereo;
	applySettingsImmediate();
}

void UDPSrcGUI::on_agc_toggled(bool agc)
{
    m_settings.m_agc = agc;
    applySettingsImmediate();
}

void UDPSrcGUI::on_gain_valueChanged(int value)
{
    m_settings.m_gain = value / 10.0;
	ui->gainText->setText(tr("%1").arg(value/10.0, 0, 'f', 1));
	applySettingsImmediate();
}

void UDPSrcGUI::on_volume_valueChanged(int value)
{
    m_settings.m_volume = value;
	ui->volumeText->setText(QString("%1").arg(value));
	applySettingsImmediate();
}

void UDPSrcGUI::on_squelch_valueChanged(int value)
{
    m_settings.m_squelchdB = value;

    if (value == -100) // means disabled
    {
        ui->squelchText->setText("---");
        m_settings.m_squelchEnabled = false;
    }
    else
    {
        ui->squelchText->setText(tr("%1").arg(value*1.0, 0, 'f', 0));
        m_settings.m_squelchEnabled = true;
    }

    applySettingsImmediate();
}

void UDPSrcGUI::on_squelchGate_valueChanged(int value)
{
    m_settings.m_squelchGate = value;
    ui->squelchGateText->setText(tr("%1").arg(value*10.0, 0, 'f', 0));
    applySettingsImmediate();
}

void UDPSrcGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
	if ((widget == ui->spectrumBox) && (m_udpSrc != 0))
	{
		m_udpSrc->setSpectrum(m_udpSrc->getInputMessageQueue(), rollDown);
	}
}

void UDPSrcGUI::onMenuDialogCalled(const QPoint &p)
{
    BasicChannelSettingsDialog dialog(&m_channelMarker, this);
    dialog.move(p);
    dialog.exec();


    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    m_settings.m_udpAddress = m_channelMarker.getUDPAddress(),
    m_settings.m_udpPort =  m_channelMarker.getUDPSendPort(),
    m_settings.m_rgbColor = m_channelMarker.getColor().rgb();
    m_settings.m_title = m_channelMarker.getTitle();

    setWindowTitle(m_settings.m_title);
    setTitleColor(m_settings.m_rgbColor);
    displayUDPAddress();

    applySettingsImmediate();
}

void UDPSrcGUI::leaveEvent(QEvent*)
{
	m_channelMarker.setHighlighted(false);
}

void UDPSrcGUI::enterEvent(QEvent*)
{
	m_channelMarker.setHighlighted(true);
}


