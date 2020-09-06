///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#include <QDebug>

#include <QTime>
#include <QDateTime>
#include <QString>
#include <QMessageBox>
#include <QFileDialog>

#include "plugin/pluginapi.h"
#include "device/deviceapi.h"
#include "device/deviceuiset.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "gui/crightclickenabler.h"
#include "gui/basicdevicesettingsdialog.h"
#include "dsp/dspengine.h"
#include "dsp/dspdevicemimoengine.h"
#include "dsp/dspcommands.h"
#include "util/db.h"

#include "mainwindow.h"

#include "ui_metismisogui.h"
#include "metismisogui.h"

MetisMISOGui::MetisMISOGui(DeviceUISet *deviceUISet, QWidget* parent) :
    QWidget(parent),
    ui(new Ui::MetisMISOGui),
    m_deviceUISet(deviceUISet),
    m_settings(),
    m_doApplySettings(true),
    m_forceSettings(true),
    m_sampleMIMO(nullptr),
    m_tickCount(0),
    m_lastEngineState(DeviceAPI::StNotStarted)
{
    qDebug("MetisMISOGui::MetisMISOGui");
    m_sampleMIMO = m_deviceUISet->m_deviceAPI->getSampleMIMO();
    m_streamIndex = 0;
    m_spectrumStreamIndex = 0;
    m_rxSampleRate = 48000;
    m_txSampleRate = 48000;

    ui->setupUi(this);
    ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->centerFrequency->setValueRange(7, 0, 61440000);

    displaySettings();

    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
    connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    m_statusTimer.start(500);

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
    m_sampleMIMO->setMessageQueueToGUI(&m_inputMessageQueue);

    CRightClickEnabler *startStopRightClickEnabler = new CRightClickEnabler(ui->startStop);
    connect(startStopRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(openDeviceSettingsDialog(const QPoint &)));
}

MetisMISOGui::~MetisMISOGui()
{
    delete ui;
}

void MetisMISOGui::destroy()
{
    delete this;
}

void MetisMISOGui::setName(const QString& name)
{
    setObjectName(name);
}

QString MetisMISOGui::getName() const
{
    return objectName();
}

void MetisMISOGui::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    sendSettings();
}

qint64 MetisMISOGui::getCenterFrequency() const
{
    if (m_streamIndex == 0) {
        return m_settings.m_rx1CenterFrequency;
    } else if (m_streamIndex == 1) {
        return m_settings.m_rx2CenterFrequency;
    } else if (m_streamIndex == 2) {
        return m_settings.m_rx3CenterFrequency;
    } else if (m_streamIndex == 3) {
        return m_settings.m_rx4CenterFrequency;
    } else if (m_streamIndex == 4) {
        return m_settings.m_rx5CenterFrequency;
    } else if (m_streamIndex == 5) {
        return m_settings.m_rx6CenterFrequency;
    } else if (m_streamIndex == 6) {
        return m_settings.m_rx7CenterFrequency;
    } else if (m_streamIndex == 7) {
        return m_settings.m_rx8CenterFrequency;
    } else if (m_streamIndex == 8) {
        return m_settings.m_txCenterFrequency;
    } else {
        return 0;
    }
}

void MetisMISOGui::setCenterFrequency(qint64 centerFrequency)
{
    if (m_streamIndex == 0) {
        m_settings.m_rx1CenterFrequency = centerFrequency;
    } else if (m_streamIndex == 1) {
        m_settings.m_rx2CenterFrequency = centerFrequency;
    } else if (m_streamIndex == 2) {
        m_settings.m_rx3CenterFrequency = centerFrequency;
    } else if (m_streamIndex == 3) {
        m_settings.m_rx4CenterFrequency = centerFrequency;
    } else if (m_streamIndex == 4) {
        m_settings.m_rx5CenterFrequency = centerFrequency;
    } else if (m_streamIndex == 5) {
        m_settings.m_rx6CenterFrequency = centerFrequency;
    } else if (m_streamIndex == 6) {
        m_settings.m_rx7CenterFrequency = centerFrequency;
    } else if (m_streamIndex == 7) {
        m_settings.m_rx8CenterFrequency = centerFrequency;
    } else if (m_streamIndex == 8) {
        m_settings.m_txCenterFrequency = centerFrequency;
    } else {
        m_settings.m_txCenterFrequency = 0;
    }

    displaySettings();
    sendSettings();
}

QByteArray MetisMISOGui::serialize() const
{
    return m_settings.serialize();
}

bool MetisMISOGui::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data)) {
        displaySettings();
        m_forceSettings = true;
        sendSettings();
        return true;
    } else {
        resetToDefaults();
        return false;
    }
}

void MetisMISOGui::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        MetisMISO::MsgStartStop *message = MetisMISO::MsgStartStop::create(checked);
        m_sampleMIMO->getInputMessageQueue()->push(message);
    }
}

void MetisMISOGui::on_streamIndex_currentIndexChanged(int index)
{
    if (ui->streamLock->isChecked())
    {
        m_spectrumStreamIndex = index;

        if (m_spectrumStreamIndex < MetisMISOSettings::m_maxReceivers)
        {
            m_deviceUISet->m_spectrum->setDisplayedStream(true, index);
            m_deviceUISet->m_deviceAPI->setSpectrumSinkInput(true, m_spectrumStreamIndex);
            m_deviceUISet->setSpectrumScalingFactor(SDR_RX_SCALEF);
        }
        else
        {
            m_deviceUISet->m_spectrum->setDisplayedStream(false, 0);
            m_deviceUISet->m_deviceAPI->setSpectrumSinkInput(false, 0);
            m_deviceUISet->setSpectrumScalingFactor(SDR_TX_SCALEF);
        }

        updateSpectrum();

        ui->spectrumSource->blockSignals(true);
        ui->spectrumSource->setCurrentIndex(index);
        ui->spectrumSource->blockSignals(false);
    }

    m_streamIndex = index;

    displayFrequency();
    displaySampleRate();
}

void MetisMISOGui::on_spectrumSource_currentIndexChanged(int index)
{
    m_spectrumStreamIndex = index;

    if (m_spectrumStreamIndex < MetisMISOSettings::m_maxReceivers)
    {
        m_deviceUISet->m_spectrum->setDisplayedStream(true, index);
        m_deviceUISet->m_deviceAPI->setSpectrumSinkInput(true, m_spectrumStreamIndex);
        m_deviceUISet->setSpectrumScalingFactor(SDR_RX_SCALEF);
    }
    else
    {
        m_deviceUISet->m_spectrum->setDisplayedStream(false, 0);
        m_deviceUISet->m_deviceAPI->setSpectrumSinkInput(false, 0);
        m_deviceUISet->setSpectrumScalingFactor(SDR_TX_SCALEF);
    }

    updateSpectrum();

    if (ui->streamLock->isChecked())
    {
        ui->streamIndex->blockSignals(true);
        ui->streamIndex->setCurrentIndex(index);
        ui->streamIndex->blockSignals(false);
        m_streamIndex = index;
        displayFrequency();
        displaySampleRate();
    }
}

void MetisMISOGui::on_streamLock_toggled(bool checked)
{
    if (checked && (ui->streamIndex->currentIndex() != ui->spectrumSource->currentIndex())) {
        ui->spectrumSource->setCurrentIndex(ui->streamIndex->currentIndex());
    }
}

void MetisMISOGui::on_centerFrequency_changed(quint64 value)
{
    if (m_streamIndex == 0) {
        m_settings.m_rx1CenterFrequency = value * 1000;
    } else if (m_streamIndex == 1) {
        m_settings.m_rx2CenterFrequency = value * 1000;
    } else if (m_streamIndex == 2) {
        m_settings.m_rx3CenterFrequency = value * 1000;
    } else if (m_streamIndex == 3) {
        m_settings.m_rx4CenterFrequency = value * 1000;
    } else if (m_streamIndex == 4) {
        m_settings.m_rx5CenterFrequency = value * 1000;
    } else if (m_streamIndex == 5) {
        m_settings.m_rx6CenterFrequency = value * 1000;
    } else if (m_streamIndex == 6) {
        m_settings.m_rx7CenterFrequency = value * 1000;
    } else if (m_streamIndex == 7) {
        m_settings.m_rx8CenterFrequency = value * 1000;
    } else if (m_streamIndex == 8) {
        m_settings.m_txCenterFrequency = value * 1000;
    } else {
        m_settings.m_txCenterFrequency = 0;
    }

    sendSettings();
}

void MetisMISOGui::on_samplerateIndex_currentIndexChanged(int index)
{
    m_settings.m_sampleRateIndex = index < 0 ? 0 : index > 3 ? 3 : index;
    sendSettings();
}

void MetisMISOGui::on_log2Decim_currentIndexChanged(int index)
{
    m_settings.m_log2Decim = index < 0 ? 0 : index > 3 ? 3 : index;
    displaySampleRate();
    sendSettings();
}

void MetisMISOGui::on_dcBlock_toggled(bool checked)
{
    m_settings.m_dcBlock = checked;
    sendSettings();
}

void MetisMISOGui::on_iqCorrection_toggled(bool checked)
{
    m_settings.m_iqCorrection = checked;
    sendSettings();
}

void MetisMISOGui::on_preamp_toggled(bool checked)
{
	m_settings.m_preamp = checked;
	sendSettings();
}

void MetisMISOGui::on_random_toggled(bool checked)
{
	m_settings.m_random = checked;
	sendSettings();
}

void MetisMISOGui::on_dither_toggled(bool checked)
{
	m_settings.m_dither = checked;
	sendSettings();
}

void MetisMISOGui::on_duplex_toggled(bool checked)
{
	m_settings.m_duplex = checked;
	sendSettings();
}

void MetisMISOGui::on_nbRxIndex_currentIndexChanged(int index)
{
    m_settings.m_nbReceivers = index + 1;
    sendSettings();
}

void MetisMISOGui::on_txEnable_toggled(bool checked)
{
    m_settings.m_txEnable = checked;
    sendSettings();
}

void MetisMISOGui::displaySettings()
{
    blockApplySettings(true);

    ui->streamIndex->setCurrentIndex(m_streamIndex);
    ui->spectrumSource->setCurrentIndex(m_spectrumStreamIndex);
    ui->nbRxIndex->setCurrentIndex(m_settings.m_nbReceivers - 1);
    ui->samplerateIndex->setCurrentIndex(m_settings.m_sampleRateIndex);
    ui->log2Decim->setCurrentIndex(m_settings.m_log2Decim);
    ui->dcBlock->setChecked(m_settings.m_dcBlock);
    ui->iqCorrection->setChecked(m_settings.m_iqCorrection);
    ui->preamp->setChecked(m_settings.m_preamp);
    ui->random->setChecked(m_settings.m_random);
    ui->dither->setChecked(m_settings.m_dither);
    ui->duplex->setChecked(m_settings.m_duplex);
    ui->nbRxIndex->setCurrentIndex(m_settings.m_nbReceivers - 1);
    ui->txEnable->setChecked(m_settings.m_txEnable);
    displayFrequency();
    displaySampleRate();
    updateSpectrum();

    blockApplySettings(false);
}

void MetisMISOGui::sendSettings()
{
    if(!m_updateTimer.isActive()) {
        m_updateTimer.start(100);
    }
}

void MetisMISOGui::updateHardware()
{
    if (m_doApplySettings)
    {
        MetisMISO::MsgConfigureMetisMISO* message = MetisMISO::MsgConfigureMetisMISO::create(m_settings, m_forceSettings);
        m_sampleMIMO->getInputMessageQueue()->push(message);
        m_forceSettings = false;
        m_updateTimer.stop();
    }
}

void MetisMISOGui::updateStatus()
{
    int state = m_deviceUISet->m_deviceAPI->state();

    if(m_lastEngineState != state)
    {
        switch(state)
        {
            case DeviceAPI::StNotStarted:
                ui->startStop->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
                break;
            case DeviceAPI::StIdle:
                ui->startStop->setStyleSheet("QToolButton { background-color : blue; }");
                break;
            case DeviceAPI::StRunning:
                ui->startStop->setStyleSheet("QToolButton { background-color : green; }");
                break;
            case DeviceAPI::StError:
                ui->startStop->setStyleSheet("QToolButton { background-color : red; }");
                QMessageBox::information(this, tr("Message"), m_deviceUISet->m_deviceAPI->errorMessage());
                break;
            default:
                break;
        }

        m_lastEngineState = state;
    }
}

bool MetisMISOGui::handleMessage(const Message& message)
{
    if (MetisMISO::MsgConfigureMetisMISO::match(message))
    {
        qDebug("MetisMISOGui::handleMessage: MsgConfigureMetisMISO");
        const MetisMISO::MsgConfigureMetisMISO& cfg = (MetisMISO::MsgConfigureMetisMISO&) message;
        m_settings = cfg.getSettings();
        displaySettings();
        return true;
    }
    else if (MetisMISO::MsgStartStop::match(message))
    {
        qDebug("MetisMISOGui::handleMessage: MsgStartStop");
        MetisMISO::MsgStartStop& notif = (MetisMISO::MsgStartStop&) message;
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

void MetisMISOGui::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        if (DSPMIMOSignalNotification::match(*message))
        {
            DSPMIMOSignalNotification* notif = (DSPMIMOSignalNotification*) message;
            int istream = notif->getIndex();
            bool sourceOrSink = notif->getSourceOrSink();
            qint64 frequency = notif->getCenterFrequency();

            if (sourceOrSink)
            {
                m_rxSampleRate = notif->getSampleRate();

                if (istream == 0) {
                    m_settings.m_rx1CenterFrequency = frequency;
                } else if (istream == 1) {
                    m_settings.m_rx2CenterFrequency = frequency;
                } else if (istream == 2) {
                    m_settings.m_rx3CenterFrequency = frequency;
                } else if (istream == 3) {
                    m_settings.m_rx4CenterFrequency = frequency;
                } else if (istream == 4) {
                    m_settings.m_rx5CenterFrequency = frequency;
                } else if (istream == 5) {
                    m_settings.m_rx6CenterFrequency = frequency;
                } else if (istream == 6) {
                    m_settings.m_rx7CenterFrequency = frequency;
                } else if (istream == 7) {
                    m_settings.m_rx8CenterFrequency = frequency;
                }
            }
            else
            {
                m_txSampleRate = notif->getSampleRate();
                m_settings.m_txCenterFrequency = frequency;
            }

            qDebug() << "MetisMISOGui::handleInputMessages: DSPMIMOSignalNotification: "
                << "sourceOrSink:" << sourceOrSink
                << "istream:" << istream
                << "m_rxSampleRate:" << m_rxSampleRate
                << "m_txSampleRate:" << m_txSampleRate
                << "frequency:" << frequency;

            displayFrequency();
            updateSpectrum();

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

void MetisMISOGui::displayFrequency()
{
    qint64 centerFrequency;

    if (m_streamIndex == 0) {
        centerFrequency = m_settings.m_rx1CenterFrequency;
    } else if (m_streamIndex == 1) {
        centerFrequency = m_settings.m_rx2CenterFrequency;
    } else if (m_streamIndex == 2) {
        centerFrequency = m_settings.m_rx3CenterFrequency;
    } else if (m_streamIndex == 3) {
        centerFrequency = m_settings.m_rx4CenterFrequency;
    } else if (m_streamIndex == 4) {
        centerFrequency = m_settings.m_rx5CenterFrequency;
    } else if (m_streamIndex == 5) {
        centerFrequency = m_settings.m_rx6CenterFrequency;
    } else if (m_streamIndex == 6) {
        centerFrequency = m_settings.m_rx7CenterFrequency;
    } else if (m_streamIndex == 7) {
        centerFrequency = m_settings.m_rx8CenterFrequency;
    } else if (m_streamIndex == 8) {
        centerFrequency = m_settings.m_txCenterFrequency;
    } else {
        centerFrequency = 0;
    }

    ui->centerFrequency->setValue(centerFrequency / 1000);
}

void MetisMISOGui::displaySampleRate()
{
    if (m_streamIndex < MetisMISOSettings::m_maxReceivers)
    {
        int deviceSampleRate = 48000 * (1<<m_settings.m_sampleRateIndex);
        int sampleRate = deviceSampleRate / (1<<m_settings.m_log2Decim);
        ui->deviceRateText->setText(tr("%1k").arg((float) sampleRate / 1000));
    }
    else
    {
        ui->deviceRateText->setText(tr("48k"));
    }
}

void MetisMISOGui::updateSpectrum()
{
    qint64 centerFrequency;

    if (m_spectrumStreamIndex == 0) {
        centerFrequency = m_settings.m_rx1CenterFrequency;
    } else if (m_spectrumStreamIndex == 1) {
        centerFrequency = m_settings.m_rx2CenterFrequency;
    } else if (m_spectrumStreamIndex == 2) {
        centerFrequency = m_settings.m_rx3CenterFrequency;
    } else if (m_spectrumStreamIndex == 3) {
        centerFrequency = m_settings.m_rx4CenterFrequency;
    } else if (m_spectrumStreamIndex == 4) {
        centerFrequency = m_settings.m_rx5CenterFrequency;
    } else if (m_spectrumStreamIndex == 5) {
        centerFrequency = m_settings.m_rx6CenterFrequency;
    } else if (m_spectrumStreamIndex == 6) {
        centerFrequency = m_settings.m_rx7CenterFrequency;
    } else if (m_spectrumStreamIndex == 7) {
        centerFrequency = m_settings.m_rx8CenterFrequency;
    } else if (m_spectrumStreamIndex == 8) {
        centerFrequency = m_settings.m_txCenterFrequency;
    } else {
        centerFrequency = 0;
    }

    m_deviceUISet->getSpectrum()->setCenterFrequency(centerFrequency);

    if (m_spectrumStreamIndex < MetisMISOSettings::m_maxReceivers) {
        m_deviceUISet->getSpectrum()->setSampleRate(m_rxSampleRate);
    } else {
        m_deviceUISet->getSpectrum()->setSampleRate(m_txSampleRate);
    }
}

void MetisMISOGui::openDeviceSettingsDialog(const QPoint& p)
{
    BasicDeviceSettingsDialog dialog(this);
    dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
    dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
    dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
    dialog.setReverseAPIDeviceIndex(m_settings.m_reverseAPIDeviceIndex);

    dialog.move(p);
    dialog.exec();

    m_settings.m_useReverseAPI = dialog.useReverseAPI();
    m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
    m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
    m_settings.m_reverseAPIDeviceIndex = dialog.getReverseAPIDeviceIndex();

    sendSettings();
}
