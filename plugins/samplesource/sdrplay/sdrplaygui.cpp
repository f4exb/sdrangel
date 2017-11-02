///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#include <QDebug>
#include <QMessageBox>

#include "sdrplaygui.h"

#include <device/devicesourceapi.h>
#include "device/deviceuiset.h"

#include "ui_sdrplaygui.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"


SDRPlayGui::SDRPlayGui(DeviceUISet *deviceUISet, QWidget* parent) :
    QWidget(parent),
    ui(new Ui::SDRPlayGui),
    m_deviceUISet(deviceUISet),
    m_forceSettings(true)
{
    m_sampleSource = (SDRPlayInput*) m_deviceUISet->m_deviceSourceAPI->getSampleSource();

    ui->setupUi(this);
    ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->centerFrequency->setValueRange(7, 10U, 12000U);

    ui->fBand->clear();
    for (unsigned int i = 0; i < SDRPlayBands::getNbBands(); i++)
    {
        ui->fBand->addItem(SDRPlayBands::getBandName(i));
    }

    ui->ifFrequency->clear();
    for (unsigned int i = 0; i < SDRPlayIF::getNbIFs(); i++)
    {
        ui->ifFrequency->addItem(QString::number(SDRPlayIF::getIF(i)/1000));
    }

    ui->samplerate->clear();
    for (unsigned int i = 0; i < SDRPlaySampleRates::getNbRates(); i++)
    {
        ui->samplerate->addItem(QString::number(SDRPlaySampleRates::getRate(i)/1000));
    }

    ui->bandwidth->clear();
    for (unsigned int i = 0; i < SDRPlayBandwidths::getNbBandwidths(); i++)
    {
        ui->bandwidth->addItem(QString::number(SDRPlayBandwidths::getBandwidth(i)/1000));
    }

    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
    connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    m_statusTimer.start(500);

    displaySettings();

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
}

SDRPlayGui::~SDRPlayGui()
{
    delete ui;
}

void SDRPlayGui::destroy()
{
    delete this;
}

void SDRPlayGui::setName(const QString& name)
{
    setObjectName(name);
}

QString SDRPlayGui::getName() const
{
    return objectName();
}

void SDRPlayGui::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    sendSettings();
}

qint64 SDRPlayGui::getCenterFrequency() const
{
    return m_settings.m_centerFrequency;
}

void SDRPlayGui::setCenterFrequency(qint64 centerFrequency)
{
    m_settings.m_centerFrequency = centerFrequency;
    displaySettings();
    sendSettings();
}

QByteArray SDRPlayGui::serialize() const
{
    return m_settings.serialize();
}

bool SDRPlayGui::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data))
    {
        displaySettings();
        m_forceSettings = true;
        sendSettings();
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

bool SDRPlayGui::handleMessage(const Message& message)
{
    if (SDRPlayInput::MsgReportSDRPlayGains::match(message))
    {
    	qDebug() << "SDRPlayGui::handleMessage: MsgReportSDRPlayGains";

    	SDRPlayInput::MsgReportSDRPlayGains msg = (SDRPlayInput::MsgReportSDRPlayGains&) message;

    	if (m_settings.m_tunerGainMode)
    	{
            ui->gainLNA->setChecked(msg.getLNAGain() != 0);
            ui->gainMixer->setChecked(msg.getMixerGain() != 0);
            ui->gainBaseband->setValue(msg.getBasebandGain());

            QString gainText;
            gainText.sprintf("%02d", msg.getBasebandGain());
            ui->gainBasebandText->setText(gainText);
    	}
    	else
    	{
    	    ui->gainTuner->setValue(msg.getTunerGain());

            QString gainText;
            gainText.sprintf("%03d", msg.getTunerGain());
            ui->gainTunerText->setText(gainText);
    	}

        return true;
    }
    else
    {
        return false;
    }
}

void SDRPlayGui::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        qDebug("SDRPlayGui::handleInputMessages: message: %s", message->getIdentifier());

        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_sampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            qDebug("SDRPlayGui::handleInputMessages: DSPSignalNotification: SampleRate:%d, CenterFrequency:%llu", notif->getSampleRate(), notif->getCenterFrequency());
            updateSampleRateAndFrequency();

            delete message;
        }
        else
        {
            if (handleMessage(*message))
            {
                delete message;
            }
        }
    }
}

void SDRPlayGui::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_sampleRate);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    ui->deviceRateText->setText(tr("%1k").arg((float)m_sampleRate / 1000));
}


void SDRPlayGui::displaySettings()
{
    ui->centerFrequency->setValue(m_settings.m_centerFrequency / 1000);

    ui->ppm->setValue(m_settings.m_LOppmTenths);
    ui->ppmText->setText(QString("%1").arg(QString::number(m_settings.m_LOppmTenths/10.0, 'f', 1)));

    ui->samplerate->setCurrentIndex(m_settings.m_devSampleRateIndex);

    ui->dcOffset->setChecked(m_settings.m_dcBlock);
    ui->iqImbalance->setChecked(m_settings.m_iqCorrection);

    ui->fBand->setCurrentIndex(m_settings.m_frequencyBandIndex);
    ui->bandwidth->setCurrentIndex(m_settings.m_bandwidthIndex);
    ui->ifFrequency->setCurrentIndex(m_settings.m_ifFrequencyIndex);
    ui->samplerate->setCurrentIndex(m_settings.m_devSampleRateIndex);

    ui->decim->setCurrentIndex(m_settings.m_log2Decim);
    ui->fcPos->setCurrentIndex((int) m_settings.m_fcPos);

    ui->gainTunerOn->setChecked(m_settings.m_tunerGainMode);

    if (m_settings.m_tunerGainMode)
    {
        ui->gainTuner->setEnabled(true);
        ui->gainLNA->setEnabled(false);
        ui->gainMixer->setEnabled(false);
        ui->gainBaseband->setEnabled(false);

        int gain = m_settings.m_tunerGain;
        ui->gainTuner->setValue(gain);
        QString gainText;
        gainText.sprintf("%03d", gain);
        ui->gainTunerText->setText(gainText);
        m_settings.m_tunerGain = gain;
    }
    else
    {
        ui->gainTuner->setEnabled(false);
        ui->gainLNA->setEnabled(true);
        ui->gainMixer->setEnabled(true);
        ui->gainBaseband->setEnabled(true);

        ui->gainLNA->setChecked(m_settings.m_lnaOn != 0);
        ui->gainMixer->setChecked(m_settings.m_mixerAmpOn != 0);

        int gain = m_settings.m_basebandGain;
        ui->gainBaseband->setValue(gain);
        QString gainText;
        gainText.sprintf("%02d", gain);
        ui->gainBasebandText->setText(gainText);
    }
}

void SDRPlayGui::sendSettings()
{
    if(!m_updateTimer.isActive())
        m_updateTimer.start(100);
}

void SDRPlayGui::updateHardware()
{
    qDebug() << "SDRPlayGui::updateHardware";
    SDRPlayInput::MsgConfigureSDRPlay* message = SDRPlayInput::MsgConfigureSDRPlay::create( m_settings, m_forceSettings);
    m_sampleSource->getInputMessageQueue()->push(message);
    m_forceSettings = false;
    m_updateTimer.stop();
}

void SDRPlayGui::updateStatus()
{
    int state = m_deviceUISet->m_deviceSourceAPI->state();

    if(m_lastEngineState != state)
    {
        switch(state)
        {
            case DSPDeviceSourceEngine::StNotStarted:
                ui->startStop->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
                break;
            case DSPDeviceSourceEngine::StIdle:
                ui->startStop->setStyleSheet("QToolButton { background-color : blue; }");
                break;
            case DSPDeviceSourceEngine::StRunning:
                ui->startStop->setStyleSheet("QToolButton { background-color : green; }");
                break;
            case DSPDeviceSourceEngine::StError:
                ui->startStop->setStyleSheet("QToolButton { background-color : red; }");
                QMessageBox::information(this, tr("Message"), m_deviceUISet->m_deviceSourceAPI->errorMessage());
                break;
            default:
                break;
        }

        m_lastEngineState = state;
    }
}

void SDRPlayGui::on_centerFrequency_changed(quint64 value)
{
    m_settings.m_centerFrequency = value * 1000;
    sendSettings();
}

void SDRPlayGui::on_ppm_valueChanged(int value)
{
    m_settings.m_LOppmTenths = value;
    ui->ppmText->setText(QString("%1").arg(QString::number(m_settings.m_LOppmTenths/10.0, 'f', 1)));
    sendSettings();
}

void SDRPlayGui::on_dcOffset_toggled(bool checked)
{
    qDebug("SDRPlayGui::on_dcOffset_toggled: %s", checked ? "on" : "off");
    m_settings.m_dcBlock = checked;
    sendSettings();
}

void SDRPlayGui::on_iqImbalance_toggled(bool checked)
{
    m_settings.m_iqCorrection = checked;
    sendSettings();
}

void SDRPlayGui::on_fBand_currentIndexChanged(int index)
{
    ui->centerFrequency->setValueRange(
            7,
            SDRPlayBands::getBandLow(index),
            SDRPlayBands::getBandHigh(index));

    ui->centerFrequency->setValue((SDRPlayBands::getBandLow(index)+SDRPlayBands::getBandHigh(index)) / 2);
    m_settings.m_centerFrequency = (SDRPlayBands::getBandLow(index)+SDRPlayBands::getBandHigh(index)) * 500;
    m_settings.m_frequencyBandIndex = index;

    sendSettings();
}

void SDRPlayGui::on_bandwidth_currentIndexChanged(int index)
{
    m_settings.m_bandwidthIndex = index;
    sendSettings();
}

void SDRPlayGui::on_samplerate_currentIndexChanged(int index)
{
    m_settings.m_devSampleRateIndex = index;
    sendSettings();
}

void SDRPlayGui::on_ifFrequency_currentIndexChanged(int index)
{
    m_settings.m_ifFrequencyIndex = index;
    sendSettings();
}

void SDRPlayGui::on_decim_currentIndexChanged(int index)
{
    m_settings.m_log2Decim = index;
    sendSettings();
}

void SDRPlayGui::on_fcPos_currentIndexChanged(int index)
{
    m_settings.m_fcPos = (SDRPlaySettings::fcPos_t) index;
    sendSettings();
}

void SDRPlayGui::on_gainTunerOn_toggled(bool checked)
{
    qDebug("SDRPlayGui::on_gainTunerOn_toggled: %s", checked ? "on" : "off");
    m_settings.m_tunerGainMode = true;
    ui->gainTuner->setEnabled(true);
    ui->gainLNA->setEnabled(false);
    ui->gainMixer->setEnabled(false);
    ui->gainBaseband->setEnabled(false);

    sendSettings();
}

void SDRPlayGui::on_gainTuner_valueChanged(int value)
{
    int gain = value;
	QString gainText;
	gainText.sprintf("%03d", gain);
    ui->gainTunerText->setText(gainText);
    m_settings.m_tunerGain = gain;

    sendSettings();
}

void SDRPlayGui::on_gainManualOn_toggled(bool checked)
{
    qDebug("SDRPlayGui::on_gainManualOn_toggled: %s", checked ? "on" : "off");
    m_settings.m_tunerGainMode = false;
    ui->gainTuner->setEnabled(false);
    ui->gainLNA->setEnabled(true);
    ui->gainMixer->setEnabled(true);
    ui->gainBaseband->setEnabled(true);

    sendSettings();
}

void SDRPlayGui::on_gainLNA_toggled(bool checked)
{
	m_settings.m_lnaOn = checked ? 1 : 0;
	sendSettings();
}

void SDRPlayGui::on_gainMixer_toggled(bool checked)
{
	m_settings.m_mixerAmpOn = checked ? 1 : 0;
	sendSettings();
}

void SDRPlayGui::on_gainBaseband_valueChanged(int value)
{
	m_settings.m_basebandGain = value;

    QString gainText;
    gainText.sprintf("%02d", value);
    ui->gainBasebandText->setText(gainText);

	sendSettings();
}

void SDRPlayGui::on_startStop_toggled(bool checked)
{
    if (checked)
    {
        if (m_deviceUISet->m_deviceSourceAPI->initAcquisition())
        {
            m_deviceUISet->m_deviceSourceAPI->startAcquisition();
            DSPEngine::instance()->startAudioOutput();
        }
    }
    else
    {
        m_deviceUISet->m_deviceSourceAPI->stopAcquisition();
        DSPEngine::instance()->stopAudioOutput();
    }
}

void SDRPlayGui::on_record_toggled(bool checked)
{
    if (checked) {
        ui->record->setStyleSheet("QToolButton { background-color : red; }");
    } else {
        ui->record->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
    }

    SDRPlayInput::MsgFileRecord* message = SDRPlayInput::MsgFileRecord::create(checked);
    m_sampleSource->getInputMessageQueue()->push(message);
}

// ====================================================================

unsigned int SDRPlaySampleRates::m_rates[m_nb_rates] = {
		1536000,   // 0
		1792000,   // 1
        2000000,   // 2
        2048000,   // 3
        2304000,   // 4
        2400000,   // 5
        3072000,   // 6
        3200000,   // 7
        4000000,   // 8
        4096000,   // 9
        4608000,   // 10
        4800000,   // 11
        5000000,   // 12
        6000000,   // 13
        6144000,   // 14
        6400000,   // 15
        8000000,   // 16
        8192000,   // 17
};

unsigned int SDRPlaySampleRates::getRate(unsigned int rate_index)
{
    if (rate_index < m_nb_rates)
    {
        return m_rates[rate_index];
    }
    else
    {
        return m_rates[0];
    }
}

unsigned int SDRPlaySampleRates::getRateIndex(unsigned int rate)
{
    for (unsigned int i=0; i < m_nb_rates; i++)
    {
        if (rate == m_rates[i])
        {
            return i;
        }
    }

    return 0;
}

unsigned int SDRPlaySampleRates::getNbRates()
{
    return SDRPlaySampleRates::m_nb_rates;
}

// ====================================================================

unsigned int SDRPlayBandwidths::m_bw[m_nb_bw] = {
        200000,    // 0
        300000,    // 1
        600000,    // 2
        1536000,   // 3
        5000000,   // 4
        6000000,   // 5
        7000000,   // 6
        8000000,   // 7
};

unsigned int SDRPlayBandwidths::getBandwidth(unsigned int bandwidth_index)
{
    if (bandwidth_index < m_nb_bw)
    {
        return m_bw[bandwidth_index];
    }
    else
    {
        return m_bw[0];
    }
}

unsigned int SDRPlayBandwidths::getBandwidthIndex(unsigned int bandwidth)
{
    for (unsigned int i=0; i < m_nb_bw; i++)
    {
        if (bandwidth == m_bw[i])
        {
            return i;
        }
    }

    return 0;
}

unsigned int SDRPlayBandwidths::getNbBandwidths()
{
    return SDRPlayBandwidths::m_nb_bw;
}

// ====================================================================

unsigned int SDRPlayIF::m_if[m_nb_if] = {
        0,         // 0
        450000,    // 1
        1620000,   // 2
        2048000,   // 3
};

unsigned int SDRPlayIF::getIF(unsigned int if_index)
{
    if (if_index < m_nb_if)
    {
        return m_if[if_index];
    }
    else
    {
        return m_if[0];
    }
}

unsigned int SDRPlayIF::getIFIndex(unsigned int iff)
{
    for (unsigned int i=0; i < m_nb_if; i++)
    {
        if (iff == m_if[i])
        {
            return i;
        }
    }

    return 0;
}

unsigned int SDRPlayIF::getNbIFs()
{
    return SDRPlayIF::m_nb_if;
}

// ====================================================================

/** Lower frequency bound in kHz inclusive */
unsigned int SDRPlayBands::m_bandLow[m_nb_bands] = {
        10,       // 0
        12000,    // 1
        30000,    // 2
        50000,    // 3
        120000,   // 4
        250000,   // 5
        380000,   // 6
        1000000,  // 7
};

/** Lower frequency bound in kHz exclusive */
unsigned int SDRPlayBands::m_bandHigh[m_nb_bands] = {
        12000,    // 0
        30000,    // 1
        50000,    // 2
        120000,   // 3
        250000,   // 4
        380000,   // 5
        1000000,  // 6
        2000000,  // 7
};

const char* SDRPlayBands::m_bandName[m_nb_bands] = {
        "10k-12M",    // 0
        "12-30M",     // 1
        "30-50M",     // 2
        "50-120M",    // 3
        "120-250M",   // 4
        "250-380M",   // 5
        "380M-1G",    // 6
        "1-2G",       // 7
};

QString SDRPlayBands::getBandName(unsigned int band_index)
{
    if (band_index < m_nb_bands)
    {
        return QString(m_bandName[band_index]);
    }
    else
    {
        return QString(m_bandName[0]);
    }
}

unsigned int SDRPlayBands::getBandLow(unsigned int band_index)
{
    if (band_index < m_nb_bands)
    {
        return m_bandLow[band_index];
    }
    else
    {
        return m_bandLow[0];
    }
}

unsigned int SDRPlayBands::getBandHigh(unsigned int band_index)
{
    if (band_index < m_nb_bands)
    {
        return m_bandHigh[band_index];
    }
    else
    {
        return m_bandHigh[0];
    }
}


unsigned int SDRPlayBands::getNbBands()
{
    return SDRPlayBands::m_nb_bands;
}
