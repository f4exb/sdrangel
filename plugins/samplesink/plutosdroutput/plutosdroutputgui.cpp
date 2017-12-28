///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#include <stdio.h>
#include <QDebug>
#include <QMessageBox>

#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "gui/glspectrum.h"
#include "device/devicesinkapi.h"
#include "device/deviceuiset.h"
#include "plutosdr/deviceplutosdr.h"
#include "plutosdroutput.h"
#include "plutosdroutputgui.h"
#include "ui_plutosdroutputgui.h"

PlutoSDROutputGUI::PlutoSDROutputGUI(DeviceUISet *deviceUISet, QWidget* parent) :
    QWidget(parent),
    ui(new Ui::PlutoSDROutputGUI),
    m_deviceUISet(deviceUISet),
    m_settings(),
    m_forceSettings(true),
    m_sampleSink(0),
    m_sampleRate(0),
    m_deviceCenterFrequency(0),
    m_lastEngineState((DSPDeviceSinkEngine::State)-1),
    m_doApplySettings(true),
    m_statusCounter(0)
{
    m_sampleSink = (PlutoSDROutput*) m_deviceUISet->m_deviceSinkAPI->getSampleSink();

    ui->setupUi(this);
    ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    updateFrequencyLimits();

    ui->sampleRate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
    ui->sampleRate->setValueRange(8, DevicePlutoSDR::srLowLimitFreq, DevicePlutoSDR::srHighLimitFreq);

    ui->lpf->setColorMapper(ColorMapper(ColorMapper::GrayYellow));
    ui->lpf->setValueRange(5, DevicePlutoSDR::bbLPTxLowLimitFreq/1000, DevicePlutoSDR::bbLPTxHighLimitFreq/1000);

    ui->lpFIR->setColorMapper(ColorMapper(ColorMapper::GrayYellow));
    ui->lpFIR->setValueRange(5, 1U, 56000U); // will be dynamically recalculated

    ui->swInterpLabel->setText(QString::fromUtf8("S\u2191"));
    ui->lpFIRInterpolationLabel->setText(QString::fromUtf8("\u2191"));

    blockApplySettings(true);
    displaySettings();
    blockApplySettings(false);

    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
    connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    m_statusTimer.start(500);

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
}

PlutoSDROutputGUI::~PlutoSDROutputGUI()
{
    delete ui;
}

void PlutoSDROutputGUI::destroy()
{
    delete this;
}

void PlutoSDROutputGUI::setName(const QString& name)
{
    setObjectName(name);
}

QString PlutoSDROutputGUI::getName() const
{
    return objectName();
}

void PlutoSDROutputGUI::resetToDefaults()
{

}

qint64 PlutoSDROutputGUI::getCenterFrequency() const
{
    return m_settings.m_centerFrequency;
}

void PlutoSDROutputGUI::setCenterFrequency(qint64 centerFrequency)
{
    m_settings.m_centerFrequency = centerFrequency;
    displaySettings();
    sendSettings();
}

QByteArray PlutoSDROutputGUI::serialize() const
{
    return m_settings.serialize();
}

bool PlutoSDROutputGUI::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data))
    {
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);
        sendSettings(true);
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

bool PlutoSDROutputGUI::handleMessage(const Message& message __attribute__((unused)))
{
    if (PlutoSDROutput::MsgConfigurePlutoSDR::match(message))
    {
        const PlutoSDROutput::MsgConfigurePlutoSDR& cfg = (PlutoSDROutput::MsgConfigurePlutoSDR&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (DevicePlutoSDRShared::MsgCrossReportToBuddy::match(message)) // message from buddy
    {
        DevicePlutoSDRShared::MsgCrossReportToBuddy& conf = (DevicePlutoSDRShared::MsgCrossReportToBuddy&) message;
        m_settings.m_devSampleRate = conf.getDevSampleRate();
        m_settings.m_lpfFIRlog2Interp = conf.getLpfFiRlog2IntDec();
        m_settings.m_lpfFIRBW = conf.getLpfFirbw();
        m_settings.m_LOppmTenths = conf.getLoPPMTenths();
        m_settings.m_lpfFIREnable = conf.isLpfFirEnable();
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);

        return true;
    }
    else if (PlutoSDROutput::MsgStartStop::match(message))
    {
        PlutoSDROutput::MsgStartStop& notif = (PlutoSDROutput::MsgStartStop&) message;
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

void PlutoSDROutputGUI::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        PlutoSDROutput::MsgStartStop *message = PlutoSDROutput::MsgStartStop::create(checked);
        m_sampleSink->getInputMessageQueue()->push(message);
    }
}

void PlutoSDROutputGUI::on_centerFrequency_changed(quint64 value)
{
    m_settings.m_centerFrequency = value * 1000;
    sendSettings();
}

void PlutoSDROutputGUI::on_loPPM_valueChanged(int value)
{
    ui->loPPMText->setText(QString("%1").arg(QString::number(value/10.0, 'f', 1)));
    m_settings.m_LOppmTenths = value;
    sendSettings();
}

void PlutoSDROutputGUI::on_swInterp_currentIndexChanged(int index)
{
    m_settings.m_log2Interp = index > 5 ? 5 : index;
    sendSettings();
}

void PlutoSDROutputGUI::on_sampleRate_changed(quint64 value)
{
    m_settings.m_devSampleRate = value;
    sendSettings();
}

void PlutoSDROutputGUI::on_lpf_changed(quint64 value)
{
    m_settings.m_lpfBW = value * 1000;
    sendSettings();
}

void PlutoSDROutputGUI::on_lpFIREnable_toggled(bool checked)
{
    m_settings.m_lpfFIREnable = checked;
    ui->lpFIRInterpolation->setEnabled(checked);
    ui->lpFIRGain->setEnabled(checked);
    sendSettings();
}

void PlutoSDROutputGUI::on_lpFIR_changed(quint64 value)
{
    m_settings.m_lpfFIRBW = value * 1000;
    sendSettings();
}

void PlutoSDROutputGUI::on_lpFIRInterpolation_currentIndexChanged(int index)
{
    m_settings.m_lpfFIRlog2Interp = index > 2 ? 2 : index;
    setSampleRateLimits();
    sendSettings();
}

void PlutoSDROutputGUI::on_lpFIRGain_currentIndexChanged(int index)
{
    m_settings.m_lpfFIRGain = 6*(index > 1 ? 1 : index) - 6;
    sendSettings();
}

void PlutoSDROutputGUI::on_att_valueChanged(int value)
{
    ui->attText->setText(QString("%1 dB").arg(QString::number(value*0.25, 'f', 2)));
    m_settings.m_att = value;
    sendSettings();
}

void PlutoSDROutputGUI::on_antenna_currentIndexChanged(int index)
{
    m_settings.m_antennaPath = (PlutoSDROutputSettings::RFPath) (index < PlutoSDROutputSettings::RFPATH_END ? index : 0);
    sendSettings();
}

void PlutoSDROutputGUI::on_transverter_clicked()
{
    m_settings.m_transverterMode = ui->transverter->getDeltaFrequencyAcive();
    m_settings.m_transverterDeltaFrequency = ui->transverter->getDeltaFrequency();
    qDebug("PlutoSDROutputGUI::on_transverter_clicked: %lld Hz %s", m_settings.m_transverterDeltaFrequency, m_settings.m_transverterMode ? "on" : "off");
    updateFrequencyLimits();
    m_settings.m_centerFrequency = ui->centerFrequency->getValueNew()*1000;
    sendSettings();
}

void PlutoSDROutputGUI::displaySettings()
{
    ui->transverter->setDeltaFrequency(m_settings.m_transverterDeltaFrequency);
    ui->transverter->setDeltaFrequencyActive(m_settings.m_transverterMode);
    updateFrequencyLimits();
    ui->centerFrequency->setValue(m_settings.m_centerFrequency / 1000);
    ui->sampleRate->setValue(m_settings.m_devSampleRate);

    ui->loPPM->setValue(m_settings.m_LOppmTenths);
    ui->loPPMText->setText(QString("%1").arg(QString::number(m_settings.m_LOppmTenths/10.0, 'f', 1)));

    ui->swInterp->setCurrentIndex(m_settings.m_log2Interp);

    ui->lpf->setValue(m_settings.m_lpfBW / 1000);

    ui->lpFIREnable->setChecked(m_settings.m_lpfFIREnable);
    ui->lpFIR->setValue(m_settings.m_lpfFIRBW / 1000);
    ui->lpFIRInterpolation->setCurrentIndex(m_settings.m_lpfFIRlog2Interp);
    ui->lpFIRGain->setCurrentIndex((m_settings.m_lpfFIRGain + 6)/6);
    ui->lpFIRInterpolation->setEnabled(m_settings.m_lpfFIREnable);
    ui->lpFIRGain->setEnabled(m_settings.m_lpfFIREnable);

    ui->att->setValue(m_settings.m_att);
    ui->attText->setText(QString("%1 dB").arg(QString::number(m_settings.m_att*0.25, 'f', 2)));

    ui->antenna->setCurrentIndex((int) m_settings.m_antennaPath);

    setFIRBWLimits();
    setSampleRateLimits();
}

void PlutoSDROutputGUI::sendSettings(bool forceSettings)
{
    m_forceSettings = forceSettings;
    if(!m_updateTimer.isActive()) { m_updateTimer.start(100); }
}

void PlutoSDROutputGUI::updateHardware()
{
    if (m_doApplySettings)
    {
        qDebug() << "PlutoSDROutputGUI::updateHardware";
        PlutoSDROutput::MsgConfigurePlutoSDR* message = PlutoSDROutput::MsgConfigurePlutoSDR::create(m_settings, m_forceSettings);
        m_sampleSink->getInputMessageQueue()->push(message);
        m_forceSettings = false;
        m_updateTimer.stop();
    }
}

void PlutoSDROutputGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void PlutoSDROutputGUI::updateStatus()
{
    int state = m_deviceUISet->m_deviceSinkAPI->state();

    if(m_lastEngineState != state)
    {
        switch(state)
        {
            case DSPDeviceSinkEngine::StNotStarted:
                ui->startStop->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
                break;
            case DSPDeviceSinkEngine::StIdle:
                ui->startStop->setStyleSheet("QToolButton { background-color : blue; }");
                break;
            case DSPDeviceSinkEngine::StRunning:
                ui->startStop->setStyleSheet("QToolButton { background-color : green; }");
                break;
            case DSPDeviceSinkEngine::StError:
                ui->startStop->setStyleSheet("QToolButton { background-color : red; }");
                QMessageBox::information(this, tr("Message"), m_deviceUISet->m_deviceSinkAPI->errorMessage());
                break;
            default:
                break;
        }

        m_lastEngineState = state;
    }

    if (m_statusCounter % 2 == 0) // 1s
    {
        uint32_t dacRate = ((PlutoSDROutput *) m_sampleSink)->getDACSampleRate();

        if (dacRate < 100000000) {
            ui->dacRateLabel->setText(tr("%1k").arg(QString::number(dacRate / 1000.0f, 'g', 5)));
        } else {
            ui->dacRateLabel->setText(tr("%1M").arg(QString::number(dacRate / 1000000.0f, 'g', 5)));
        }
    }

    if (m_statusCounter % 4 == 0) // 2s
    {
        std::string rssiStr;
        ((PlutoSDROutput *) m_sampleSink)->getRSSI(rssiStr);
        ui->rssiText->setText(tr("-%1").arg(QString::fromStdString(rssiStr)));
    }

    if (m_statusCounter % 10 == 0) // 5s
    {
        if (m_deviceUISet->m_deviceSinkAPI->isBuddyLeader()) {
            ((PlutoSDROutput *) m_sampleSink)->fetchTemperature();
        }

        ui->temperatureText->setText(tr("%1C").arg(QString::number(((PlutoSDROutput *) m_sampleSink)->getTemperature(), 'f', 0)));
    }

    m_statusCounter++;
}

void PlutoSDROutputGUI::setFIRBWLimits()
{
    float high = DevicePlutoSDR::firBWHighLimitFactor * ((PlutoSDROutput *) m_sampleSink)->getFIRSampleRate();
    float low = DevicePlutoSDR::firBWLowLimitFactor * ((PlutoSDROutput *) m_sampleSink)->getFIRSampleRate();
    ui->lpFIR->setValueRange(5, (int(low)/1000)+1, (int(high)/1000)+1);
    ui->lpFIR->setValue(m_settings.m_lpfFIRBW/1000);
}

void PlutoSDROutputGUI::setSampleRateLimits()
{
    uint32_t low = ui->lpFIREnable->isChecked() ? DevicePlutoSDR::srLowLimitFreq / (1<<ui->lpFIRInterpolation->currentIndex()) : DevicePlutoSDR::srLowLimitFreq;
    ui->sampleRate->setValueRange(8, low, DevicePlutoSDR::srHighLimitFreq);
    ui->sampleRate->setValue(m_settings.m_devSampleRate);
}

void PlutoSDROutputGUI::updateFrequencyLimits()
{
    // values in kHz
    qint64 deltaFrequency = m_settings.m_transverterMode ? m_settings.m_transverterDeltaFrequency/1000 : 0;
    qint64 minLimit = DevicePlutoSDR::loLowLimitFreq/1000 + deltaFrequency;
    qint64 maxLimit = DevicePlutoSDR::loHighLimitFreq/1000 + deltaFrequency;

    minLimit = minLimit < 0 ? 0 : minLimit > 9999999 ? 9999999 : minLimit;
    maxLimit = maxLimit < 0 ? 0 : maxLimit > 9999999 ? 9999999 : maxLimit;

    qDebug("PlutoSDRInputGui::updateFrequencyLimits: delta: %lld min: %lld max: %lld", deltaFrequency, minLimit, maxLimit);

    ui->centerFrequency->setValueRange(7, minLimit, maxLimit);
}

void PlutoSDROutputGUI::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        qDebug("PlutoSDROutputGUI::handleInputMessages: message: %s", message->getIdentifier());

        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_sampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            qDebug("PlutoSDROutputGUI::handleInputMessages: DSPSignalNotification: SampleRate: %d, CenterFrequency: %llu", notif->getSampleRate(), notif->getCenterFrequency());
            updateSampleRateAndFrequency();
            setFIRBWLimits();

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

void PlutoSDROutputGUI::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_sampleRate);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    ui->deviceRateLabel->setText(tr("%1k").arg(QString::number(m_sampleRate / 1000.0f, 'g', 5)));
}
