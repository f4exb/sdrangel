///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018-2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2021-2023 Jon Beniston, M7RCE <jon@beniston.com>                //
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

#include <QLocale>

#include "device/deviceuiset.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/dialogpositioner.h"
#include "dsp/hbfilterchainconverter.h"
#include "dsp/scopevis.h"
#include "dsp/spectrumvis.h"
#include "maincore.h"

#include "interferometergui.h"
#include "interferometer.h"
#include "ui_interferometergui.h"

InterferometerGUI* InterferometerGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, MIMOChannel *channelMIMO)
{
    auto* gui = new InterferometerGUI(pluginAPI, deviceUISet, channelMIMO);
    return gui;
}

void InterferometerGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray InterferometerGUI::serialize() const
{
    return m_settings.serialize();
}

bool InterferometerGUI::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        displaySettings();
        applySettings(true);
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

MessageQueue* InterferometerGUI::getInputMessageQueue()
{
    return &m_inputMessageQueue;
}

bool InterferometerGUI::handleMessage(const Message& message)
{
    if (Interferometer::MsgBasebandNotification::match(message))
    {
        auto& notif = (const Interferometer::MsgBasebandNotification&) message;
        m_sampleRate = notif.getSampleRate();
        m_centerFrequency = notif.getCenterFrequency();
        displayRateAndShift();
        updateAbsoluteCenterFrequency();
        return true;
    }
    else if (Interferometer::MsgConfigureInterferometer::match(message))
    {
        auto& cfg = (const Interferometer::MsgConfigureInterferometer&) message;

        if (cfg.getForce()) {
            m_settings = cfg.getSettings();
        } else {
            m_settings.applySettings(cfg.getSettingsKeys(), cfg.getSettings());
        }

        ui->scopeGUI->updateSettings();
        ui->spectrumGUI->updateSettings();
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        return true;
    }
    else if (Interferometer::MsgReportDevices::match(message))
    {
        auto& msg = const_cast<Message&>(message);
        auto& report = (Interferometer::MsgReportDevices&) msg;
        updateDeviceSetList(report.getDeviceSetIndexes());
        return true;
    }
    else
    {
        return false;
    }
}

InterferometerGUI::InterferometerGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, MIMOChannel *channelMIMO, QWidget* parent) :
        ChannelGUI(parent),
        ui(new Ui::InterferometerGUI),
        m_pluginAPI(pluginAPI),
        m_deviceUISet(deviceUISet),
        m_sampleRate(48000),
        m_centerFrequency(435000000),
        m_tickCount(0)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelmimo/interferometer/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_interferometer = (Interferometer*) channelMIMO;
    m_spectrumVis = m_interferometer->getSpectrumVis();
    m_spectrumVis->setGLSpectrum(ui->glSpectrum);
    m_scopeVis = m_interferometer->getScopeVis();
    m_scopeVis->setGLScope(ui->glScope);
    m_interferometer->setMessageQueueToGUI(InterferometerGUI::getInputMessageQueue());
    m_sampleRate = m_interferometer->getDeviceSampleRate();

	ui->spectrumGUI->setBuddies(m_spectrumVis, ui->glSpectrum);
	ui->scopeGUI->setBuddies(m_scopeVis->getInputMessageQueue(), m_scopeVis, ui->glScope);

    ui->glSpectrum->setCenterFrequency(0);
    ui->glSpectrum->setSampleRate(m_sampleRate);
    ui->glSpectrum->setLsbDisplay(false);

    SpectrumSettings spectrumSettings = m_spectrumVis->getSettings();
    spectrumSettings.m_displayWaterfall = true;
    spectrumSettings.m_displayMaxHold = true;
    spectrumSettings.m_ssb = false;
    SpectrumVis::MsgConfigureSpectrumVis *msg = SpectrumVis::MsgConfigureSpectrumVis::create(spectrumSettings, false);
    m_spectrumVis->getInputMessageQueue()->push(msg);

    ui->glScope->setTraceModulo(Interferometer::m_fftSize);

	ui->glScope->connectTimer(MainCore::instance()->getMasterTimer());
	connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    m_channelMarker.blockSignals(true);
    m_channelMarker.addStreamIndex(1);
    m_channelMarker.setColor(m_settings.m_rgbColor);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("Interferometer");
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setRollupState(&m_rollupState);
    m_settings.setScopeGUI(ui->scopeGUI);
    m_settings.setSpectrumGUI(ui->spectrumGUI);

    m_deviceUISet->addChannelMarker(&m_channelMarker);

    m_scopeVis->setTraceChunkSize(Interferometer::m_fftSize); // Set scope trace length unit to FFT size
    ui->scopeGUI->traceLengthChange();

    connect(InterferometerGUI::getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));

    updateDeviceSetList(m_interferometer->getDeviceSetList());
    displaySettings();
    makeUIConnections();
    displayRateAndShift();
    applySettings(true);
    m_resizer.enableChildMouseTracking();
}

InterferometerGUI::~InterferometerGUI()
{
    delete ui;
}

void InterferometerGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void InterferometerGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        setTitleColor(m_channelMarker.getColor());

        Interferometer::MsgConfigureInterferometer* message = Interferometer::MsgConfigureInterferometer::create(m_settings, m_settingsKeys, force);
        m_interferometer->getInputMessageQueue()->push(message);
    }

    m_settingsKeys.clear();
}

void InterferometerGUI::displaySettings()
{
    ui->correlationType->setCurrentIndex((int) m_settings.m_correlationType);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.setBandwidth(m_sampleRate);
    m_channelMarker.setMovable(false); // do not let user move the center arbitrarily
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());

    blockApplySettings(true);
    int index = getLocalDeviceIndexInCombo(m_settings.m_localDeviceIndex);
    if (index >= 0) {
        ui->localDevice->setCurrentIndex(index);
    }
    ui->localDevicePlay->setChecked(m_settings.m_play);
    ui->decimationFactor->setCurrentIndex(m_settings.m_log2Decim);
    applyDecimation();
    ui->phaseCorrection->setValue(m_settings.m_phase);
    ui->phaseCorrectionText->setText(tr("%1").arg(m_settings.m_phase));
    ui->gain->setValue(m_settings.m_gain);
    ui->gainText->setText(QString("%1").arg(m_settings.m_gain / 10.0, 0, 'f', 1));
    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void InterferometerGUI::displayRateAndShift()
{
    auto shift = (int) (m_shiftFrequencyFactor * m_sampleRate);
    double channelSampleRate = ((double) m_sampleRate) / (1<<m_settings.m_log2Decim);
    QLocale loc;
    ui->offsetFrequencyText->setText(tr("%1 Hz").arg(loc.toString(shift)));
    ui->channelRateText->setText(tr("%1k").arg(QString::number(channelSampleRate / 1000.0, 'g', 5)));
    m_channelMarker.setCenterFrequency(shift);
    m_channelMarker.setBandwidth((int) channelSampleRate);
    ui->glSpectrum->setSampleRate((int) channelSampleRate);
    m_scopeVis->setLiveRate((int) channelSampleRate);
}

void InterferometerGUI::leaveEvent(QEvent*)
{
    m_channelMarker.setHighlighted(false);
}

void InterferometerGUI::enterEvent(EnterEventType*)
{
    m_channelMarker.setHighlighted(true);
}

void InterferometerGUI::handleSourceMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()) != nullptr)
    {
        if (handleMessage(*message))
        {
            delete message;
        }
    }
}

void InterferometerGUI::onWidgetRolled(const QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

void InterferometerGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuType::ContextMenuChannelSettings)
    {
        BasicChannelSettingsDialog dialog(&m_channelMarker, this);
        dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
        dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
        dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
        dialog.setReverseAPIDeviceIndex(m_settings.m_reverseAPIDeviceIndex);
        dialog.setReverseAPIChannelIndex(m_settings.m_reverseAPIChannelIndex);
        dialog.setDefaultTitle(m_displayedName);

        dialog.move(p);
        new DialogPositioner(&dialog, false);
        dialog.exec();

        m_settings.m_rgbColor = m_channelMarker.getColor().rgb();
        m_settings.m_title = m_channelMarker.getTitle();
        m_settings.m_useReverseAPI = dialog.useReverseAPI();
        m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
        m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
        m_settings.m_reverseAPIDeviceIndex = dialog.getReverseAPIDeviceIndex();
        m_settings.m_reverseAPIChannelIndex = dialog.getReverseAPIChannelIndex();

        setWindowTitle(m_settings.m_title);
        setTitle(m_channelMarker.getTitle());
        setTitleColor(m_settings.m_rgbColor);

        m_settingsKeys.append("title");
        m_settingsKeys.append("rgbColor");
        m_settingsKeys.append("useReverseAPI");
        m_settingsKeys.append("reverseAPIAddress");
        m_settingsKeys.append("reverseAPIPort");
        m_settingsKeys.append("reverseAPIFeatureSetIndex");
        m_settingsKeys.append("reverseAPIFeatureIndex");

        applySettings();
    }

    resetContextMenuType();
}

void InterferometerGUI::updateDeviceSetList(const QList<int>& deviceSetIndexes)
{
    auto it = deviceSetIndexes.begin();

    ui->localDevice->blockSignals(true);

    ui->localDevice->clear();

    for (; it != deviceSetIndexes.end(); ++it) {
        ui->localDevice->addItem(QString("R%1").arg(*it), *it);
    }

    ui->localDevice->blockSignals(false);
}

int InterferometerGUI::getLocalDeviceIndexInCombo(int localDeviceIndex) const
{
    int index = 0;

    for (; index < ui->localDevice->count(); index++)
    {
        if (localDeviceIndex == ui->localDevice->itemData(index).toInt()) {
            return index;
        }
    }

    return -1;
}

void InterferometerGUI::on_decimationFactor_currentIndexChanged(int index)
{
    m_settings.m_log2Decim = index;
    m_settingsKeys.append("log2Decim");
    applyDecimation();
}

void InterferometerGUI::on_position_valueChanged(int value)
{
    m_settings.m_filterChainHash = value;
    m_settingsKeys.append("filterChainHash");
    applyPosition();
}

void InterferometerGUI::on_phaseCorrection_valueChanged(int value)
{
    m_settings.m_phase = value;
    m_settingsKeys.append("phase");
    ui->phaseCorrectionText->setText(tr("%1").arg(value));
    applySettings();
}

void InterferometerGUI::on_gain_valueChanged(int value)
{
    m_settings.m_gain = value;
    m_settingsKeys.append("gain");
    ui->gainText->setText(QString("%1").arg(m_settings.m_gain / 10.0, 0, 'f', 1));
    applySettings();
}

void InterferometerGUI::on_phaseCorrectionLabel_clicked()
{
    m_settings.m_phase = 0;
    ui->phaseCorrection->setValue(0);
}

void InterferometerGUI::on_gainLabel_clicked()
{
    m_settings.m_gain = 0;
    ui->gain->setValue(0);
}

void InterferometerGUI::on_correlationType_currentIndexChanged(int index)
{
    m_settings.m_correlationType = (InterferometerSettings::CorrelationType) index;
    m_settingsKeys.append("correlationType");
    applySettings();
}

void InterferometerGUI::on_localDevice_currentIndexChanged(int index)
{
    if (index >= 0)
    {
        m_settings.m_localDeviceIndex = ui->localDevice->currentData().toInt();
        m_settingsKeys.append("localDeviceIndex");
        applySettings();
    }
}

void InterferometerGUI::on_localDevicePlay_toggled(bool checked)
{
    m_settings.m_play = checked;
    m_settingsKeys.append("play");
    applySettings();
}

void InterferometerGUI::applyDecimation()
{
    uint32_t maxHash = 1;

    for (uint32_t i = 0; i < m_settings.m_log2Decim; i++) {
        maxHash *= 3;
    }

    ui->position->setMaximum(maxHash-1);
    ui->position->setValue(m_settings.m_filterChainHash);
    m_settings.m_filterChainHash = ui->position->value();
    m_settingsKeys.append("filterChainHash");
    applyPosition();
}

void InterferometerGUI::applyPosition()
{
    ui->filterChainIndex->setText(tr("%1").arg(m_settings.m_filterChainHash));
    QString s;
    m_shiftFrequencyFactor = HBFilterChainConverter::convertToString(m_settings.m_log2Decim, m_settings.m_filterChainHash, s);
    ui->filterChainText->setText(s);

    displayRateAndShift();
    updateAbsoluteCenterFrequency();
    applySettings();
}

void InterferometerGUI::tick()
{
    if (++m_tickCount == 20) { // once per second
        m_tickCount = 0;
    }
}

void InterferometerGUI::makeUIConnections() const
{
    QObject::connect(ui->decimationFactor, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &InterferometerGUI::on_decimationFactor_currentIndexChanged);
    QObject::connect(ui->position, &QSlider::valueChanged, this, &InterferometerGUI::on_position_valueChanged);
    QObject::connect(ui->phaseCorrection, &QSlider::valueChanged, this, &InterferometerGUI::on_phaseCorrection_valueChanged);
    QObject::connect(ui->gain, &QDial::valueChanged, this, &InterferometerGUI::on_gain_valueChanged);
    QObject::connect(ui->phaseCorrectionLabel, &ClickableLabel::clicked, this, &InterferometerGUI::on_phaseCorrectionLabel_clicked);
    QObject::connect(ui->gainLabel, &ClickableLabel::clicked, this, &InterferometerGUI::on_gainLabel_clicked);
    QObject::connect(ui->correlationType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &InterferometerGUI::on_correlationType_currentIndexChanged);
    QObject::connect(ui->localDevice, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &InterferometerGUI::on_localDevice_currentIndexChanged);
    QObject::connect(ui->localDevicePlay, &ButtonSwitch::toggled, this, &InterferometerGUI::on_localDevicePlay_toggled);
}

void InterferometerGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency((qint64) ((double) m_centerFrequency + m_shiftFrequencyFactor * m_sampleRate));
}
