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

#include <stdint.h>
#include <sstream>
#include <iostream>
#include <cassert>

#include <QDebug>
#include <QMessageBox>
#include <QTime>
#include <QDateTime>
#include <QString>
#include <QFileDialog>

#ifdef _WIN32
#include <nn.h>
#include <pair.h>
#else
#include <nanomsg/nn.h>
#include <nanomsg/pair.h>
#endif

#include "ui_sdrdaemonsourcegui.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "mainwindow.h"
#include "util/simpleserializer.h"

#include <device/devicesourceapi.h>
#include "device/deviceuiset.h"
#include "sdrdaemonsourcegui.h"

SDRdaemonSourceGui::SDRdaemonSourceGui(DeviceUISet *deviceUISet, QWidget* parent) :
	QWidget(parent),
	ui(new Ui::SDRdaemonSourceGui),
	m_deviceUISet(deviceUISet),
	m_settings(),
	m_sampleSource(0),
	m_acquisition(false),
	m_streamSampleRate(0),
	m_streamCenterFrequency(0),
	m_lastEngineState((DSPDeviceSourceEngine::State)-1),
	m_framesDecodingStatus(0),
	m_bufferLengthInSecs(0.0),
    m_bufferGauge(-50),
	m_nbOriginalBlocks(128),
    m_nbFECBlocks(0),
    m_samplesCount(0),
    m_tickCount(0),
    m_addressEdited(false),
    m_dataPortEdited(false),
	m_countUnrecoverable(0),
	m_countRecovered(0),
    m_doApplySettings(true),
    m_forceSettings(true),
    m_txDelay(0.0)
{
    m_paletteGreenText.setColor(QPalette::WindowText, Qt::green);
    m_paletteWhiteText.setColor(QPalette::WindowText, Qt::white);

	m_startingTimeStamp.tv_sec = 0;
	m_startingTimeStamp.tv_usec = 0;
	ui->setupUi(this);

	ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
	ui->centerFrequency->setValueRange(7, 0, 9999999U);

    ui->freq->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->freq->setValueRange(7, 0, 9999999U);

    ui->sampleRate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
    ui->sampleRate->setValueRange(7, 32000U, 9999999U);

	displaySettings();

	connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
	m_statusTimer.start(500);
	connect(&(m_deviceUISet->m_deviceSourceAPI->getMasterTimer()), SIGNAL(timeout()), this, SLOT(tick()));
    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));

    m_sampleSource = (SDRdaemonSourceInput*) m_deviceUISet->m_deviceSourceAPI->getSampleSource();

	connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);

    m_eventsTime.start();
    displayEventCounts();
    displayEventTimer();

    m_forceSettings = true;
    sendSettings();
}

SDRdaemonSourceGui::~SDRdaemonSourceGui()
{
	delete ui;
}

void SDRdaemonSourceGui::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void SDRdaemonSourceGui::destroy()
{
	delete this;
}

void SDRdaemonSourceGui::setName(const QString& name)
{
	setObjectName(name);
}

QString SDRdaemonSourceGui::getName() const
{
	return objectName();
}

void SDRdaemonSourceGui::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    m_forceSettings = true;
    sendSettings();
}

QByteArray SDRdaemonSourceGui::serialize() const
{
    return m_settings.serialize();
}

bool SDRdaemonSourceGui::deserialize(const QByteArray& data)
{
    qDebug("SDRdaemonSourceGui::deserialize");

    if (m_settings.deserialize(data))
    {
        updateTxDelay();
        displaySettings();
        m_forceSettings = true;
        sendSettings();

        return true;
    }
    else
    {
        return false;
    }
}

qint64 SDRdaemonSourceGui::getCenterFrequency() const
{
    return m_streamCenterFrequency;
}

void SDRdaemonSourceGui::setCenterFrequency(qint64 centerFrequency)
{
    m_settings.m_centerFrequency = centerFrequency;
    sendSettings();
}

bool SDRdaemonSourceGui::handleMessage(const Message& message)
{
    if (SDRdaemonSourceInput::MsgConfigureSDRdaemonSource::match(message))
    {
        const SDRdaemonSourceInput::MsgConfigureSDRdaemonSource& cfg = (SDRdaemonSourceInput::MsgConfigureSDRdaemonSource&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (SDRdaemonSourceInput::MsgReportSDRdaemonAcquisition::match(message))
	{
		m_acquisition = ((SDRdaemonSourceInput::MsgReportSDRdaemonAcquisition&)message).getAcquisition();
		updateWithAcquisition();
		return true;
	}
	else if (SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamData::match(message))
	{
        m_startingTimeStamp.tv_sec = ((SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamData&)message).get_tv_sec();
        m_startingTimeStamp.tv_usec = ((SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamData&)message).get_tv_usec();

        qDebug() << "SDRdaemonSourceGui::handleMessage: SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamData: "
                << " : " << m_startingTimeStamp.tv_sec
                << " : " << m_startingTimeStamp.tv_usec;

        updateWithStreamTime();
        return true;
	}
	else if (SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamTiming::match(message))
	{
		m_startingTimeStamp.tv_sec = ((SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamTiming&)message).get_tv_sec();
		m_startingTimeStamp.tv_usec = ((SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamTiming&)message).get_tv_usec();
		m_framesDecodingStatus = ((SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamTiming&)message).getFramesDecodingStatus();
		m_allBlocksReceived = ((SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamTiming&)message).allBlocksReceived();
		m_bufferLengthInSecs = ((SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamTiming&)message).getBufferLengthInSecs();
        m_bufferGauge = ((SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamTiming&)message).getBufferGauge();
        m_minNbBlocks = ((SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamTiming&)message).getMinNbBlocks();
        m_minNbOriginalBlocks = ((SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamTiming&)message).getMinNbOriginalBlocks();
        m_maxNbRecovery = ((SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamTiming&)message).getMaxNbRecovery();
        m_avgNbBlocks = ((SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamTiming&)message).getAvgNbBlocks();
        m_avgNbOriginalBlocks = ((SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamTiming&)message).getAvgNbOriginalBlocks();
        m_avgNbRecovery = ((SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamTiming&)message).getAvgNbRecovery();
        m_nbOriginalBlocks = ((SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamTiming&)message).getNbOriginalBlocksPerFrame();

        int nbFECBlocks = ((SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamTiming&)message).getNbFECBlocksPerFrame();

        if (m_nbFECBlocks != nbFECBlocks)
        {
            m_nbFECBlocks = nbFECBlocks;
            updateTxDelay();
        }

		updateWithStreamTime();
		return true;
	}
	else if (SDRdaemonSourceInput::MsgStartStop::match(message))
    {
	    SDRdaemonSourceInput::MsgStartStop& notif = (SDRdaemonSourceInput::MsgStartStop&) message;
        blockApplySettings(true);
        ui->startStop->setChecked(notif.getStartStop());
        blockApplySettings(false);

        return true;
    }
	else
	{
		return false;
	}
}

void SDRdaemonSourceGui::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        //qDebug("SDRdaemonGui::handleInputMessages: message: %s", message->getIdentifier());

        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;

            if (notif->getSampleRate() != m_streamSampleRate)
            {
                m_streamSampleRate = notif->getSampleRate();
                updateTxDelay();
            }

            m_streamCenterFrequency = notif->getCenterFrequency();

            qDebug("SDRdaemonGui::handleInputMessages: DSPSignalNotification: SampleRate:%d, CenterFrequency:%llu", notif->getSampleRate(), notif->getCenterFrequency());

            updateSampleRateAndFrequency();
            DSPSignalNotification *fwd = new DSPSignalNotification(*notif);
            m_sampleSource->getInputMessageQueue()->push(fwd);

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

void SDRdaemonSourceGui::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_streamSampleRate);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_streamCenterFrequency);
    ui->deviceRateText->setText(tr("%1k").arg((float)m_streamSampleRate / 1000));
    blockApplySettings(true);
    ui->centerFrequency->setValue(m_streamCenterFrequency / 1000);
    ui->freq->setValue(m_streamCenterFrequency / 1000);
    blockApplySettings(false);
}

void SDRdaemonSourceGui::updateTxDelay()
{
    if (m_streamSampleRate == 0) {
        m_txDelay = 0.0; // 0 value will not set the Tx delay
    } else {
        m_txDelay = ((127*127*m_settings.m_txDelay) / m_streamSampleRate)/(128 + m_nbFECBlocks);
    }

    ui->txDelayText->setToolTip(tr("%1 us").arg(QString::number(m_txDelay*1e6, 'f', 0)));
}

void SDRdaemonSourceGui::displaySettings()
{
    blockApplySettings(true);

    ui->centerFrequency->setValue(m_streamCenterFrequency / 1000);
    ui->deviceRateText->setText(tr("%1k").arg(m_streamSampleRate / 1000.0));

    ui->freq->setValue(m_streamCenterFrequency / 1000);
    ui->decim->setCurrentIndex(m_settings.m_log2Decim);
    ui->fcPos->setCurrentIndex(m_settings.m_fcPos);
    ui->sampleRate->setValue(m_settings.m_sampleRate);
    ui->specificParms->setText(m_settings.m_specificParameters);
    ui->specificParms->setCursorPosition(0);
    ui->txDelayText->setText(tr("%1").arg(m_settings.m_txDelay*100));
    ui->nbFECBlocks->setValue(m_settings.m_nbFECBlocks);
    QString nstr = QString("%1").arg(m_settings.m_nbFECBlocks, 2, 10, QChar('0'));
    ui->nbFECBlocksText->setText(nstr);

    QString s0 = QString::number(128 + m_settings.m_nbFECBlocks, 'f', 0);
    ui->nominalNbBlocksText->setText(tr("%1/%2").arg(s0).arg(nstr));

    ui->address->setText(m_settings.m_address);
    ui->dataPort->setText(tr("%1").arg(m_settings.m_dataPort));
    ui->controlPort->setText(tr("%1").arg(m_settings.m_controlPort));
    ui->specificParms->setText(m_settings.m_specificParameters);

	ui->dcOffset->setChecked(m_settings.m_dcBlock);
	ui->iqImbalance->setChecked(m_settings.m_iqCorrection);

	blockApplySettings(false);
}

void SDRdaemonSourceGui::sendSettings()
{
    if(!m_updateTimer.isActive())
        m_updateTimer.start(100);
}

void SDRdaemonSourceGui::on_applyButton_clicked(bool checked __attribute__((unused)))
{
    m_settings.m_address = ui->address->text();

    bool send = false;
    bool ctlOk;
    int udpCtlPort = ui->controlPort->text().toInt(&ctlOk);

    if((ctlOk) && (udpCtlPort >= 1024) && (udpCtlPort < 65535))
    {
        m_settings.m_controlPort = udpCtlPort;
        send = true;
    }

    bool dataOk;
    int udpDataPort = ui->dataPort->text().toInt(&dataOk);

    if((dataOk) && (udpDataPort >= 1024) && (udpDataPort < 65535))
    {
        m_settings.m_dataPort = udpDataPort;
        send = true;
    }

    if (send) {
        sendSettings();
    }
}

void SDRdaemonSourceGui::on_sendButton_clicked(bool checked __attribute__((unused)))
{
    updateTxDelay();
    m_forceSettings = true;
    sendSettings();
	ui->specificParms->setCursorPosition(0);
}

void SDRdaemonSourceGui::on_address_returnPressed()
{
    m_settings.m_address = ui->address->text();
    sendSettings();
}

void SDRdaemonSourceGui::on_dataPort_returnPressed()
{
    bool dataOk;
    int udpDataPort = ui->dataPort->text().toInt(&dataOk);

    if((!dataOk) || (udpDataPort < 1024) || (udpDataPort > 65535))
    {
        return;
    }
    else
    {
        m_settings.m_dataPort = udpDataPort;
        sendSettings();
    }
}

void SDRdaemonSourceGui::on_controlPort_returnPressed()
{
    bool ctlOk;
    int udpCtlPort = ui->controlPort->text().toInt(&ctlOk);

    if((!ctlOk) || (udpCtlPort < 1024) || (udpCtlPort > 65535))
    {
        return;
    }
    else
    {
        m_settings.m_controlPort = udpCtlPort;
        sendSettings();
    }
}

void SDRdaemonSourceGui::on_dcOffset_toggled(bool checked)
{
    m_settings.m_dcBlock = checked;
    sendSettings();
}

void SDRdaemonSourceGui::on_iqImbalance_toggled(bool checked)
{
    m_settings.m_iqCorrection = checked;
    sendSettings();
}

void SDRdaemonSourceGui::on_freq_changed(quint64 value)
{
    m_settings.m_centerFrequency = value * 1000;
    sendSettings();
}

void SDRdaemonSourceGui::on_sampleRate_changed(quint64 value)
{
    m_settings.m_sampleRate = value;
    sendSettings();
}

void SDRdaemonSourceGui::on_specificParms_returnPressed()
{
    if ((ui->specificParms->text()).size() > 0) {
        m_settings.m_specificParameters = ui->specificParms->text();
        sendSettings();
    }
}

void SDRdaemonSourceGui::on_decim_currentIndexChanged(int index __attribute__((unused)))
{
    m_settings.m_log2Decim = ui->decim->currentIndex();
    sendSettings();
}

void SDRdaemonSourceGui::on_fcPos_currentIndexChanged(int index __attribute__((unused)))
{
    m_settings.m_fcPos = ui->fcPos->currentIndex();
    sendSettings();
}

void SDRdaemonSourceGui::on_txDelay_valueChanged(int value)
{
    m_settings.m_txDelay = value / 100.0;
    ui->txDelayText->setText(tr("%1").arg(value));
    updateTxDelay();
    sendSettings();
}

void SDRdaemonSourceGui::on_nbFECBlocks_valueChanged(int value)
{
    m_settings.m_nbFECBlocks = value;
    QString nstr = QString("%1").arg(m_settings.m_nbFECBlocks, 2, 10, QChar('0'));
    ui->nbFECBlocksText->setText(nstr);
    sendSettings();
}

void SDRdaemonSourceGui::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        SDRdaemonSourceInput::MsgStartStop *message = SDRdaemonSourceInput::MsgStartStop::create(checked);
        m_sampleSource->getInputMessageQueue()->push(message);
    }
}

void SDRdaemonSourceGui::on_record_toggled(bool checked)
{
    if (checked) {
        ui->record->setStyleSheet("QToolButton { background-color : red; }");
    } else {
        ui->record->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
    }

    SDRdaemonSourceInput::MsgFileRecord* message = SDRdaemonSourceInput::MsgFileRecord::create(checked);
    m_sampleSource->getInputMessageQueue()->push(message);
}

void SDRdaemonSourceGui::on_eventCountsReset_clicked(bool checked __attribute__((unused)))
{
    m_countUnrecoverable = 0;
    m_countRecovered = 0;
    m_eventsTime.start();
    displayEventCounts();
    displayEventTimer();
}

void SDRdaemonSourceGui::displayEventCounts()
{
    QString nstr = QString("%1").arg(m_countUnrecoverable, 3, 10, QChar('0'));
    ui->eventUnrecText->setText(nstr);
    nstr = QString("%1").arg(m_countRecovered, 3, 10, QChar('0'));
    ui->eventRecText->setText(nstr);
}

void SDRdaemonSourceGui::displayEventTimer()
{
    int elapsedTimeMillis = m_eventsTime.elapsed();
    QTime recordLength(0, 0, 0, 0);
    recordLength = recordLength.addSecs(elapsedTimeMillis/1000);
    QString s_time = recordLength.toString("hh:mm:ss");
    ui->eventCountsTimeText->setText(s_time);
}

void SDRdaemonSourceGui::updateWithAcquisition()
{
}

void SDRdaemonSourceGui::updateWithStreamTime()
{
	bool updateEventCounts = false;
    quint64 startingTimeStampMsec = ((quint64) m_startingTimeStamp.tv_sec * 1000LL) + ((quint64) m_startingTimeStamp.tv_usec / 1000LL);
    QDateTime dt = QDateTime::fromMSecsSinceEpoch(startingTimeStampMsec);
    QString s_date = dt.toString("yyyy-MM-dd  hh:mm:ss.zzz");
	ui->absTimeText->setText(s_date);

	if (m_framesDecodingStatus == 2)
	{
		ui->allFramesDecoded->setStyleSheet("QToolButton { background-color : green; }");
	}
	else if (m_framesDecodingStatus == 1)
	{
        if (m_countRecovered < 999) m_countRecovered++;
        updateEventCounts = true;
        ui->allFramesDecoded->setStyleSheet("QToolButton { background:rgb(56,56,56); }");
	}
	else
	{
        if (m_countUnrecoverable < 999) m_countUnrecoverable++;
        updateEventCounts = true;
        ui->allFramesDecoded->setStyleSheet("QToolButton { background-color : red; }");
	}

	QString s = QString::number(m_bufferLengthInSecs, 'f', 1);
	ui->bufferLenSecsText->setText(tr("%1").arg(s));

    s = QString::number(m_bufferGauge, 'f', 0);
	ui->bufferRWBalanceText->setText(tr("%1").arg(s));

    ui->bufferGaugeNegative->setValue((m_bufferGauge < 0 ? -m_bufferGauge : 0));
    ui->bufferGaugePositive->setValue((m_bufferGauge < 0 ? 0 : m_bufferGauge));

    s = QString::number(m_minNbBlocks, 'f', 0);
    ui->minNbBlocksText->setText(tr("%1").arg(s));

    s = QString("%1").arg(m_maxNbRecovery, 2, 10, QChar('0'));
    ui->maxNbRecoveryText->setText(tr("%1").arg(s));

    s = QString::number(m_nbOriginalBlocks + m_nbFECBlocks, 'f', 0);
    QString s1 = QString("%1").arg(m_nbFECBlocks, 2, 10, QChar('0'));
    ui->nominalNbBlocksText->setText(tr("%1/%2").arg(s).arg(s1));

    if (updateEventCounts)
    {
        displayEventCounts();
    }

    displayEventTimer();
}

void SDRdaemonSourceGui::updateHardware()
{
    if (m_doApplySettings)
    {
        qDebug() << "SDRdaemonSinkGui::updateHardware";
        SDRdaemonSourceInput::MsgConfigureSDRdaemonSource* message =
                SDRdaemonSourceInput::MsgConfigureSDRdaemonSource::create(m_settings, m_forceSettings);
        m_sampleSource->getInputMessageQueue()->push(message);
        m_forceSettings = false;
        m_updateTimer.stop();
    }
}

void SDRdaemonSourceGui::updateStatus()
{
    if (m_sampleSource->isStreaming())
    {
        int state = m_deviceUISet->m_deviceSourceAPI->state();

        if (m_lastEngineState != state)
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

        ui->startStop->setEnabled(true);
    }
    else
    {
        ui->startStop->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
        ui->startStop->setChecked(false);
        ui->startStop->setEnabled(false);
    }
}

void SDRdaemonSourceGui::tick()
{
	if ((++m_tickCount & 0xf) == 0) {
		SDRdaemonSourceInput::MsgConfigureSDRdaemonStreamTiming* message = SDRdaemonSourceInput::MsgConfigureSDRdaemonStreamTiming::create();
		m_sampleSource->getInputMessageQueue()->push(message);
	}
}
