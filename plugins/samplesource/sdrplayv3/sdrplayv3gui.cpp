///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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
#include <QMessageBox>
#include <QResizeEvent>

#include "sdrplayv3gui.h"
#include "sdrplayv3input.h"

#include "device/deviceapi.h"
#include "device/deviceuiset.h"

#include "ui_sdrplayv3gui.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "gui/basicdevicesettingsdialog.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"

SDRPlayV3Gui::SDRPlayV3Gui(DeviceUISet *deviceUISet, QWidget* parent) :
    DeviceGUI(parent),
    ui(new Ui::SDRPlayV3Gui),
    m_doApplySettings(true),
    m_forceSettings(true)
{
    m_deviceUISet = deviceUISet;
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_sdrPlayV3Input = (SDRPlayV3Input*) m_deviceUISet->m_deviceAPI->getSampleSource();

    ui->setupUi(getContents());
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    getContents()->setStyleSheet("#SDRPlayV3Gui { background-color: rgb(64, 64, 64); }");
    m_helpURL = "plugins/samplesource/sdrplayv3/readme.md";
    ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    updateFrequencyLimits();

    ui->ifFrequency->clear();
    for (unsigned int i = 0; i < SDRPlayV3IF::getNbIFs(); i++)
    {
        ui->ifFrequency->addItem(QString::number(SDRPlayV3IF::getIF(i)/1000));
    }

    ui->samplerate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
    ui->samplerate->setValueRange(8, 2000000U, 10660000U);

    ui->bandwidth->clear();
    for (unsigned int i = 0; i < SDRPlayV3Bandwidths::getNbBandwidths(); i++)
    {
        ui->bandwidth->addItem(QString::number(SDRPlayV3Bandwidths::getBandwidth(i)/1000));
    }

    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
    connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    m_statusTimer.start(500);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(openDeviceSettingsDialog(const QPoint &)));

    ui->tuner->blockSignals(true);
    ui->antenna->blockSignals(true);
    ui->tuner->clear();
    ui->antenna->clear();
    switch (m_sdrPlayV3Input->getDeviceId())
    {
    case SDRPLAY_RSP1_ID:
        ui->tuner->addItem("1");
        ui->antenna->addItem("50Ohm");
        ui->amNotch->setVisible(false);
        ui->biasTee->setVisible(false);
        ui->extRef->setVisible(false);
        break;
    case SDRPLAY_RSP1A_ID:
        ui->tuner->addItem("1");
        ui->antenna->addItem("50Ohm");
        ui->amNotch->setVisible(false);
        ui->extRef->setVisible(false);
        break;
    case SDRPLAY_RSP2_ID:
        ui->tuner->addItem("1");
        ui->antenna->addItem("A");
        ui->antenna->addItem("B");
        ui->antenna->addItem("Hi-Z");
        ui->amNotch->setVisible(false);
        break;
    case SDRPLAY_RSPduo_ID:
        ui->tuner->addItem("1");
        ui->tuner->addItem("2");
        ui->antenna->addItem("50Ohm");
        ui->antenna->addItem("Hi-Z");
        ui->biasTee->setVisible(false);
        break;
    case SDRPLAY_RSPdx_ID:
        ui->tuner->addItem("1");
        ui->antenna->addItem("A");
        ui->antenna->addItem("B");
        ui->antenna->addItem("C");
        ui->amNotch->setVisible(false);
        ui->extRef->setVisible(false);
        break;
    }
    ui->tuner->blockSignals(false);
    ui->antenna->blockSignals(false);

    displaySettings();
    makeUIConnections();

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
    m_sdrPlayV3Input->setMessageQueueToGUI(&m_inputMessageQueue);
}

SDRPlayV3Gui::~SDRPlayV3Gui()
{
    m_statusTimer.stop();
    m_updateTimer.stop();
    delete ui;
}

void SDRPlayV3Gui::destroy()
{
    delete this;
}

void SDRPlayV3Gui::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    m_forceSettings = true;
    sendSettings();
}

QByteArray SDRPlayV3Gui::serialize() const
{
    return m_settings.serialize();
}

bool SDRPlayV3Gui::deserialize(const QByteArray& data)
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

void SDRPlayV3Gui::resizeEvent(QResizeEvent* size)
{
    adjustSize();
    size->accept();
}

bool SDRPlayV3Gui::handleMessage(const Message& message)
{
    if (SDRPlayV3Input::MsgConfigureSDRPlayV3::match(message))
    {
        const SDRPlayV3Input::MsgConfigureSDRPlayV3& cfg = (SDRPlayV3Input::MsgConfigureSDRPlayV3&) message;

        if (cfg.getForce()) {
            m_settings = cfg.getSettings();
        } else {
            m_settings.applySettings(cfg.getSettingsKeys(), cfg.getSettings());
        }

        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (SDRPlayV3Input::MsgStartStop::match(message))
    {
        SDRPlayV3Input::MsgStartStop& notif = (SDRPlayV3Input::MsgStartStop&) message;
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

void SDRPlayV3Gui::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        qDebug("SDRPlayV3Gui::handleInputMessages: message: %s", message->getIdentifier());

        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_sampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            qDebug("SDRPlayV3Gui::handleInputMessages: DSPSignalNotification: SampleRate:%d, CenterFrequency:%llu", notif->getSampleRate(), notif->getCenterFrequency());
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

void SDRPlayV3Gui::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_sampleRate);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    ui->deviceRateText->setText(tr("%1k").arg((float)m_sampleRate / 1000));
}

void SDRPlayV3Gui::updateFrequencyLimits()
{
    // values in kHz
    qint64 deltaFrequency = m_settings.m_transverterMode ? m_settings.m_transverterDeltaFrequency/1000 : 0;
    qint64 minLimit = 1U + deltaFrequency;
    qint64 maxLimit = 2000000U + deltaFrequency;

    if (m_settings.m_transverterMode)
    {
        minLimit = minLimit < 0 ? 0 : minLimit > 999999999 ? 999999999 : minLimit;
        maxLimit = maxLimit < 0 ? 0 : maxLimit > 999999999 ? 999999999 : maxLimit;
        ui->centerFrequency->setValueRange(9, minLimit, maxLimit);
    }
    else
    {
        minLimit = minLimit < 0 ? 0 : minLimit > 9999999 ? 9999999 : minLimit;
        maxLimit = maxLimit < 0 ? 0 : maxLimit > 9999999 ? 9999999 : maxLimit;
        ui->centerFrequency->setValueRange(7, minLimit, maxLimit);
    }
    qDebug("SDRPlayV3Gui::updateFrequencyLimits: delta: %lld min: %lld max: %lld", deltaFrequency, minLimit, maxLimit);
}

void SDRPlayV3Gui::displaySettings()
{
    ui->transverter->setDeltaFrequency(m_settings.m_transverterDeltaFrequency);
    ui->transverter->setDeltaFrequencyActive(m_settings.m_transverterMode);
    ui->transverter->setIQOrder(m_settings.m_iqOrder);
    updateFrequencyLimits();

    ui->centerFrequency->setValue(m_settings.m_centerFrequency / 1000);

    ui->ppm->setValue(m_settings.m_LOppmTenths);
    ui->ppmText->setText(QString("%1").arg(QString::number(m_settings.m_LOppmTenths/10.0, 'f', 1)));

    ui->tuner->setCurrentIndex(m_settings.m_tuner);
    ui->antenna->setCurrentIndex(m_settings.m_antenna);

    ui->samplerate->setValue(m_settings.m_devSampleRate);

    ui->dcOffset->setChecked(m_settings.m_dcBlock);
    ui->iqImbalance->setChecked(m_settings.m_iqCorrection);

    ui->extRef->setChecked(m_settings.m_extRef);
    ui->biasTee->setChecked(m_settings.m_biasTee);
    ui->amNotch->setChecked(m_settings.m_amNotch);
    ui->fmNotch->setChecked(m_settings.m_fmNotch);
    ui->dabNotch->setChecked(m_settings.m_dabNotch);

    ui->bandwidth->setCurrentIndex(m_settings.m_bandwidthIndex);
    ui->ifFrequency->setCurrentIndex(m_settings.m_ifFrequencyIndex);
    ui->samplerate->setValue(m_settings.m_devSampleRate);

    ui->decim->setCurrentIndex(m_settings.m_log2Decim);
    ui->fcPos->setCurrentIndex((int) m_settings.m_fcPos);

    updateLNAValues();
    ui->gainLNA->setCurrentIndex(m_settings.m_lnaIndex);
    if (m_settings.m_ifAGC)
    {
        ui->gainIFAGC->setChecked(true);
        ui->gainIF->setEnabled(false);
    }
    else
    {
        ui->gainIFAGC->setChecked(false);
        ui->gainIF->setEnabled(true);
    }
    int gain = m_settings.m_ifGain;
    ui->gainIF->setValue(gain);
    QString gainText = QStringLiteral("%1").arg(gain, 2, 10, QLatin1Char('0'));
    ui->gainIFText->setText(gainText);
}

void SDRPlayV3Gui::updateLNAValues()
{
    int currentValue = ui->gainLNA->currentText().toInt();
    bool found = false;

    const int *attenuations = SDRPlayV3LNA::getAttenuations(m_sdrPlayV3Input->getDeviceId(), m_settings.m_centerFrequency);
    int len = attenuations[0];
    ui->gainLNA->clear();
    for (int i = 1; i <= len; i++)
    {
        if (attenuations[i] == 0)
            ui->gainLNA->addItem("0");
        else
            ui->gainLNA->addItem(QString("-%1").arg(attenuations[i]));

        // Find closest match
        if ((attenuations[i] == -currentValue) || (!found && (attenuations[i] > -currentValue)))
        {
            ui->gainLNA->setCurrentIndex(i - 1);
            found = true;
        }
    }
}

void SDRPlayV3Gui::sendSettings()
{
    if (!m_updateTimer.isActive()) {
        m_updateTimer.start(100);
    }
}

void SDRPlayV3Gui::updateHardware()
{
    if (m_doApplySettings)
    {
        qDebug() << "SDRPlayV3Gui::updateHardware";
        SDRPlayV3Input::MsgConfigureSDRPlayV3* message = SDRPlayV3Input::MsgConfigureSDRPlayV3::create(m_settings, m_settingsKeys, m_forceSettings);
        m_sdrPlayV3Input->getInputMessageQueue()->push(message);
        m_forceSettings = false;
        m_settingsKeys.clear();
        m_updateTimer.stop();
    }
}

void SDRPlayV3Gui::updateStatus()
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

void SDRPlayV3Gui::on_centerFrequency_changed(quint64 value)
{
    m_settings.m_centerFrequency = value * 1000;
    updateLNAValues();
    m_settingsKeys.append("centerFrequency");
    sendSettings();
}

void SDRPlayV3Gui::on_ppm_valueChanged(int value)
{
    m_settings.m_LOppmTenths = value;
    ui->ppmText->setText(QString("%1").arg(QString::number(m_settings.m_LOppmTenths/10.0, 'f', 1)));
    m_settingsKeys.append("LOppmTenths");
    sendSettings();
}

void SDRPlayV3Gui::on_tuner_currentIndexChanged(int index)
{
    m_settings.m_tuner = index;
    m_settingsKeys.append("tuner");

    if (m_sdrPlayV3Input->getDeviceId() == SDRPLAY_RSPduo_ID)
    {
        ui->antenna->clear();
        ui->antenna->addItem("50Ohm");
        if (m_settings.m_tuner == 0)
            ui->antenna->addItem("Hi-Z");
        ui->amNotch->setVisible(index == 0);
        ui->biasTee->setVisible(index == 1);
    }

    sendSettings();
}

void SDRPlayV3Gui::on_antenna_currentIndexChanged(int index)
{
    m_settings.m_antenna = index;
    m_settingsKeys.append("antenna");
    sendSettings();
}

void SDRPlayV3Gui::on_dcOffset_toggled(bool checked)
{
    m_settings.m_dcBlock = checked;
    m_settingsKeys.append("dcBlock");
    sendSettings();
}

void SDRPlayV3Gui::on_iqImbalance_toggled(bool checked)
{
    m_settings.m_iqCorrection = checked;
    m_settingsKeys.append("iqCorrection");
    sendSettings();
}

void SDRPlayV3Gui::on_extRef_toggled(bool checked)
{
    m_settings.m_extRef = checked;
    m_settingsKeys.append("extRef");
    sendSettings();
}

void SDRPlayV3Gui::on_biasTee_toggled(bool checked)
{
    m_settings.m_biasTee = checked;
    m_settingsKeys.append("biasTee");
    sendSettings();
}

void SDRPlayV3Gui::on_amNotch_toggled(bool checked)
{
    m_settings.m_amNotch = checked;
    m_settingsKeys.append("amNotch");
    sendSettings();
}

void SDRPlayV3Gui::on_fmNotch_toggled(bool checked)
{
    m_settings.m_fmNotch = checked;
    m_settingsKeys.append("fmNotch");
    sendSettings();
}

void SDRPlayV3Gui::on_dabNotch_toggled(bool checked)
{
    m_settings.m_dabNotch = checked;
    m_settingsKeys.append("dabNotch");
    sendSettings();
}

void SDRPlayV3Gui::on_bandwidth_currentIndexChanged(int index)
{
    m_settings.m_bandwidthIndex = index;
    m_settingsKeys.append("bandwidthIndex");
    sendSettings();
}

void SDRPlayV3Gui::on_samplerate_changed(quint64 value)
{
    m_settings.m_devSampleRate = (uint32_t) value;
    m_settingsKeys.append("devSampleRate");
    sendSettings();
}

void SDRPlayV3Gui::on_ifFrequency_currentIndexChanged(int index)
{
    m_settings.m_ifFrequencyIndex = index;
    m_settingsKeys.append("ifFrequencyIndex");
    sendSettings();
}

void SDRPlayV3Gui::on_decim_currentIndexChanged(int index)
{
    m_settings.m_log2Decim = index;
    m_settingsKeys.append("log2Decim");
    sendSettings();
}

void SDRPlayV3Gui::on_fcPos_currentIndexChanged(int index)
{
    m_settings.m_fcPos = (SDRPlayV3Settings::fcPos_t) index;
    m_settingsKeys.append("fcPos");
    sendSettings();
}

void SDRPlayV3Gui::on_gainLNA_currentIndexChanged(int index)
{
    m_settings.m_lnaIndex = index;
    m_settingsKeys.append("lnaIndex");
    sendSettings();
}

void SDRPlayV3Gui::on_gainIFAGC_toggled(bool checked)
{
    m_settings.m_ifAGC = checked;
    m_settingsKeys.append("ifAGC");
    ui->gainIF->setEnabled(!checked);
    sendSettings();
}

void SDRPlayV3Gui::on_gainIF_valueChanged(int value)
{
    m_settings.m_ifGain = value;
    m_settingsKeys.append("ifGain");

    QString gainText = QStringLiteral("%1").arg(value, 2, 10, QLatin1Char('0'));
    ui->gainIFText->setText(gainText);

    sendSettings();
}

void SDRPlayV3Gui::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        SDRPlayV3Input::MsgStartStop *message = SDRPlayV3Input::MsgStartStop::create(checked);
        m_sdrPlayV3Input->getInputMessageQueue()->push(message);
    }
}

void SDRPlayV3Gui::on_transverter_clicked()
{
    m_settings.m_transverterMode = ui->transverter->getDeltaFrequencyAcive();
    m_settings.m_transverterDeltaFrequency = ui->transverter->getDeltaFrequency();
    m_settings.m_iqOrder = ui->transverter->getIQOrder();
    qDebug("SDRPlayV3Gui::on_transverter_clicked: %lld Hz %s", m_settings.m_transverterDeltaFrequency, m_settings.m_transverterMode ? "on" : "off");
    updateFrequencyLimits();
    m_settings.m_centerFrequency = ui->centerFrequency->getValueNew()*1000;
    m_settingsKeys.append("transverterMode");
    m_settingsKeys.append("transverterDeltaFrequency");
    m_settingsKeys.append("iqOrder");
    m_settingsKeys.append("centerFrequency");
    sendSettings();
}

void SDRPlayV3Gui::openDeviceSettingsDialog(const QPoint& p)
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
        m_settingsKeys.append("useReverseAPI");
        m_settingsKeys.append("reverseAPIAddress");
        m_settingsKeys.append("reverseAPIPort");
        m_settingsKeys.append("reverseAPIDeviceIndex");

        sendSettings();
    }

    resetContextMenuType();
}

void SDRPlayV3Gui::makeUIConnections()
{
    QObject::connect(ui->centerFrequency, &ValueDial::changed, this, &SDRPlayV3Gui::on_centerFrequency_changed);
    QObject::connect(ui->ppm, &QSlider::valueChanged, this, &SDRPlayV3Gui::on_ppm_valueChanged);
    QObject::connect(ui->tuner, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SDRPlayV3Gui::on_tuner_currentIndexChanged);
    QObject::connect(ui->antenna, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SDRPlayV3Gui::on_antenna_currentIndexChanged);
    QObject::connect(ui->dcOffset, &ButtonSwitch::toggled, this, &SDRPlayV3Gui::on_dcOffset_toggled);
    QObject::connect(ui->iqImbalance, &ButtonSwitch::toggled, this, &SDRPlayV3Gui::on_iqImbalance_toggled);
    QObject::connect(ui->extRef, &ButtonSwitch::toggled, this, &SDRPlayV3Gui::on_extRef_toggled);
    QObject::connect(ui->biasTee, &ButtonSwitch::toggled, this, &SDRPlayV3Gui::on_biasTee_toggled);
    QObject::connect(ui->amNotch, &ButtonSwitch::toggled, this, &SDRPlayV3Gui::on_amNotch_toggled);
    QObject::connect(ui->fmNotch, &ButtonSwitch::toggled, this, &SDRPlayV3Gui::on_fmNotch_toggled);
    QObject::connect(ui->dabNotch, &ButtonSwitch::toggled, this, &SDRPlayV3Gui::on_dabNotch_toggled);
    QObject::connect(ui->bandwidth, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SDRPlayV3Gui::on_bandwidth_currentIndexChanged);
    QObject::connect(ui->samplerate, &ValueDial::changed, this, &SDRPlayV3Gui::on_samplerate_changed);
    QObject::connect(ui->ifFrequency, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SDRPlayV3Gui::on_ifFrequency_currentIndexChanged);
    QObject::connect(ui->decim, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SDRPlayV3Gui::on_decim_currentIndexChanged);
    QObject::connect(ui->fcPos, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SDRPlayV3Gui::on_fcPos_currentIndexChanged);
    QObject::connect(ui->gainLNA, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SDRPlayV3Gui::on_gainLNA_currentIndexChanged);
    QObject::connect(ui->gainIFAGC, &ButtonSwitch::toggled, this, &SDRPlayV3Gui::on_gainIFAGC_toggled);
    QObject::connect(ui->gainIF, &QDial::valueChanged, this, &SDRPlayV3Gui::on_gainIF_valueChanged);
    QObject::connect(ui->startStop, &ButtonSwitch::toggled, this, &SDRPlayV3Gui::on_startStop_toggled);
    QObject::connect(ui->transverter, &TransverterButton::clicked, this, &SDRPlayV3Gui::on_transverter_clicked);
}
