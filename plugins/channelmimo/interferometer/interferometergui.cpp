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

#include <QLocale>

#include "device/deviceuiset.h"
#include "gui/basicchannelsettingsdialog.h"
#include "dsp/hbfilterchainconverter.h"
#include "dsp/scopevis.h"
#include "dsp/spectrumvis.h"
#include "maincore.h"

#include "interferometergui.h"
#include "interferometer.h"
#include "ui_interferometergui.h"

InterferometerGUI* InterferometerGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, MIMOChannel *channelMIMO)
{
    InterferometerGUI* gui = new InterferometerGUI(pluginAPI, deviceUISet, channelMIMO);
    return gui;
}

void InterferometerGUI::destroy()
{
    delete this;
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
        Interferometer::MsgBasebandNotification& notif = (Interferometer::MsgBasebandNotification&) message;
        m_sampleRate = notif.getSampleRate();
        m_centerFrequency = notif.getCenterFrequency();
        displayRateAndShift();
        updateAbsoluteCenterFrequency();
        return true;
    }
    else if (Interferometer::MsgConfigureInterferometer::match(message))
    {
        const Interferometer::MsgConfigureInterferometer& notif = (const Interferometer::MsgConfigureInterferometer&) message;
        m_settings = notif.getSettings();
        ui->scopeGUI->updateSettings();
        ui->spectrumGUI->updateSettings();
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
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
    m_interferometer->setMessageQueueToGUI(getInputMessageQueue());
    m_sampleRate = m_interferometer->getDeviceSampleRate();

	ui->glSpectrum->setDisplayWaterfall(true);
	ui->glSpectrum->setDisplayMaxHold(true);
    ui->glSpectrum->setCenterFrequency(0);
    ui->glSpectrum->setSampleRate(m_sampleRate);
	ui->glSpectrum->setSsbSpectrum(false);
    ui->glSpectrum->setLsbDisplay(false);
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

	ui->spectrumGUI->setBuddies(m_spectrumVis, ui->glSpectrum);
	ui->scopeGUI->setBuddies(m_scopeVis->getInputMessageQueue(), m_scopeVis, ui->glScope);

    m_scopeVis->setTraceChunkSize(Interferometer::m_fftSize); // Set scope trace length unit to FFT size
    ui->scopeGUI->traceLengthChange();

    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));

    displaySettings();
    makeUIConnections();
    displayRateAndShift();
    applySettings(true);
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

        Interferometer::MsgConfigureInterferometer* message = Interferometer::MsgConfigureInterferometer::create(m_settings, force);
        m_interferometer->getInputMessageQueue()->push(message);
    }
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
    ui->decimationFactor->setCurrentIndex(m_settings.m_log2Decim);
    applyDecimation();
    ui->phaseCorrection->setValue(m_settings.m_phase);
    ui->phaseCorrectionText->setText(tr("%1").arg(m_settings.m_phase));
    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void InterferometerGUI::displayRateAndShift()
{
    int shift = m_shiftFrequencyFactor * m_sampleRate;
    double channelSampleRate = ((double) m_sampleRate) / (1<<m_settings.m_log2Decim);
    QLocale loc;
    ui->offsetFrequencyText->setText(tr("%1 Hz").arg(loc.toString(shift)));
    ui->channelRateText->setText(tr("%1k").arg(QString::number(channelSampleRate / 1000.0, 'g', 5)));
    m_channelMarker.setCenterFrequency(shift);
    m_channelMarker.setBandwidth(channelSampleRate);
    ui->glSpectrum->setSampleRate(channelSampleRate);
    m_scopeVis->setLiveRate(channelSampleRate);
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

    while ((message = getInputMessageQueue()->pop()) != 0)
    {
        if (handleMessage(*message))
        {
            delete message;
        }
    }
}

void InterferometerGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

void InterferometerGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuChannelSettings)
    {
        BasicChannelSettingsDialog dialog(&m_channelMarker, this);
        dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
        dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
        dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
        dialog.setReverseAPIDeviceIndex(m_settings.m_reverseAPIDeviceIndex);
        dialog.setReverseAPIChannelIndex(m_settings.m_reverseAPIChannelIndex);
        dialog.setDefaultTitle(m_displayedName);

        dialog.move(p);
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

        applySettings();
    }

    resetContextMenuType();
}

void InterferometerGUI::on_decimationFactor_currentIndexChanged(int index)
{
    m_settings.m_log2Decim = index;
    applyDecimation();
}

void InterferometerGUI::on_position_valueChanged(int value)
{
    m_settings.m_filterChainHash = value;
    applyPosition();
}

void InterferometerGUI::on_phaseCorrection_valueChanged(int value)
{
    m_settings.m_phase = value;
    ui->phaseCorrectionText->setText(tr("%1").arg(value));
    applySettings();
}

void InterferometerGUI::on_correlationType_currentIndexChanged(int index)
{
    m_settings.m_correlationType = (InterferometerSettings::CorrelationType) index;
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

void InterferometerGUI::makeUIConnections()
{
    QObject::connect(ui->decimationFactor, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &InterferometerGUI::on_decimationFactor_currentIndexChanged);
    QObject::connect(ui->position, &QSlider::valueChanged, this, &InterferometerGUI::on_position_valueChanged);
    QObject::connect(ui->phaseCorrection, &QSlider::valueChanged, this, &InterferometerGUI::on_phaseCorrection_valueChanged);
    QObject::connect(ui->correlationType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &InterferometerGUI::on_correlationType_currentIndexChanged);
}

void InterferometerGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_centerFrequency + m_shiftFrequencyFactor * m_sampleRate);
}
