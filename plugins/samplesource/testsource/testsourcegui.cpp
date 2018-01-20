///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#include <QTime>
#include <QDateTime>
#include <QString>
#include <QFileDialog>
#include <QMessageBox>

#include "ui_testsourcegui.h"
#include "plugin/pluginapi.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "util/db.h"

#include "mainwindow.h"

#include "testsourcegui.h"
#include <device/devicesourceapi.h>
#include "device/deviceuiset.h"

TestSourceGui::TestSourceGui(DeviceUISet *deviceUISet, QWidget* parent) :
    QWidget(parent),
    ui(new Ui::TestSourceGui),
    m_deviceUISet(deviceUISet),
    m_settings(),
    m_doApplySettings(true),
    m_forceSettings(true),
    m_sampleSource(0),
    m_tickCount(0),
    m_lastEngineState((DSPDeviceSourceEngine::State)-1)
{
    qDebug("TestSourceGui::TestSourceGui");
    m_sampleSource = m_deviceUISet->m_deviceSourceAPI->getSampleSource();

    ui->setupUi(this);
    ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->centerFrequency->setValueRange(7, 0, 9999999);
    ui->sampleRate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
    ui->sampleRate->setValueRange(7, 48000, 9999999);
    ui->frequencyShift->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->frequencyShift->setValueRange(false, 7, -9999999, 9999999);

    displaySettings();

    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
    connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    m_statusTimer.start(500);

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
}

TestSourceGui::~TestSourceGui()
{
    delete ui;
}

void TestSourceGui::destroy()
{
    delete this;
}

void TestSourceGui::setName(const QString& name)
{
    setObjectName(name);
}

QString TestSourceGui::getName() const
{
    return objectName();
}

void TestSourceGui::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    sendSettings();
}

qint64 TestSourceGui::getCenterFrequency() const
{
    return m_settings.m_centerFrequency;
}

void TestSourceGui::setCenterFrequency(qint64 centerFrequency)
{
    m_settings.m_centerFrequency = centerFrequency;
    displaySettings();
    sendSettings();
}

QByteArray TestSourceGui::serialize() const
{
    return m_settings.serialize();
}

bool TestSourceGui::deserialize(const QByteArray& data)
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

void TestSourceGui::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        TestSourceInput::MsgStartStop *message = TestSourceInput::MsgStartStop::create(checked);
        m_sampleSource->getInputMessageQueue()->push(message);
    }
}

void TestSourceGui::on_centerFrequency_changed(quint64 value)
{
    m_settings.m_centerFrequency = value * 1000;
    sendSettings();
}

void TestSourceGui::on_dcOffset_toggled(bool checked)
{
    m_settings.m_dcBlock = checked;
    sendSettings();
}

void TestSourceGui::on_iqImbalance_toggled(bool checked)
{
    m_settings.m_iqImbalance = checked;
    sendSettings();
}


void TestSourceGui::on_frequencyShift_changed(qint64 value)
{
    m_settings.m_frequencyShift = value;
    sendSettings();
}

void TestSourceGui::on_decimation_currentIndexChanged(int index)
{
    if ((index < 0) || (index > 6)) {
        return;
    }

    m_settings.m_log2Decim = index;
    sendSettings();
}

void TestSourceGui::on_fcPos_currentIndexChanged(int index)
{
    if ((index < 0) || (index > 2)) {
        return;
    }

    m_settings.m_fcPos = (TestSourceSettings::fcPos_t) index;
    sendSettings();
}

void TestSourceGui::on_sampleRate_changed(quint64 value)
{
    updateFrequencyShiftLimit();
    m_settings.m_frequencyShift = ui->frequencyShift->getValueNew();
    m_settings.m_sampleRate = value;
    sendSettings();
}

void TestSourceGui::on_sampleSize_currentIndexChanged(int index)
{
    if ((index < 0) || (index > 2)) {
        return;
    }

    updateAmpCoarseLimit();
    updateAmpFineLimit();
    displayAmplitude();
    m_settings.m_amplitudeBits = ui->amplitudeCoarse->value() * 100 + ui->amplitudeFine->value();
    m_settings.m_sampleSizeIndex = index;
    sendSettings();
}

void TestSourceGui::on_amplitudeCoarse_valueChanged(int value __attribute__((unused)))
{
    updateAmpFineLimit();
    displayAmplitude();
    m_settings.m_amplitudeBits = ui->amplitudeCoarse->value() * 100 + ui->amplitudeFine->value();
    sendSettings();
}

void TestSourceGui::on_amplitudeFine_valueChanged(int value __attribute__((unused)))
{
    displayAmplitude();
    m_settings.m_amplitudeBits = ui->amplitudeCoarse->value() * 100 + ui->amplitudeFine->value();
    sendSettings();
}

void TestSourceGui::on_dcBias_valueChanged(int value)
{
    ui->dcBiasText->setText(QString(tr("%1 %").arg(value)));
    m_settings.m_dcFactor = value / 100.0f;
    sendSettings();
}

void TestSourceGui::on_iBias_valueChanged(int value)
{
    ui->iBiasText->setText(QString(tr("%1 %").arg(value)));
    m_settings.m_iFactor = value / 100.0f;
    sendSettings();
}

void TestSourceGui::on_qBias_valueChanged(int value)
{
    ui->qBiasText->setText(QString(tr("%1 %").arg(value)));
    m_settings.m_qFactor = value / 100.0f;
    sendSettings();
}

void TestSourceGui::on_record_toggled(bool checked)
{
    if (checked) {
        ui->record->setStyleSheet("QToolButton { background-color : red; }");
    } else {
        ui->record->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
    }

    TestSourceInput::MsgFileRecord* message = TestSourceInput::MsgFileRecord::create(checked);
    m_sampleSource->getInputMessageQueue()->push(message);
}

void TestSourceGui::displayAmplitude()
{
    int amplitudeInt = ui->amplitudeCoarse->value() * 100 + ui->amplitudeFine->value();
    double power;

    switch (ui->sampleSize->currentIndex())
    {
    case 0: // 8 bits: 128
        power = (double) amplitudeInt*amplitudeInt / (double) (1<<14);
        break;
    case 1: // 12 bits 2048
        power = (double) amplitudeInt*amplitudeInt / (double) (1<<22);
        break;
    case 2: // 16 bits 32768
    default:
        power = (double) amplitudeInt*amplitudeInt / (double) (1<<30);
        break;
    }

    ui->amplitudeBits->setText(QString(tr("%1 b").arg(amplitudeInt)));
    double powerDb = CalcDb::dbPower(power);
    ui->power->setText(QString(tr("%1 dB").arg(QString::number(powerDb, 'f', 1))));
}

void TestSourceGui::updateAmpCoarseLimit()
{
    switch (ui->sampleSize->currentIndex())
    {
    case 0: // 8 bits: 128
        ui->amplitudeCoarse->setMaximum(1);
        break;
    case 1: // 12 bits 2048
        ui->amplitudeCoarse->setMaximum(20);
        break;
    case 2: // 16 bits 32768
    default:
        ui->amplitudeCoarse->setMaximum(327);
        break;
    }
}

void TestSourceGui::updateAmpFineLimit()
{
    switch (ui->sampleSize->currentIndex())
    {
    case 0: // 8 bits: 128
        if (ui->amplitudeCoarse->value() == 1) {
            ui->amplitudeFine->setMaximum(27);
        } else {
            ui->amplitudeFine->setMaximum(99);
        }
        break;
    case 1: // 12 bits 2048
        if (ui->amplitudeCoarse->value() == 20) {
            ui->amplitudeFine->setMaximum(47);
        } else {
            ui->amplitudeFine->setMaximum(99);
        }
        break;
    case 2: // 16 bits 32768
    default:
        if (ui->amplitudeCoarse->value() == 327) {
            ui->amplitudeFine->setMaximum(67);
        } else {
            ui->amplitudeFine->setMaximum(99);
        }
        break;
    }
}

void TestSourceGui::updateFrequencyShiftLimit()
{
    int sampleRate = ui->sampleRate->getValueNew();
    ui->frequencyShift->setValueRange(false, 7, -sampleRate, sampleRate);
}

void TestSourceGui::displaySettings()
{
    blockApplySettings(true);
    ui->sampleSize->blockSignals(true);

    ui->centerFrequency->setValue(m_settings.m_centerFrequency / 1000);
    ui->decimation->setCurrentIndex(m_settings.m_log2Decim);
    ui->fcPos->setCurrentIndex((int) m_settings.m_fcPos);
    ui->sampleRate->setValue(m_settings.m_sampleRate);
    updateFrequencyShiftLimit();
    ui->frequencyShift->setValue(m_settings.m_frequencyShift);
    ui->sampleSize->setCurrentIndex(m_settings.m_sampleSizeIndex);
    updateAmpCoarseLimit();
    int amplitudeBits = m_settings.m_amplitudeBits;
    ui->amplitudeCoarse->setValue(amplitudeBits/100);
    updateAmpFineLimit();
    ui->amplitudeFine->setValue(amplitudeBits%100);
    displayAmplitude();
    int dcBiasPercent = roundf(m_settings.m_dcFactor * 100.0f);
    ui->dcBiasText->setText(QString(tr("%1 %").arg(dcBiasPercent)));
    int iBiasPercent = roundf(m_settings.m_iFactor * 100.0f);
    ui->iBiasText->setText(QString(tr("%1 %").arg(iBiasPercent)));
    int qBiasPercent = roundf(m_settings.m_qFactor * 100.0f);
    ui->qBiasText->setText(QString(tr("%1 %").arg(qBiasPercent)));
    ui->dcOffset->setChecked(m_settings.m_dcBlock);
    ui->iqImbalance->setChecked(m_settings.m_iqImbalance);

    ui->sampleSize->blockSignals(false);
    blockApplySettings(false);
}

void TestSourceGui::sendSettings()
{
    if(!m_updateTimer.isActive()) {
        m_updateTimer.start(100);
    }
}

void TestSourceGui::updateHardware()
{
    if (m_doApplySettings)
    {
        TestSourceInput::MsgConfigureTestSource* message = TestSourceInput::MsgConfigureTestSource::create(m_settings, m_forceSettings);
        m_sampleSource->getInputMessageQueue()->push(message);
        m_forceSettings = false;
        m_updateTimer.stop();
    }
}

void TestSourceGui::updateStatus()
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

bool TestSourceGui::handleMessage(const Message& message)
{
    if (TestSourceInput::MsgConfigureTestSource::match(message))
    {
        qDebug("TestSourceGui::handleMessage: MsgConfigureTestSource");
        const TestSourceInput::MsgConfigureTestSource& cfg = (TestSourceInput::MsgConfigureTestSource&) message;
        m_settings = cfg.getSettings();
        displaySettings();
        return true;
    }
    else if (TestSourceInput::MsgStartStop::match(message))
    {
        qDebug("TestSourceGui::handleMessage: MsgStartStop");
        TestSourceInput::MsgStartStop& notif = (TestSourceInput::MsgStartStop&) message;
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

void TestSourceGui::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_deviceSampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            qDebug("TestSourceGui::handleInputMessages: DSPSignalNotification: SampleRate:%d, CenterFrequency:%llu",
                    notif->getSampleRate(),
                    notif->getCenterFrequency());
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

void TestSourceGui::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_deviceSampleRate);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    ui->deviceRateText->setText(tr("%1k").arg((float)m_deviceSampleRate / 1000));
}


