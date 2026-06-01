///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
// Copyright (C) 2022-2023 Jon Beniston, M7RCE <jon@beniston.com>                //
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
#include <QtGlobal>

#include <QTime>
#include <QDateTime>
#include <QString>
#include <QMessageBox>
#include <QFileDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QPair>
#include <QList>

#include "ui_fobosgui.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "gui/basicdevicesettingsdialog.h"
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "mainspectrum/mainspectrumgui.h"
#include "dsp/dspcommands.h"
#include "util/db.h"

#include "fobosgui.h"
#include "device/deviceapi.h"
#include "device/deviceuiset.h"

FOBOSGui::FOBOSGui(DeviceUISet *deviceUISet, QWidget* parent) :
    DeviceGUI(parent),
    ui(new Ui::FOBOSGui),
    m_settings(),
    m_doApplySettings(true),
    m_forceSettings(true),
    m_sampleSource(0),
    m_tickCount(0),
    m_lastEngineState(DeviceAPI::StNotStarted),
    m_backendStatus(QStringLiteral("Not started")),
    m_backendDetails(QStringLiteral("Backend mode: Auto. Start the source to detect Agile or Regular API."))
{
    qDebug("FOBOSGui::FOBOSGui");
    m_deviceUISet = deviceUISet;
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_sampleSource = m_deviceUISet->m_deviceAPI->getSampleSource();

    ui->setupUi(getContents());
    sizeToContents();
    getContents()->setStyleSheet("#FOBOSGui { background-color: rgb(64, 64, 64); }");
    m_helpURL = "plugins/samplesource/FOBOS/readme.md";
    ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->centerFrequency->setValueRange(9, 0, 999999999);
    ui->sampleRate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
    ui->sampleRate->setValueRange(8, 8000000, 50000000);
    ui->frequencyShift->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->frequencyShift->setValueRange(false, 7, -9999999, 9999999);
    ui->frequencyShiftLabel->setText(QString("%1").arg(QChar(0x94, 0x03)));

    // Fobos SDR: Auto backend selection with controlled restart, live gain control and device-busy diagnostics.
    // Device stop/start lifecycle is handled by FOBOSWorker.
    ui->autoCorrLabel->setText(QStringLiteral("LNA"));
    ui->autoCorr->clear();
    ui->autoCorr->addItem(QStringLiteral("0"));
    ui->autoCorr->addItem(QStringLiteral("1"));
    ui->autoCorr->addItem(QStringLiteral("2"));

    ui->modulationLabel->setText(QStringLiteral("VGA"));
    ui->modulation->clear();
    for (int i = 0; i <= 31; ++i) {
        ui->modulation->addItem(QString::number(i));
    }

    ui->samplerateLabel->setText(QStringLiteral("SR"));
    ui->sampleRateUnit->setText(QStringLiteral("S/s"));
    ui->amplitudeCoarseLabel->setText(QStringLiteral("IQ scale coarse"));
    ui->amplitudeFineLabel->setText(QStringLiteral("IQ scale fine"));

    // Hide TestSource-only controls that are not part of the real Fobos source.
    // Planned uSDR-style controls for later sub-iterations: Input RF/HF modes, BW %, GPO, External clock.
    ui->decimationLabel->setVisible(false);
    ui->decimation->setVisible(false);
    ui->fcPosLabel->setVisible(false);
    ui->fcPos->setVisible(false);
    ui->sampleSzieLabel->setVisible(false);
    ui->sampleSize->setVisible(false);
    ui->sampleSizeUnits->setVisible(false);
    ui->frequencyShiftLabel->setVisible(false);
    ui->frequencyShift->setVisible(false);
    ui->frequencyShiftUnits->setVisible(false);
    ui->modulationFrequency->setVisible(false);
    ui->modulationFrequencyText->setVisible(false);
    ui->amModulationLabel->setVisible(false);
    ui->amModulation->setVisible(false);
    ui->amModulationText->setVisible(false);
    ui->fmDeviationLabel->setVisible(false);
    ui->fmDeviation->setVisible(false);
    ui->fmDeviationText->setVisible(false);
    ui->dcBiasLabel->setVisible(false);
    ui->dcBiasMinusLabel->setVisible(false);
    ui->dcBias->setVisible(false);
    ui->dcBiasPlusLabel->setVisible(false);
    ui->dcBiasText->setVisible(false);
    ui->iBiasLabel->setVisible(false);
    ui->iBiasMinusLabel->setVisible(false);
    ui->iBias->setVisible(false);
    ui->iBiasText->setVisible(false);
    ui->iBiasPlusLabel->setVisible(false);
    ui->qBiasLabel->setVisible(false);
    ui->qBiasMinusLabel->setVisible(false);
    ui->qBias->setVisible(false);
    ui->qBiasText->setVisible(false);
    ui->qBiasPlusQLabel->setVisible(false);
    ui->phaseImbalanceLabel->setVisible(false);
    ui->phaseImbalanceMinusLabel->setVisible(false);
    ui->phaseImbalance->setVisible(false);
    ui->phaseImbalanceText->setVisible(false);
    ui->phaseImbalancePlusLabel->setVisible(false);
    ui->line->setVisible(false);
    ui->line_4->setVisible(false);

    ui->power->setText(QStringLiteral("Stopped"));
    ui->autoCorr->setToolTip(QStringLiteral("Fobos LNA gain 0..2"));
    ui->modulation->setToolTip(QStringLiteral("Fobos VGA gain 0..31"));
    ui->sampleRate->setToolTip(QStringLiteral("Fobos sample rate in S/s. Supported rates are read from API on Start. Changing sample rate while running requires restart."));
    ui->amplitudeCoarse->setToolTip(QStringLiteral("Digital IQ display scale coarse: value/100. Live software display gain."));
    ui->amplitudeFine->setToolTip(QStringLiteral("Digital IQ display scale fine: value/100. Live software display gain."));
    ui->centerFrequency->setToolTip(QStringLiteral("Center frequency. Live retune is supported in RF mode."));
    ui->power->setToolTip(QStringLiteral("Fobos source status."));

    displaySettings();
    makeUIConnections();

    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
    connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    m_statusTimer.start(500);

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
    m_sampleSource->setMessageQueueToGUI(&m_inputMessageQueue);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(openDeviceSettingsDialog(const QPoint &)));
    DialPopup::addPopupsToChildDials(this);
    m_resizer.enableChildMouseTracking();
}

FOBOSGui::~FOBOSGui()
{
    m_statusTimer.stop();
    m_updateTimer.stop();
    delete ui;
}

void FOBOSGui::destroy()
{
    delete this;
}

void FOBOSGui::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    m_forceSettings = true;
    sendSettings();
}

QByteArray FOBOSGui::serialize() const
{
    return m_settings.serialize();
}

bool FOBOSGui::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
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

void FOBOSGui::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        FOBOSInput::MsgStartStop *message = FOBOSInput::MsgStartStop::create(checked);
        m_sampleSource->getInputMessageQueue()->push(message);
    }
}

void FOBOSGui::on_centerFrequency_changed(quint64 value)
{
    m_settings.m_centerFrequency = value * 1000;
    m_settingsKeys.append("centerFrequency");
    sendSettings();
}

void FOBOSGui::on_autoCorr_currentIndexChanged(int index)
{
    if ((index < 0) || (index > 2)) {
        return;
    }

    m_settings.m_autoCorrOptions = static_cast<FOBOSSettings::AutoCorrOptions>(index);
    m_settingsKeys.append("autoCorrOptions");
    sendSettings();
}

void FOBOSGui::on_frequencyShift_changed(qint64 value)
{
    m_settings.m_frequencyShift = value;
    m_settingsKeys.append("frequencyShift");
    sendSettings();
}

void FOBOSGui::on_decimation_currentIndexChanged(int index)
{
    if ((index < 0) || (index > 6)) {
        return;
    }

    m_settings.m_log2Decim = index;
    m_settingsKeys.append("log2Decim");
    sendSettings();
}

void FOBOSGui::on_fcPos_currentIndexChanged(int index)
{
    if ((index < 0) || (index > 2)) {
        return;
    }

    m_settings.m_fcPos = (FOBOSSettings::fcPos_t) index;
    m_settingsKeys.append("fcPos");
    sendSettings();
}

void FOBOSGui::on_sampleRate_changed(quint64 value)
{
    updateFrequencyShiftLimit();
    m_settings.m_frequencyShift = ui->frequencyShift->getValueNew();
    m_settings.m_sampleRate = value;
    m_settingsKeys.append("frequencyShift");
    m_settingsKeys.append("sampleRate");
    sendSettings();
}

void FOBOSGui::on_sampleSize_currentIndexChanged(int index)
{
    if ((index < 0) || (index > 2)) {
        return;
    }

    updateAmpCoarseLimit();
    updateAmpFineLimit();
    displayAmplitude();
    m_settings.m_amplitudeBits = ui->amplitudeCoarse->value() * 100 + ui->amplitudeFine->value();
    m_settings.m_sampleSizeIndex = index;
    m_settingsKeys.append("amplitudeBits");
    m_settingsKeys.append("sampleSizeIndex");
    sendSettings();
}

void FOBOSGui::on_amplitudeCoarse_valueChanged(int value)
{
    (void) value;
    updateAmpFineLimit();
    displayAmplitude();
    m_settings.m_amplitudeBits = ui->amplitudeCoarse->value() * 100 + ui->amplitudeFine->value();
    m_settingsKeys.append("amplitudeBits");
    sendSettings();
}

void FOBOSGui::on_amplitudeFine_valueChanged(int value)
{
    (void) value;
    displayAmplitude();
    m_settings.m_amplitudeBits = ui->amplitudeCoarse->value() * 100 + ui->amplitudeFine->value();
    m_settingsKeys.append("amplitudeBits");
    sendSettings();
}

void FOBOSGui::on_modulation_currentIndexChanged(int index)
{
    if ((index < 0) || (index > 31)) {
        return;
    }

    m_settings.m_modulation = static_cast<FOBOSSettings::Modulation>(index);
    m_settingsKeys.append("modulation");
    sendSettings();
}

void FOBOSGui::on_modulationFrequency_valueChanged(int value)
{
    m_settings.m_modulationTone = value;
    ui->modulationFrequencyText->setText(QString("%1").arg(m_settings.m_modulationTone / 100.0, 0, 'f', 2));
    m_settingsKeys.append("modulationTone");
    sendSettings();
}

void FOBOSGui::on_amModulation_valueChanged(int value)
{
    m_settings.m_amModulation = value;
    ui->amModulationText->setText(QString("%1").arg(m_settings.m_amModulation));
    m_settingsKeys.append("amModulation");
    sendSettings();
}

void FOBOSGui::on_fmDeviation_valueChanged(int value)
{
    m_settings.m_fmDeviation = value;
    ui->fmDeviationText->setText(QString("%1").arg(m_settings.m_fmDeviation / 10.0, 0, 'f', 1));
    m_settingsKeys.append("fmDeviation");
    sendSettings();
}

void FOBOSGui::on_dcBias_valueChanged(int value)
{
    ui->dcBiasText->setText(QString(tr("%1 %").arg(value)));
    m_settings.m_dcFactor = value / 100.0f;
    m_settingsKeys.append("dcFactor");
    sendSettings();
}

void FOBOSGui::on_iBias_valueChanged(int value)
{
    ui->iBiasText->setText(QString(tr("%1 %").arg(value)));
    m_settings.m_iFactor = value / 100.0f;
    m_settingsKeys.append("iFactor");
    sendSettings();
}

void FOBOSGui::on_qBias_valueChanged(int value)
{
    ui->qBiasText->setText(QString(tr("%1 %").arg(value)));
    m_settings.m_qFactor = value / 100.0f;
    m_settingsKeys.append("qFactor");
    sendSettings();
}

void FOBOSGui::on_phaseImbalance_valueChanged(int value)
{
    ui->phaseImbalanceText->setText(QString(tr("%1 %").arg(value)));
    m_settings.m_phaseImbalance = value / 100.0f;
    m_settingsKeys.append("phaseImbalance");
    sendSettings();
}

void FOBOSGui::displayAmplitude()
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

    const double iqGain = amplitudeInt / 100.0;
    ui->amplitudeBits->setText(QString(tr("x%1").arg(QString::number(iqGain, 'f', 2))));
    (void) power;
    // Fobos SDR: ui->power is reserved for device status.
    // Do not overwrite Running/Stopped/Error with a legacy TestSource dB display.
    ui->power->setToolTip(QString(tr("Digital IQ display scale x%1. This is software display gain, not RF gain."))
        .arg(QString::number(iqGain, 'f', 2)));
}

void FOBOSGui::updateAmpCoarseLimit()
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

void FOBOSGui::updateAmpFineLimit()
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

void FOBOSGui::updateFrequencyShiftLimit()
{
    int sampleRate = ui->sampleRate->getValueNew();
    ui->frequencyShift->setValueRange(false, 7, -sampleRate, sampleRate);
}

void FOBOSGui::displaySettings()
{
    blockApplySettings(true);
    ui->sampleSize->blockSignals(true);

    setTitle(m_settings.m_title);
    getDeviceUISet()->m_mainSpectrumGUI->setTitle(m_settings.m_title);
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
    ui->dcBias->setValue((int) dcBiasPercent);
    ui->dcBiasText->setText(QString(tr("%1 %").arg(dcBiasPercent)));
    int iBiasPercent = roundf(m_settings.m_iFactor * 100.0f);
    ui->iBias->setValue((int) iBiasPercent);
    ui->iBiasText->setText(QString(tr("%1 %").arg(iBiasPercent)));
    int qBiasPercent = roundf(m_settings.m_qFactor * 100.0f);
    ui->qBias->setValue((int) qBiasPercent);
    ui->qBiasText->setText(QString(tr("%1 %").arg(qBiasPercent)));
    int phaseImbalancePercent = roundf(m_settings.m_phaseImbalance * 100.0f);
    ui->phaseImbalance->setValue((int) phaseImbalancePercent);
    ui->phaseImbalanceText->setText(QString(tr("%1 %").arg(phaseImbalancePercent)));
    ui->autoCorr->setCurrentIndex(m_settings.m_autoCorrOptions);
    ui->sampleSize->blockSignals(false);
    ui->modulation->setCurrentIndex((int) m_settings.m_modulation);
    ui->modulationFrequency->setValue(m_settings.m_modulationTone);
    ui->modulationFrequencyText->setText(QString("%1").arg(m_settings.m_modulationTone / 100.0, 0, 'f', 2));
    ui->amModulation->setValue(m_settings.m_amModulation);
    ui->amModulationText->setText(QString("%1").arg(m_settings.m_amModulation));
    ui->fmDeviation->setValue(m_settings.m_fmDeviation);
    ui->fmDeviationText->setText(QString("%1").arg(m_settings.m_fmDeviation / 10.0, 0, 'f', 1));
    blockApplySettings(false);
}

void FOBOSGui::sendSettings()
{
    if (!m_updateTimer.isActive()) {
        m_updateTimer.start(100);
    }
}

void FOBOSGui::updateHardware()
{
    if (m_doApplySettings)
    {
        FOBOSInput::MsgConfigureFOBOS* message = FOBOSInput::MsgConfigureFOBOS::create(m_settings, m_settingsKeys, m_forceSettings);
        m_sampleSource->getInputMessageQueue()->push(message);
        m_forceSettings = false;
        m_settingsKeys.clear();
        m_updateTimer.stop();
    }
}

void FOBOSGui::updateStatus()
{
    int state = m_deviceUISet->m_deviceAPI->state();

    if(m_lastEngineState != state)
    {
        switch(state)
        {
            case DeviceAPI::StNotStarted:
                ui->startStop->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
                ui->power->setText(QStringLiteral("Stopped"));
                break;
            case DeviceAPI::StIdle:
                ui->startStop->setStyleSheet("QToolButton { background-color : blue; }");
                ui->power->setText(QStringLiteral("Idle"));
                break;
            case DeviceAPI::StRunning:
                ui->startStop->setStyleSheet("QToolButton { background-color : green; }");
                ui->power->setText(QStringLiteral("Running (%1)").arg(m_backendStatus));
                break;
            case DeviceAPI::StError:
                ui->startStop->setStyleSheet("QToolButton { background-color : red; }");
                ui->power->setText(QStringLiteral("Error"));
                QMessageBox::information(this, tr("Message"), m_deviceUISet->m_deviceAPI->errorMessage());
                break;
            default:
                break;
        }

        m_lastEngineState = state;
    }
}

bool FOBOSGui::handleMessage(const Message& message)
{
    if (FOBOSInput::MsgConfigureFOBOS::match(message))
    {
        qDebug("FOBOSGui::handleMessage: MsgConfigureFOBOS");
        const FOBOSInput::MsgConfigureFOBOS& cfg = (FOBOSInput::MsgConfigureFOBOS&) message;

        if (cfg.getForce()) {
            m_settings = cfg.getSettings();
        } else {
            m_settings.applySettings(cfg.getSettingsKeys(), cfg.getSettings());
        }

        displaySettings();
        return true;
    }
    else if (FOBOSInput::MsgReportFOBOSBackend::match(message))
    {
        const FOBOSInput::MsgReportFOBOSBackend& report = (const FOBOSInput::MsgReportFOBOSBackend&) message;
        m_backendStatus = report.getBackend();
        m_backendDetails = report.getDetails();
        if (m_deviceUISet->m_deviceAPI->state() == DeviceAPI::StRunning) {
            ui->power->setText(QStringLiteral("Running (%1)").arg(m_backendStatus));
        }
        return true;
    }
    else if (FOBOSInput::MsgStartStop::match(message))
    {
        qDebug("FOBOSGui::handleMessage: MsgStartStop");
        FOBOSInput::MsgStartStop& notif = (FOBOSInput::MsgStartStop&) message;
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

void FOBOSGui::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_deviceSampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            qDebug("FOBOSGui::handleInputMessages: DSPSignalNotification: SampleRate:%d, CenterFrequency:%llu",
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

void FOBOSGui::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_deviceSampleRate);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    ui->deviceRateText->setText(tr("%1k").arg((float)m_deviceSampleRate / 1000));
}

bool FOBOSGui::openFobosOperatorSettingsDialog(const QPoint& p)
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Fobos SDR settings"));
    dialog.setMinimumWidth(420);

    QVBoxLayout* mainLayout = new QVBoxLayout(&dialog);

    QGroupBox* idGroup = new QGroupBox(tr("Identification"), &dialog);
    QFormLayout* idLayout = new QFormLayout(idGroup);
    QLineEdit* nameEdit = new QLineEdit(m_settings.m_title, idGroup);
    QLabel* backendModeLabel = new QLabel(tr("Auto"), idGroup);
    QLabel* detectedBackendLabel = new QLabel(m_backendStatus, idGroup);
    QLabel* apiLabel = new QLabel(m_backendDetails, idGroup);
    apiLabel->setWordWrap(true);
    idLayout->addRow(tr("Name"), nameEdit);
    idLayout->addRow(tr("Backend mode"), backendModeLabel);
    idLayout->addRow(tr("Detected backend"), detectedBackendLabel);
    idLayout->addRow(tr("API / board"), apiLabel);
    mainLayout->addWidget(idGroup);

    QGroupBox* rxGroup = new QGroupBox(tr("Receiver"), &dialog);
    QFormLayout* rxLayout = new QFormLayout(rxGroup);

    QComboBox* inputCombo = new QComboBox(rxGroup);
    inputCombo->addItem(tr("RF"), (int) FOBOSSettings::InputRF);
    inputCombo->addItem(tr("IQ (HF1+j*HF2) direct sampling"), (int) FOBOSSettings::InputIQDirect);
    inputCombo->addItem(tr("HF1 direct sampling"), (int) FOBOSSettings::InputHF1Direct);
    inputCombo->addItem(tr("HF2 direct sampling"), (int) FOBOSSettings::InputHF2Direct);
    inputCombo->setCurrentIndex(qBound(0, (int) m_settings.m_inputMode, 3));
    inputCombo->setToolTip(tr("Input mode is applied on next Start. HF1/HF2 direct modes use software extraction from direct-sampling IQ."));

    QComboBox* srCombo = new QComboBox(rxGroup);
    srCombo->addItem(QStringLiteral("8"),    8000000);
    srCombo->addItem(QStringLiteral("10"),   10000000);
    srCombo->addItem(QStringLiteral("12.5"), 12500000);
    srCombo->addItem(QStringLiteral("16"),   16000000);
    srCombo->addItem(QStringLiteral("20"),   20000000);
    srCombo->addItem(QStringLiteral("25"),   25000000);
    srCombo->addItem(QStringLiteral("32"),   32000000);
    srCombo->addItem(QStringLiteral("40"),   40000000);
    srCombo->addItem(QStringLiteral("50"),   50000000);
    int srIndex = srCombo->findData((int) m_settings.m_sampleRate);
    if (srIndex < 0) { srIndex = srCombo->findData(25000000); }
    if (srIndex < 0) { srIndex = 0; }
    srCombo->setCurrentIndex(srIndex);
    srCombo->setToolTip(tr("Sample Rate is changed through a controlled restart while running. Supported values are logged from the active Fobos API."));

    QComboBox* bwCombo = new QComboBox(rxGroup);
    bwCombo->addItem(tr("100%"), 100);
    bwCombo->addItem(tr("90%"), 90);
    bwCombo->addItem(tr("80%"), 80);
    bwCombo->addItem(tr("70%"), 70);
    bwCombo->addItem(tr("60%"), 60);
    bwCombo->addItem(tr("50%"), 50);
    bwCombo->addItem(tr("40%"), 40);
    bwCombo->addItem(tr("30%"), 30);
    bwCombo->addItem(tr("20%"), 20);
    int bwIndex = bwCombo->findData((int) m_settings.m_bandwidthPercent);
    bwCombo->setCurrentIndex(bwIndex >= 0 ? bwIndex : 1);
    bwCombo->setToolTip(tr("Relative analog bandwidth. Applied by the Agile backend; shown for compatibility and ignored by the Regular backend."));

    rxLayout->addRow(tr("Input"), inputCombo);
    rxLayout->addRow(tr("Sample Rate, MSPS"), srCombo);
    rxLayout->addRow(tr("Bandwidth (Agile only)"), bwCombo);
    mainLayout->addWidget(rxGroup);

    QGroupBox* gainGroup = new QGroupBox(tr("Gain / display scale"), &dialog);
    QFormLayout* gainLayout = new QFormLayout(gainGroup);

    // Fobos SDR: uSDR-style gain sliders. LNA has only 0..2 steps, VGA has 0..31 steps.
    QWidget* lnaWidget = new QWidget(gainGroup);
    QHBoxLayout* lnaRow = new QHBoxLayout(lnaWidget);
    lnaRow->setContentsMargins(0, 0, 0, 0);
    QSlider* lnaSlider = new QSlider(Qt::Horizontal, lnaWidget);
    lnaSlider->setRange(0, 2);
    lnaSlider->setSingleStep(1);
    lnaSlider->setPageStep(1);
    lnaSlider->setTickPosition(QSlider::TicksBelow);
    lnaSlider->setTickInterval(1);
    lnaSlider->setValue(qBound(0, (int) m_settings.m_autoCorrOptions, 2));
    QLabel* lnaValue = new QLabel(QStringLiteral("#%1").arg(lnaSlider->value()), lnaWidget);
    lnaValue->setMinimumWidth(28);
    lnaRow->addWidget(lnaSlider, 1);
    lnaRow->addWidget(lnaValue);
    connect(lnaSlider, &QSlider::valueChanged, lnaValue, [lnaValue](int v) {
        lnaValue->setText(QStringLiteral("#%1").arg(v));
    });

    QWidget* vgaWidget = new QWidget(gainGroup);
    QHBoxLayout* vgaRow = new QHBoxLayout(vgaWidget);
    vgaRow->setContentsMargins(0, 0, 0, 0);
    QSlider* vgaSlider = new QSlider(Qt::Horizontal, vgaWidget);
    vgaSlider->setRange(0, 31);
    vgaSlider->setSingleStep(1);
    vgaSlider->setPageStep(4);
    vgaSlider->setTickPosition(QSlider::TicksBelow);
    vgaSlider->setTickInterval(4);
    vgaSlider->setValue(qBound(0, (int) m_settings.m_modulation, 31));
    QLabel* vgaValue = new QLabel(QStringLiteral("#%1").arg(vgaSlider->value()), vgaWidget);
    vgaValue->setMinimumWidth(32);
    vgaRow->addWidget(vgaSlider, 1);
    vgaRow->addWidget(vgaValue);
    connect(vgaSlider, &QSlider::valueChanged, vgaValue, [vgaValue](int v) {
        vgaValue->setText(QStringLiteral("#%1").arg(v));
    });

    QComboBox* scaleCombo = new QComboBox(gainGroup);
    const QList<int> scaleList = {1, 2, 4, 8, 16, 32, 64};
    int scaleIndex = 0;
    const int currentScale = qBound(1, m_settings.m_amplitudeBits / 100, 64);
    for (int i = 0; i < scaleList.size(); ++i) {
        scaleCombo->addItem(QStringLiteral("x%1").arg(scaleList[i]), scaleList[i]);
        if (scaleList[i] == currentScale) {
            scaleIndex = i;
        }
    }
    scaleCombo->setCurrentIndex(scaleIndex);

    gainLayout->addRow(tr("LNA Gain"), lnaWidget);
    gainLayout->addRow(tr("VGA Gain"), vgaWidget);
    gainLayout->addRow(tr("IQ Scale"), scaleCombo);
    mainLayout->addWidget(gainGroup);

    QGroupBox* policyGroup = new QGroupBox(tr("Apply behavior"), &dialog);
    QVBoxLayout* policyLayout = new QVBoxLayout(policyGroup);
    QLabel* liveNote = new QLabel(tr("Live while running: Frequency in RF mode, LNA, VGA, IQ Scale, GPO, and bandwidth where supported by the active backend."), policyGroup);
    QLabel* restartNote = new QLabel(tr("Controlled restart while running: Sample Rate, Input mode and External clock. SDRangel briefly stops and restarts Fobos so the spectrum scale matches real hardware settings."), policyGroup);
    liveNote->setWordWrap(true);
    restartNote->setWordWrap(true);
    policyLayout->addWidget(liveNote);
    policyLayout->addWidget(restartNote);
    mainLayout->addWidget(policyGroup);

    QGroupBox* gpioGroup = new QGroupBox(tr("GPO / clock"), &dialog);
    QVBoxLayout* gpioLayout = new QVBoxLayout(gpioGroup);
    QHBoxLayout* gpoRow = new QHBoxLayout();
    QList<QCheckBox*> gpoChecks;
    for (int i = 0; i < 8; ++i) {
        QCheckBox* cb = new QCheckBox(QString::number(i), gpioGroup);
        cb->setChecked((m_settings.m_gpoMask & (1u << i)) != 0u);
        gpoChecks.append(cb);
        gpoRow->addWidget(cb);
    }
    QCheckBox* externalClockCheck = new QCheckBox(tr("External clock"), gpioGroup);
    externalClockCheck->setChecked(m_settings.m_externalClock);
    externalClockCheck->setToolTip(tr("External 10 MHz reference. Applied through controlled restart while running."));

    // Live-apply and controlled-restart controls. OK closes with the latest values; Cancel no longer reverts
    // already-applied live changes. This avoids the operator having to close the dialog for LNA/VGA/IQ/GPO/BW tweaks.
    auto liveApply = [this]() {
        displaySettings();
        sendSettings();
    };
    connect(lnaSlider, &QSlider::valueChanged, this, [this, liveApply](int v) {
        m_settings.m_autoCorrOptions = static_cast<FOBOSSettings::AutoCorrOptions>(qBound(0, v, 2));
        m_settingsKeys.append("autoCorrOptions");
        liveApply();
    });
    connect(vgaSlider, &QSlider::valueChanged, this, [this, liveApply](int v) {
        m_settings.m_modulation = static_cast<FOBOSSettings::Modulation>(qBound(0, v, 31));
        m_settingsKeys.append("modulation");
        liveApply();
    });
    connect(scaleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, scaleCombo, liveApply](int) {
        const int iqScale = scaleCombo->currentData().toInt();
        m_settings.m_amplitudeBits = qBound(1, iqScale, 64) * 100;
        m_settingsKeys.append("amplitudeBits");
        liveApply();
    });
    connect(bwCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, bwCombo, liveApply](int) {
        m_settings.m_bandwidthPercent = bwCombo->currentData().toUInt();
        m_settingsKeys.append("bandwidthPercent");
        liveApply();
    });
    connect(inputCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, inputCombo, liveApply](int) {
        const int inputMode = inputCombo->currentData().toInt();
        m_settings.m_inputMode = static_cast<FOBOSSettings::InputMode>(qBound(0, inputMode, 3));
        m_settingsKeys.append("inputMode");
        liveApply();
    });
    connect(srCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, srCombo, liveApply](int) {
        m_settings.m_sampleRate = srCombo->currentData().toUInt();
        m_settingsKeys.append("sampleRate");
        liveApply();
    });
    connect(externalClockCheck, &QCheckBox::toggled, this, [this, externalClockCheck, liveApply](bool) {
        m_settings.m_externalClock = externalClockCheck->isChecked();
        m_settingsKeys.append("externalClock");
        liveApply();
    });
    for (int i = 0; i < gpoChecks.size(); ++i) {
        connect(gpoChecks[i], &QCheckBox::toggled, this, [this, gpoChecks, liveApply](bool) {
            unsigned int gpoMask = 0;
            for (int bit = 0; bit < gpoChecks.size(); ++bit) {
                if (gpoChecks[bit]->isChecked()) { gpoMask |= (1u << bit); }
            }
            m_settings.m_gpoMask = gpoMask & 0xffu;
            m_settingsKeys.append("gpoMask");
            liveApply();
        });
    }
    QLabel* apiNote = new QLabel(tr("Fobos SDR source. Backend mode: Auto. Agile API is tried first; regular/classic API is used as fallback. Bandwidth control is applied by Agile and shown only as an informational setting for Regular."), gpioGroup);
    apiNote->setWordWrap(true);
    gpioLayout->addLayout(gpoRow);
    gpioLayout->addWidget(externalClockCheck);
    gpioLayout->addWidget(apiNote);
    mainLayout->addWidget(gpioGroup);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    mainLayout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    dialog.move(p);
    new DialogPositioner(&dialog, false);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    const QString newTitle = nameEdit->text().trimmed().isEmpty() ? QStringLiteral("Fobos SDR") : nameEdit->text().trimmed();
    m_settings.m_title = newTitle;
    setTitle(m_settings.m_title);
    getDeviceUISet()->m_mainSpectrumGUI->setTitle(m_settings.m_title);
    m_settingsKeys.append("title");

    const int inputMode = inputCombo->currentData().toInt();
    m_settings.m_inputMode = (FOBOSSettings::InputMode) qBound(0, inputMode, 3);
    m_settingsKeys.append("inputMode");

    m_settings.m_sampleRate = srCombo->currentData().toUInt();
    m_settingsKeys.append("sampleRate");

    const unsigned int bw = bwCombo->currentData().toUInt();
    m_settings.m_bandwidthPercent = bw;
    m_settingsKeys.append("bandwidthPercent");

    m_settings.m_autoCorrOptions = static_cast<FOBOSSettings::AutoCorrOptions>(qBound(0, lnaSlider->value(), 2));
    m_settingsKeys.append("autoCorrOptions");

    m_settings.m_modulation = static_cast<FOBOSSettings::Modulation>(qBound(0, vgaSlider->value(), 31));
    m_settingsKeys.append("modulation");

    const int iqScale = scaleCombo->currentData().toInt();
    m_settings.m_amplitudeBits = iqScale * 100;
    m_settingsKeys.append("amplitudeBits");

    unsigned int gpoMask = 0;
    for (int i = 0; i < gpoChecks.size(); ++i) {
        if (gpoChecks[i]->isChecked()) {
            gpoMask |= (1u << i);
        }
    }
    m_settings.m_gpoMask = gpoMask & 0xffu;
    m_settingsKeys.append("gpoMask");

    m_settings.m_externalClock = externalClockCheck->isChecked();
    m_settingsKeys.append("externalClock");

    displaySettings();
    sendSettings();
    return true;
}

void FOBOSGui::openDeviceSettingsDialog(const QPoint& p)
{
    if (m_contextMenuType == ContextMenuDeviceSettings)
    {
        // Fobos SDR: uSDR-style operator settings dialog with controlled restart for critical settings.
        openFobosOperatorSettingsDialog(p);
    }

    resetContextMenuType();
}

void FOBOSGui::makeUIConnections()
{
    QObject::connect(ui->startStop, &ButtonSwitch::toggled, this, &FOBOSGui::on_startStop_toggled);
    QObject::connect(ui->centerFrequency, &ValueDial::changed, this, &FOBOSGui::on_centerFrequency_changed);
    QObject::connect(ui->autoCorr, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FOBOSGui::on_autoCorr_currentIndexChanged);
    QObject::connect(ui->frequencyShift, &ValueDialZ::changed, this, &FOBOSGui::on_frequencyShift_changed);
    QObject::connect(ui->decimation, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FOBOSGui::on_decimation_currentIndexChanged);
    QObject::connect(ui->fcPos, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FOBOSGui::on_fcPos_currentIndexChanged);
    QObject::connect(ui->sampleRate, &ValueDial::changed, this, &FOBOSGui::on_sampleRate_changed);
    QObject::connect(ui->sampleSize, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FOBOSGui::on_sampleSize_currentIndexChanged);
    QObject::connect(ui->amplitudeCoarse, &QSlider::valueChanged, this, &FOBOSGui::on_amplitudeCoarse_valueChanged);
    QObject::connect(ui->amplitudeFine, &QSlider::valueChanged, this, &FOBOSGui::on_amplitudeFine_valueChanged);
    QObject::connect(ui->modulation, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FOBOSGui::on_modulation_currentIndexChanged);
    QObject::connect(ui->modulationFrequency, &QDial::valueChanged, this, &FOBOSGui::on_modulationFrequency_valueChanged);
    QObject::connect(ui->amModulation, &QDial::valueChanged, this, &FOBOSGui::on_amModulation_valueChanged);
    QObject::connect(ui->fmDeviation, &QDial::valueChanged, this, &FOBOSGui::on_fmDeviation_valueChanged);
    QObject::connect(ui->dcBias, &QSlider::valueChanged, this, &FOBOSGui::on_dcBias_valueChanged);
    QObject::connect(ui->iBias, &QSlider::valueChanged, this, &FOBOSGui::on_iBias_valueChanged);
    QObject::connect(ui->qBias, &QSlider::valueChanged, this, &FOBOSGui::on_qBias_valueChanged);
    QObject::connect(ui->phaseImbalance, &QSlider::valueChanged, this, &FOBOSGui::on_phaseImbalance_valueChanged);
}
