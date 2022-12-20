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
#include "gui/dialogpositioner.h"
#include "dsp/hbfilterchainconverter.h"
#include "maincore.h"

#include "beamsteeringcwmodgui.h"
#include "beamsteeringcwmod.h"
#include "ui_beamsteeringcwmodgui.h"

BeamSteeringCWModGUI* BeamSteeringCWModGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, MIMOChannel *mimoChannel)
{
    BeamSteeringCWModGUI* gui = new BeamSteeringCWModGUI(pluginAPI, deviceUISet, mimoChannel);
    return gui;
}

void BeamSteeringCWModGUI::destroy()
{
    delete this;
}

void BeamSteeringCWModGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray BeamSteeringCWModGUI::serialize() const
{
    return m_settings.serialize();
}

bool BeamSteeringCWModGUI::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data)) {
        displaySettings();
        applySettings(true);
        return true;
    } else {
        resetToDefaults();
        return false;
    }
}

bool BeamSteeringCWModGUI::handleMessage(const Message& message)
{
    if (BeamSteeringCWMod::MsgBasebandNotification::match(message))
    {
        BeamSteeringCWMod::MsgBasebandNotification& notif = (BeamSteeringCWMod::MsgBasebandNotification&) message;
        m_basebandSampleRate = notif.getSampleRate();
        m_centerFrequency = notif.getCenterFrequency();
        displayRateAndShift();
        updateAbsoluteCenterFrequency();
        return true;
    }
    else if (BeamSteeringCWMod::MsgConfigureBeamSteeringCWMod::match(message))
    {
        const BeamSteeringCWMod::MsgConfigureBeamSteeringCWMod& cfg = (BeamSteeringCWMod::MsgConfigureBeamSteeringCWMod&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else
    {
        return false;
    }
}

BeamSteeringCWModGUI::BeamSteeringCWModGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, MIMOChannel *mimoChannel, QWidget* parent) :
        ChannelGUI(parent),
        ui(new Ui::BeamSteeringCWModGUI),
        m_pluginAPI(pluginAPI),
        m_deviceUISet(deviceUISet),
        m_basebandSampleRate(48000),
        m_centerFrequency(435000000),
        m_tickCount(0)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelmimo/beamSteeringcwmod/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_bsCWSource = (BeamSteeringCWMod*) mimoChannel;
    m_bsCWSource->setMessageQueueToGUI(getInputMessageQueue());
    m_basebandSampleRate = m_bsCWSource->getBasebandSampleRate();

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    m_channelMarker.blockSignals(true);
    m_channelMarker.addStreamIndex(1);
    m_channelMarker.setColor(m_settings.m_rgbColor);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("BeamSteeringCWMod");
    m_channelMarker.setSourceOrSinkStream(false);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setRollupState(&m_rollupState);

    m_deviceUISet->addChannelMarker(&m_channelMarker);

    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));

    displaySettings();
    makeUIConnections();
    displayRateAndShift();
    applySettings(true);
}

BeamSteeringCWModGUI::~BeamSteeringCWModGUI()
{
    delete ui;
}

void BeamSteeringCWModGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void BeamSteeringCWModGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        setTitleColor(m_channelMarker.getColor());

        BeamSteeringCWMod::MsgConfigureBeamSteeringCWMod* message = BeamSteeringCWMod::MsgConfigureBeamSteeringCWMod::create(m_settings, force);
        m_bsCWSource->getInputMessageQueue()->push(message);
    }
}

void BeamSteeringCWModGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.setBandwidth(m_basebandSampleRate);
    m_channelMarker.setMovable(false); // do not let user move the center arbitrarily
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());

    blockApplySettings(true);
    ui->interpolationFactor->setCurrentIndex(m_settings.m_log2Interp);
    applyInterpolation();
    ui->steeringDegreesText->setText(tr("%1").arg(m_settings.m_steerDegrees));
    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void BeamSteeringCWModGUI::displayRateAndShift()
{
    int shift = m_shiftFrequencyFactor * m_basebandSampleRate;
    double channelSampleRate = ((double) m_basebandSampleRate) / (1<<m_settings.m_log2Interp);
    QLocale loc;
    ui->offsetFrequencyText->setText(tr("%1 Hz").arg(loc.toString(shift)));
    ui->channelRateText->setText(tr("%1k").arg(QString::number(channelSampleRate / 1000.0, 'g', 5)));
    m_channelMarker.setCenterFrequency(shift);
    m_channelMarker.setBandwidth(channelSampleRate);
}

void BeamSteeringCWModGUI::leaveEvent(QEvent*)
{
    m_channelMarker.setHighlighted(false);
}

void BeamSteeringCWModGUI::enterEvent(EnterEventType*)
{
    m_channelMarker.setHighlighted(true);
}

void BeamSteeringCWModGUI::handleSourceMessages()
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

void BeamSteeringCWModGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

void BeamSteeringCWModGUI::onMenuDialogCalled(const QPoint &p)
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

        applySettings();
    }

    resetContextMenuType();
}

void BeamSteeringCWModGUI::on_channelOutput_currentIndexChanged(int index)
{
    m_settings.m_channelOutput = index;
    applySettings();
}

void BeamSteeringCWModGUI::on_interpolationFactor_currentIndexChanged(int index)
{
    m_settings.m_log2Interp = index;
    applyInterpolation();
}

void BeamSteeringCWModGUI::on_position_valueChanged(int value)
{
    m_settings.m_filterChainHash = value;
    applyPosition();
}

void BeamSteeringCWModGUI::on_steeringDegrees_valueChanged(int value)
{
    m_settings.m_steerDegrees = value;
    ui->steeringDegreesText->setText(tr("%1").arg(m_settings.m_steerDegrees));
    applySettings();
}

void BeamSteeringCWModGUI::applyInterpolation()
{
    uint32_t maxHash = 1;

    for (uint32_t i = 0; i < m_settings.m_log2Interp; i++) {
        maxHash *= 3;
    }

    ui->position->setMaximum(maxHash-1);
    ui->position->setValue(m_settings.m_filterChainHash);
    m_settings.m_filterChainHash = ui->position->value();
    applyPosition();
}

void BeamSteeringCWModGUI::applyPosition()
{
    ui->filterChainIndex->setText(tr("%1").arg(m_settings.m_filterChainHash));
    QString s;
    m_shiftFrequencyFactor = HBFilterChainConverter::convertToString(m_settings.m_log2Interp, m_settings.m_filterChainHash, s);
    ui->filterChainText->setText(s);

    displayRateAndShift();
    updateAbsoluteCenterFrequency();
    applySettings();
}

void BeamSteeringCWModGUI::tick()
{
    if (++m_tickCount == 20) { // once per second
        m_tickCount = 0;
    }
}

void BeamSteeringCWModGUI::makeUIConnections()
{
    QObject::connect(ui->channelOutput, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BeamSteeringCWModGUI::on_channelOutput_currentIndexChanged);
    QObject::connect(ui->interpolationFactor, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BeamSteeringCWModGUI::on_interpolationFactor_currentIndexChanged);
    QObject::connect(ui->position, &QSlider::valueChanged, this, &BeamSteeringCWModGUI::on_position_valueChanged);
    QObject::connect(ui->steeringDegrees, &QSlider::valueChanged, this, &BeamSteeringCWModGUI::on_steeringDegrees_valueChanged);
}

void BeamSteeringCWModGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_centerFrequency + m_shiftFrequencyFactor * m_basebandSampleRate);
}
