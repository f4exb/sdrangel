///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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
#include "gui/basicdevicesettingsdialog.h"
#include "dsp/dspengine.h"
#include "dsp/dspdevicemimoengine.h"
#include "dsp/dspcommands.h"
#include "util/db.h"

#include "mainwindow.h"

#include "ui_testmigui.h"
#include "testmigui.h"

TestMIGui::TestMIGui(DeviceUISet *deviceUISet, QWidget* parent) :
    DeviceGUI(parent),
    ui(new Ui::TestMIGui),
    m_settings(),
    m_streamIndex(0),
    m_spectrumStreamIndex(0),
    m_doApplySettings(true),
    m_forceSettings(true),
    m_sampleMIMO(nullptr),
    m_tickCount(0),
    m_lastEngineState(DeviceAPI::StNotStarted)
{
    qDebug("TestMIGui::TestMIGui");
    m_deviceUISet = deviceUISet;
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/samplemimo/testmi/readme.md";
    ui->setupUi(getContents());
    sizeToContents();
    getContents()->setStyleSheet("#TestMIGui { background-color: rgb(64, 64, 64); }");

    m_sampleMIMO = m_deviceUISet->m_deviceAPI->getSampleMIMO();
    m_streamIndex = 0;
    m_deviceCenterFrequencies.push_back(m_settings.m_streams[0].m_centerFrequency);
    m_deviceCenterFrequencies.push_back(m_settings.m_streams[1].m_centerFrequency);
    m_deviceSampleRates.push_back(m_settings.m_streams[0].m_sampleRate / (1<<m_settings.m_streams[0].m_log2Decim));
    m_deviceSampleRates.push_back(m_settings.m_streams[1].m_sampleRate / (1<<m_settings.m_streams[1].m_log2Decim));

    ui->spectrumSource->addItem("0");
    ui->spectrumSource->addItem("1");
    ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->centerFrequency->setValueRange(9, 0, 999999999);
    ui->sampleRate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
    ui->sampleRate->setValueRange(7, 48000, 9999999);
    ui->frequencyShift->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->frequencyShift->setValueRange(false, 7, -9999999, 9999999);
    ui->frequencyShiftLabel->setText(QString("%1").arg(QChar(0x94, 0x03)));

    displaySettings();

    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
    connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    m_statusTimer.start(500);

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
    m_sampleMIMO->setMessageQueueToGUI(&m_inputMessageQueue);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(openDeviceSettingsDialog(const QPoint &)));

    makeUIConnections();
}

TestMIGui::~TestMIGui()
{
    delete ui;
}

void TestMIGui::destroy()
{
    delete this;
}

void TestMIGui::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    sendSettings();
}

QByteArray TestMIGui::serialize() const
{
    return m_settings.serialize();
}

bool TestMIGui::deserialize(const QByteArray& data)
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

void TestMIGui::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        TestMI::MsgStartStop *message = TestMI::MsgStartStop::create(checked);
        m_sampleMIMO->getInputMessageQueue()->push(message);
    }
}

void TestMIGui::on_streamIndex_currentIndexChanged(int index)
{
    if (ui->streamLock->isChecked())
    {
        m_spectrumStreamIndex = index;
        m_deviceUISet->m_spectrum->setDisplayedStream(true, index);
        m_deviceUISet->m_deviceAPI->setSpectrumSinkInput(true, m_spectrumStreamIndex);
        ui->spectrumSource->blockSignals(true);
        ui->spectrumSource->setCurrentIndex(index);
        ui->spectrumSource->blockSignals(false);
    }

    m_streamIndex = index;
    updateSampleRateAndFrequency();
    displaySettings();
}

void TestMIGui::on_spectrumSource_currentIndexChanged(int index)
{
    m_spectrumStreamIndex = index;
    m_deviceUISet->m_spectrum->setDisplayedStream(true, index);
    m_deviceUISet->m_deviceAPI->setSpectrumSinkInput(true, m_spectrumStreamIndex);
    updateSampleRateAndFrequency();

    if (ui->streamLock->isChecked())
    {
        ui->streamIndex->blockSignals(true);
        ui->streamIndex->setCurrentIndex(index);
        ui->streamIndex->blockSignals(false);
        m_streamIndex = index;
        displaySettings();
    }
}

void TestMIGui::on_streamLock_toggled(bool checked)
{
    if (checked && (ui->streamIndex->currentIndex() != ui->spectrumSource->currentIndex())) {
        ui->spectrumSource->setCurrentIndex(ui->streamIndex->currentIndex());
    }
}

void TestMIGui::on_centerFrequency_changed(quint64 value)
{
    m_settings.m_streams[m_streamIndex].m_centerFrequency = value * 1000;
    sendSettings();
}

void TestMIGui::on_autoCorr_currentIndexChanged(int index)
{
    if ((index < 0) || (index > TestMIStreamSettings::AutoCorrLast)) {
        return;
    }

    m_settings.m_streams[m_streamIndex].m_autoCorrOptions = (TestMIStreamSettings::AutoCorrOptions) index;
    sendSettings();
}

void TestMIGui::on_frequencyShift_changed(qint64 value)
{
    m_settings.m_streams[m_streamIndex].m_frequencyShift = value;
    sendSettings();
}

void TestMIGui::on_decimation_currentIndexChanged(int index)
{
    if ((index < 0) || (index > 6)) {
        return;
    }

    m_settings.m_streams[m_streamIndex].m_log2Decim = index;
    sendSettings();
}

void TestMIGui::on_fcPos_currentIndexChanged(int index)
{
    if ((index < 0) || (index > 2)) {
        return;
    }

    m_settings.m_streams[m_streamIndex].m_fcPos = (TestMIStreamSettings::fcPos_t) index;
    sendSettings();
}

void TestMIGui::on_sampleRate_changed(quint64 value)
{
    updateFrequencyShiftLimit();
    m_settings.m_streams[m_streamIndex].m_frequencyShift = ui->frequencyShift->getValueNew();
    m_settings.m_streams[m_streamIndex].m_sampleRate = value;
    sendSettings();
}

void TestMIGui::on_sampleSize_currentIndexChanged(int index)
{
    if ((index < 0) || (index > 2)) {
        return;
    }

    updateAmpCoarseLimit();
    updateAmpFineLimit();
    displayAmplitude();
    m_settings.m_streams[m_streamIndex].m_amplitudeBits = ui->amplitudeCoarse->value() * 100 + ui->amplitudeFine->value();
    m_settings.m_streams[m_streamIndex].m_sampleSizeIndex = index;
    sendSettings();
}

void TestMIGui::on_amplitudeCoarse_valueChanged(int value)
{
    (void) value;
    updateAmpFineLimit();
    displayAmplitude();
    m_settings.m_streams[m_streamIndex].m_amplitudeBits = ui->amplitudeCoarse->value() * 100 + ui->amplitudeFine->value();
    sendSettings();
}

void TestMIGui::on_amplitudeFine_valueChanged(int value)
{
    (void) value;
    displayAmplitude();
    m_settings.m_streams[m_streamIndex].m_amplitudeBits = ui->amplitudeCoarse->value() * 100 + ui->amplitudeFine->value();
    sendSettings();
}

void TestMIGui::on_modulation_currentIndexChanged(int index)
{
    if ((index < 0) || (index > TestMIStreamSettings::ModulationLast)) {
        return;
    }

    m_settings.m_streams[m_streamIndex].m_modulation = (TestMIStreamSettings::Modulation) index;
    sendSettings();
}

void TestMIGui::on_modulationFrequency_valueChanged(int value)
{
    m_settings.m_streams[m_streamIndex].m_modulationTone = value;
    ui->modulationFrequencyText->setText(QString("%1").arg(m_settings.m_streams[m_streamIndex].m_modulationTone / 100.0, 0, 'f', 2));
    sendSettings();
}

void TestMIGui::on_amModulation_valueChanged(int value)
{
    m_settings.m_streams[m_streamIndex].m_amModulation = value;
    ui->amModulationText->setText(QString("%1").arg(m_settings.m_streams[m_streamIndex].m_amModulation));
    sendSettings();
}

void TestMIGui::on_fmDeviation_valueChanged(int value)
{
    m_settings.m_streams[m_streamIndex].m_fmDeviation = value;
    ui->fmDeviationText->setText(QString("%1").arg(m_settings.m_streams[m_streamIndex].m_fmDeviation / 10.0, 0, 'f', 1));
    sendSettings();
}

void TestMIGui::on_dcBias_valueChanged(int value)
{
    ui->dcBiasText->setText(QString(tr("%1 %").arg(value)));
    m_settings.m_streams[m_streamIndex].m_dcFactor = value / 100.0f;
    sendSettings();
}

void TestMIGui::on_iBias_valueChanged(int value)
{
    ui->iBiasText->setText(QString(tr("%1 %").arg(value)));
    m_settings.m_streams[m_streamIndex].m_iFactor = value / 100.0f;
    sendSettings();
}

void TestMIGui::on_qBias_valueChanged(int value)
{
    ui->qBiasText->setText(QString(tr("%1 %").arg(value)));
    m_settings.m_streams[m_streamIndex].m_qFactor = value / 100.0f;
    sendSettings();
}

void TestMIGui::on_phaseImbalance_valueChanged(int value)
{
    ui->phaseImbalanceText->setText(QString(tr("%1 %").arg(value)));
    m_settings.m_streams[m_streamIndex].m_phaseImbalance = value / 100.0f;
    sendSettings();
}

void TestMIGui::displayAmplitude()
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

void TestMIGui::updateAmpCoarseLimit()
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

void TestMIGui::updateAmpFineLimit()
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

void TestMIGui::updateFrequencyShiftLimit()
{
    qint64 sampleRate = ui->sampleRate->getValueNew();
    ui->frequencyShift->setValueRange(false, 7, -sampleRate, sampleRate);
    ui->frequencyShift->setValue(m_settings.m_streams[m_streamIndex].m_frequencyShift);
}

void TestMIGui::displaySettings()
{
    blockApplySettings(true);
    ui->sampleSize->blockSignals(true);

    ui->streamIndex->setCurrentIndex(m_streamIndex);
    ui->centerFrequency->setValue(m_settings.m_streams[m_streamIndex].m_centerFrequency / 1000);
    ui->decimation->setCurrentIndex(m_settings.m_streams[m_streamIndex].m_log2Decim);
    ui->fcPos->setCurrentIndex((int) m_settings.m_streams[m_streamIndex].m_fcPos);
    ui->sampleRate->setValue(m_settings.m_streams[m_streamIndex].m_sampleRate);
    updateFrequencyShiftLimit();
    ui->frequencyShift->setValue(m_settings.m_streams[m_streamIndex].m_frequencyShift);
    ui->sampleSize->setCurrentIndex(m_settings.m_streams[m_streamIndex].m_sampleSizeIndex);
    updateAmpCoarseLimit();
    int amplitudeBits = m_settings.m_streams[m_streamIndex].m_amplitudeBits;
    ui->amplitudeCoarse->setValue(amplitudeBits/100);
    updateAmpFineLimit();
    ui->amplitudeFine->setValue(amplitudeBits%100);
    displayAmplitude();
    int dcBiasPercent = roundf(m_settings.m_streams[m_streamIndex].m_dcFactor * 100.0f);
    ui->dcBias->setValue((int) dcBiasPercent);
    ui->dcBiasText->setText(QString(tr("%1 %").arg(dcBiasPercent)));
    int iBiasPercent = roundf(m_settings.m_streams[m_streamIndex].m_iFactor * 100.0f);
    ui->iBias->setValue((int) iBiasPercent);
    ui->iBiasText->setText(QString(tr("%1 %").arg(iBiasPercent)));
    int qBiasPercent = roundf(m_settings.m_streams[m_streamIndex].m_qFactor * 100.0f);
    ui->qBias->setValue((int) qBiasPercent);
    ui->qBiasText->setText(QString(tr("%1 %").arg(qBiasPercent)));
    int phaseImbalancePercent = roundf(m_settings.m_streams[m_streamIndex].m_phaseImbalance * 100.0f);
    ui->phaseImbalance->setValue((int) phaseImbalancePercent);
    ui->phaseImbalanceText->setText(QString(tr("%1 %").arg(phaseImbalancePercent)));
    ui->autoCorr->setCurrentIndex(m_settings.m_streams[m_streamIndex].m_autoCorrOptions);
    ui->sampleSize->blockSignals(false);
    ui->modulation->setCurrentIndex((int) m_settings.m_streams[m_streamIndex].m_modulation);
    ui->modulationFrequency->setValue(m_settings.m_streams[m_streamIndex].m_modulationTone);
    ui->modulationFrequencyText->setText(QString("%1").arg(m_settings.m_streams[m_streamIndex].m_modulationTone / 100.0, 0, 'f', 2));
    ui->amModulation->setValue(m_settings.m_streams[m_streamIndex].m_amModulation);
    ui->amModulationText->setText(QString("%1").arg(m_settings.m_streams[m_streamIndex].m_amModulation));
    ui->fmDeviation->setValue(m_settings.m_streams[m_streamIndex].m_fmDeviation);
    ui->fmDeviationText->setText(QString("%1").arg(m_settings.m_streams[m_streamIndex].m_fmDeviation / 10.0, 0, 'f', 1));
    blockApplySettings(false);
}

void TestMIGui::sendSettings()
{
    if(!m_updateTimer.isActive()) {
        m_updateTimer.start(100);
    }
}

void TestMIGui::updateHardware()
{
    if (m_doApplySettings)
    {
        TestMI::MsgConfigureTestSource* message = TestMI::MsgConfigureTestSource::create(m_settings, m_forceSettings);
        m_sampleMIMO->getInputMessageQueue()->push(message);
        m_forceSettings = false;
        m_updateTimer.stop();
    }
}

void TestMIGui::updateStatus()
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

bool TestMIGui::handleMessage(const Message& message)
{
    if (TestMI::MsgConfigureTestSource::match(message))
    {
        qDebug("TestMIGui::handleMessage: MsgConfigureTestSource");
        const TestMI::MsgConfigureTestSource& cfg = (TestMI::MsgConfigureTestSource&) message;
        m_settings = cfg.getSettings();
        displaySettings();
        return true;
    }
    else if (TestMI::MsgStartStop::match(message))
    {
        qDebug("TestMIGui::handleMessage: MsgStartStop");
        TestMI::MsgStartStop& notif = (TestMI::MsgStartStop&) message;
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

void TestMIGui::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        if (DSPMIMOSignalNotification::match(*message))
        {
            DSPMIMOSignalNotification* notif = (DSPMIMOSignalNotification*) message;
            int istream = notif->getIndex();
            bool sourceOrSink = notif->getSourceOrSink();
            m_deviceSampleRates[istream] = notif->getSampleRate();
            m_deviceCenterFrequencies[istream] = notif->getCenterFrequency();
            // Do not consider multiple sources at this time
            qDebug("TestMIGui::handleInputMessages: DSPMIMOSignalNotification: %s stream: %d SampleRate:%d, CenterFrequency:%llu",
                    sourceOrSink ? "source" : "sink",
                    istream,
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

void TestMIGui::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_deviceSampleRates[m_spectrumStreamIndex]);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_deviceCenterFrequencies[m_spectrumStreamIndex]);
    ui->deviceRateText->setText(tr("%1k").arg((float) m_deviceSampleRates[m_streamIndex] / 1000));
}

void TestMIGui::openDeviceSettingsDialog(const QPoint& p)
{
    if (m_contextMenuType == ContextMenuDeviceSettings)
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

    resetContextMenuType();
}

void TestMIGui::makeUIConnections()
{
    QObject::connect(ui->startStop, &ButtonSwitch::toggled, this, &TestMIGui::on_startStop_toggled);
    QObject::connect(ui->streamIndex, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TestMIGui::on_streamIndex_currentIndexChanged);
    QObject::connect(ui->spectrumSource, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TestMIGui::on_spectrumSource_currentIndexChanged);
    QObject::connect(ui->streamLock, &QToolButton::toggled, this, &TestMIGui::on_streamLock_toggled);
    QObject::connect(ui->centerFrequency, &ValueDial::changed, this, &TestMIGui::on_centerFrequency_changed);
    QObject::connect(ui->autoCorr, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TestMIGui::on_autoCorr_currentIndexChanged);
    QObject::connect(ui->frequencyShift, &ValueDialZ::changed, this, &TestMIGui::on_frequencyShift_changed);
    QObject::connect(ui->decimation, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TestMIGui::on_decimation_currentIndexChanged);
    QObject::connect(ui->fcPos, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TestMIGui::on_fcPos_currentIndexChanged);
    QObject::connect(ui->sampleRate, &ValueDial::changed, this, &TestMIGui::on_sampleRate_changed);
    QObject::connect(ui->sampleSize, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TestMIGui::on_sampleSize_currentIndexChanged);
    QObject::connect(ui->amplitudeCoarse, &QSlider::valueChanged, this, &TestMIGui::on_amplitudeCoarse_valueChanged);
    QObject::connect(ui->amplitudeFine, &QSlider::valueChanged, this, &TestMIGui::on_amplitudeFine_valueChanged);
    QObject::connect(ui->modulation, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TestMIGui::on_modulation_currentIndexChanged);
    QObject::connect(ui->modulationFrequency, &QDial::valueChanged, this, &TestMIGui::on_modulationFrequency_valueChanged);
    QObject::connect(ui->amModulation, &QDial::valueChanged, this, &TestMIGui::on_amModulation_valueChanged);
    QObject::connect(ui->fmDeviation, &QDial::valueChanged, this, &TestMIGui::on_fmDeviation_valueChanged);
    QObject::connect(ui->dcBias, &QSlider::valueChanged, this, &TestMIGui::on_dcBias_valueChanged);
    QObject::connect(ui->iBias, &QSlider::valueChanged, this, &TestMIGui::on_iBias_valueChanged);
    QObject::connect(ui->qBias, &QSlider::valueChanged, this, &TestMIGui::on_qBias_valueChanged);
    QObject::connect(ui->phaseImbalance, &QSlider::valueChanged, this, &TestMIGui::on_phaseImbalance_valueChanged);
}
