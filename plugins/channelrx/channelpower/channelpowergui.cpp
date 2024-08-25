///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2023-2024 Jon Beniston, M7RCE                                   //
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

#include "device/deviceuiset.h"
#include "device/deviceapi.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "ui_channelpowergui.h"
#include "plugin/pluginapi.h"
#include "util/db.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "dsp/dspengine.h"
#include "maincore.h"

#include "channelpower.h"
#include "channelpowergui.h"

ChannelPowerGUI* ChannelPowerGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
    ChannelPowerGUI* gui = new ChannelPowerGUI(pluginAPI, deviceUISet, rxChannel);
    return gui;
}

void ChannelPowerGUI::destroy()
{
    delete this;
}

void ChannelPowerGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applyAllSettings();
}

QByteArray ChannelPowerGUI::serialize() const
{
    return m_settings.serialize();
}

bool ChannelPowerGUI::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        displaySettings();
        applyAllSettings();
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

bool ChannelPowerGUI::handleMessage(const Message& message)
{
    if (ChannelPower::MsgConfigureChannelPower::match(message))
    {
        qDebug("ChannelPowerGUI::handleMessage: ChannelPower::MsgConfigureChannelPower");
        const ChannelPower::MsgConfigureChannelPower& cfg = (ChannelPower::MsgConfigureChannelPower&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (DSPSignalNotification::match(message))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) message;
        m_deviceCenterFrequency = notif.getCenterFrequency();
        m_basebandSampleRate = notif.getSampleRate();
        calcOffset();
        ui->rfBW->setValueRange(floor(log10(m_basebandSampleRate))+1, 0, m_basebandSampleRate);
        updateAbsoluteCenterFrequency();
        return true;
    }

    return false;
}

void ChannelPowerGUI::handleInputMessages()
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

void ChannelPowerGUI::channelMarkerChangedByCursor()
{
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    m_settings.m_frequency = m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset;

    qint64 value = 0;

    if (m_settings.m_frequencyMode == ChannelPowerSettings::Offset) {
        value = m_settings.m_inputFrequencyOffset;
    } else if (m_settings.m_frequencyMode == ChannelPowerSettings::Absolute) {
        value = m_settings.m_frequency;
    }

    ui->deltaFrequency->blockSignals(true);
    ui->deltaFrequency->setValue(value);
    ui->deltaFrequency->blockSignals(false);

    updateAbsoluteCenterFrequency();
    applySettings({"frequency", "inputFrequencyOffset"});
}

void ChannelPowerGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void ChannelPowerGUI::on_deltaFrequency_changed(qint64 value)
{
    qint64 offset = 0;

    if (m_settings.m_frequencyMode == ChannelPowerSettings::Offset)
    {
        offset = value;
        m_settings.m_frequency = m_deviceCenterFrequency + offset;
    }
    else if (m_settings.m_frequencyMode == ChannelPowerSettings::Absolute)
    {
        m_settings.m_frequency = value;
        offset = m_settings.m_frequency - m_deviceCenterFrequency;
    }

    m_channelMarker.setCenterFrequency(offset);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings({"frequency", "inputFrequencyOffset"});
}

void ChannelPowerGUI::on_rfBW_changed(qint64 value)
{
    m_channelMarker.setBandwidth(value);
    m_settings.m_rfBandwidth = value;
    applySetting("rfBandwidth");
}

void ChannelPowerGUI::on_pulseTH_valueChanged(int value)
{
    m_settings.m_pulseThreshold = (float)value;
    ui->pulseTHText->setText(QString::number(value));
    applySetting("pulseThreshold");
}

const QStringList ChannelPowerGUI::m_averagePeriodTexts = {
    "10us", "100us", "1ms", "10ms", "100ms", "1s", "10s"
};

void ChannelPowerGUI::on_averagePeriod_valueChanged(int value)
{
    m_settings.m_averagePeriodUS = (int)std::pow(10.0f, (float)value);
    ui->averagePeriodText->setText(m_averagePeriodTexts[value-1]);
    applySetting("averagePeriodUS");
}

void ChannelPowerGUI::on_clearChannelPower_clicked()
{
    m_channelPower->resetMagLevels();
}

void ChannelPowerGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySetting("rollupState");
}

void ChannelPowerGUI::onMenuDialogCalled(const QPoint &p)
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

        if (m_deviceUISet->m_deviceMIMOEngine)
        {
            dialog.setNumberOfStreams(m_channelPower->getNumberOfDeviceStreams());
            dialog.setStreamIndex(m_settings.m_streamIndex);
        }

        dialog.move(p);
        new DialogPositioner(&dialog, true);
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

        QStringList settingsKeys({
            "rgbColor",
            "title",
            "useReverseAPI",
            "reverseAPIAddress",
            "reverseAPIPort",
            "reverseAPIDeviceIndex",
            "reverseAPIChannelIndex"
        });

        if (m_deviceUISet->m_deviceMIMOEngine)
        {
            m_settings.m_streamIndex = dialog.getSelectedStreamIndex();
            m_channelMarker.clearStreamIndexes();
            m_channelMarker.addStreamIndex(m_settings.m_streamIndex);
            updateIndexLabel();
        }

        applySettings(settingsKeys);
    }

    resetContextMenuType();
}

ChannelPowerGUI::ChannelPowerGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
    ChannelGUI(parent),
    ui(new Ui::ChannelPowerGUI),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_basebandSampleRate(1),
    m_doApplySettings(true),
    m_tickCount(0)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/channelpower/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_channelPower = reinterpret_cast<ChannelPower*>(rxChannel);
    m_channelPower->setMessageQueueToGUI(getInputMessageQueue());

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick())); // 50 ms

    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);

    ui->rfBW->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->rfBW->setValueRange(7, 0, 9999999);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(Qt::yellow);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle("Channel Power");
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    setTitleColor(m_channelMarker.getColor());
    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setRollupState(&m_rollupState);

    m_deviceUISet->addChannelMarker(&m_channelMarker);

    connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()), this, SLOT(channelMarkerHighlightedByCursor()));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    displaySettings();
    makeUIConnections();
    applyAllSettings();
    DialPopup::addPopupsToChildDials(this);
    m_resizer.enableChildMouseTracking();
}

ChannelPowerGUI::~ChannelPowerGUI()
{
    disconnect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));
    delete ui;
}

void ChannelPowerGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void ChannelPowerGUI::applySetting(const QString& settingsKey)
{
    applySettings({settingsKey});
}

void ChannelPowerGUI::applySettings(const QStringList& settingsKeys, bool force)
{
    m_settingsKeys.append(settingsKeys);
    if (m_doApplySettings)
    {
        ChannelPower::MsgConfigureChannelPower* message = ChannelPower::MsgConfigureChannelPower::create(m_settings, m_settingsKeys, force);
        m_channelPower->getInputMessageQueue()->push(message);
        m_settingsKeys.clear();
    }
}

void ChannelPowerGUI::applyAllSettings()
{
    applySettings(QStringList(), true);
}

void ChannelPowerGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());

    blockApplySettings(true);

    ui->frequencyMode->setCurrentIndex((int) m_settings.m_frequencyMode);
    on_frequencyMode_currentIndexChanged((int) m_settings.m_frequencyMode);

    ui->rfBW->setValue(m_settings.m_rfBandwidth);

    ui->pulseTH->setValue(m_settings.m_pulseThreshold);
    ui->pulseTHText->setText(QString::number((int)m_settings.m_pulseThreshold));

    int value = (int)std::log10(m_settings.m_averagePeriodUS);
    ui->averagePeriod->setValue(value);
    ui->averagePeriodText->setText(m_averagePeriodTexts[value-1]);

    ui->averagePeriod->setMinimum(std::max(1, static_cast<int> (m_averagePeriodTexts.size()) - value));

    updateIndexLabel();
    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void ChannelPowerGUI::leaveEvent(QEvent* event)
{
    m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void ChannelPowerGUI::enterEvent(EnterEventType* event)
{
    m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void ChannelPowerGUI::tick()
{
    // Get power measurements
    double magAvg, magPulseAvg, magMaxPeak, magMinPeak;
    m_channelPower->getMagLevels(magAvg, magPulseAvg, magMaxPeak, magMinPeak);

    double powDbAvg, powDbPulseAvg, powDbMaxPeak, powDbMinPeak;
    powDbAvg = std::numeric_limits<double>::quiet_NaN();
    powDbPulseAvg = std::numeric_limits<double>::quiet_NaN();
    powDbMaxPeak = std::numeric_limits<double>::quiet_NaN();
    powDbMinPeak = std::numeric_limits<double>::quiet_NaN();

    const int precision = 2;

    if (!std::isnan(magAvg))
    {
        powDbAvg = CalcDb::dbPower(magAvg * magAvg);
        if (m_tickCount % 4 == 0) {
            ui->average->setText(QString::number(powDbAvg, 'f', precision));
        }
    }
    else
    {
        ui->average->setText("");
    }
    if (!std::isnan(magPulseAvg))
    {
        powDbPulseAvg = CalcDb::dbPower(magPulseAvg * magPulseAvg);
        if (m_tickCount % 4 == 0) {
            ui->pulseAverage->setText(QString::number(powDbPulseAvg, 'f', precision));
        }
    }
    else
    {
        ui->pulseAverage->setText("");
    }
    if (magMaxPeak != -std::numeric_limits<double>::max())
    {
        powDbMaxPeak = CalcDb::dbPower(magMaxPeak * magMaxPeak);
        if (m_tickCount % 4 == 0) {
            ui->maxPeak->setText(QString::number(powDbMaxPeak, 'f', precision));
        }
    }
    else
    {
        ui->maxPeak->setText("");
    }
    if (magMinPeak != std::numeric_limits<double>::max())
    {
        powDbMinPeak = CalcDb::dbPower(magMinPeak * magMinPeak);
        if (m_tickCount % 4 == 0) {
            ui->minPeak->setText(QString::number(powDbMinPeak, 'f', precision));
        }
    }
    else
    {
        ui->minPeak->setText("");
    }

    m_tickCount++;
}

void ChannelPowerGUI::on_frequencyMode_currentIndexChanged(int index)
{
    m_settings.m_frequencyMode = (ChannelPowerSettings::FrequencyMode) index;
    ui->deltaFrequency->blockSignals(true);

    if (m_settings.m_frequencyMode == ChannelPowerSettings::Offset)
    {
        ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);
        ui->deltaFrequency->setValue(m_settings.m_inputFrequencyOffset);
        ui->deltaUnits->setText("Hz");
    }
    else if (m_settings.m_frequencyMode == ChannelPowerSettings::Absolute)
    {
        ui->deltaFrequency->setValueRange(true, 11, 0, 99999999999, 0);
        ui->deltaFrequency->setValue(m_settings.m_frequency);
        ui->deltaUnits->setText("Hz");
    }

    ui->deltaFrequency->blockSignals(false);

    updateAbsoluteCenterFrequency();
    applySetting("frequencyMode");
}

// Calculate input frequency offset, when device center frequency changes
void ChannelPowerGUI::calcOffset()
{
    if (m_settings.m_frequencyMode == ChannelPowerSettings::Offset)
    {
        ui->deltaFrequency->setValueRange(false, 7, -m_basebandSampleRate/2, m_basebandSampleRate/2);
    }
    else
    {
        qint64 offset = m_settings.m_frequency - m_deviceCenterFrequency;
        m_channelMarker.setCenterFrequency(offset);
        m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
        updateAbsoluteCenterFrequency();
        applySetting("inputFrequencyOffset");
    }
}

void ChannelPowerGUI::on_clearMeasurements_clicked()
{
    m_channelPower->resetMagLevels();
}

void ChannelPowerGUI::makeUIConnections()
{
    QObject::connect(ui->frequencyMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ChannelPowerGUI::on_frequencyMode_currentIndexChanged);
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &ChannelPowerGUI::on_deltaFrequency_changed);
    QObject::connect(ui->rfBW, &ValueDial::changed, this, &ChannelPowerGUI::on_rfBW_changed);
    QObject::connect(ui->pulseTH, QOverload<int>::of(&QDial::valueChanged), this, &ChannelPowerGUI::on_pulseTH_valueChanged);
    QObject::connect(ui->averagePeriod, QOverload<int>::of(&QDial::valueChanged), this, &ChannelPowerGUI::on_averagePeriod_valueChanged);
    QObject::connect(ui->clearChannelPower, &QPushButton::clicked, this, &ChannelPowerGUI::on_clearMeasurements_clicked);
}

void ChannelPowerGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_settings.m_frequency);
    if (   (m_basebandSampleRate > 1)
        && (   (m_settings.m_inputFrequencyOffset >= m_basebandSampleRate / 2)
            || (m_settings.m_inputFrequencyOffset < -m_basebandSampleRate / 2))) {
        setStatusText("Frequency out of band");
    } else {
        setStatusText("");
    }
}
