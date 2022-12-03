///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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
#include "gui/devicestreamselectiondialog.h"
#include "dsp/hbfilterchainconverter.h"
#include "dsp/dspcommands.h"

#include "localsinkgui.h"
#include "localsink.h"
#include "ui_localsinkgui.h"

LocalSinkGUI* LocalSinkGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *channelRx)
{
    LocalSinkGUI* gui = new LocalSinkGUI(pluginAPI, deviceUISet, channelRx);
    return gui;
}

void LocalSinkGUI::destroy()
{
    delete this;
}

void LocalSinkGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray LocalSinkGUI::serialize() const
{
    return m_settings.serialize();
}

bool LocalSinkGUI::deserialize(const QByteArray& data)
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

bool LocalSinkGUI::handleMessage(const Message& message)
{
    if (DSPSignalNotification::match(message))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) message;
        //m_channelMarker.setBandwidth(notif.getSampleRate());
        m_deviceCenterFrequency = notif.getCenterFrequency();
        m_basebandSampleRate = notif.getSampleRate();
        updateAbsoluteCenterFrequency();
        displayRateAndShift();
        return true;
    }
    else if (LocalSink::MsgConfigureLocalSink::match(message))
    {
        const LocalSink::MsgConfigureLocalSink& cfg = (LocalSink::MsgConfigureLocalSink&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (LocalSink::MsgReportDevices::match(message))
    {
        LocalSink::MsgReportDevices& report = (LocalSink::MsgReportDevices&) message;
        updateDeviceSetList(report.getDeviceSetIndexes());
        return true;
    }
    else
    {
        return false;
    }
}

LocalSinkGUI::LocalSinkGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *channelrx, QWidget* parent) :
        ChannelGUI(parent),
        ui(new Ui::LocalSinkGUI),
        m_pluginAPI(pluginAPI),
        m_deviceUISet(deviceUISet),
        m_deviceCenterFrequency(0),
        m_basebandSampleRate(0),
        m_tickCount(0)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/localsink/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_localSink = (LocalSink*) channelrx;
    m_localSink->setMessageQueueToGUI(getInputMessageQueue());

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(m_settings.m_rgbColor);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("Local Sink");
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setRollupState(&m_rollupState);

    m_deviceUISet->addChannelMarker(&m_channelMarker);

    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));

    updateDeviceSetList(m_localSink->getDeviceSetList());
    displaySettings();
    makeUIConnections();
    applySettings(true);
}

LocalSinkGUI::~LocalSinkGUI()
{
    delete ui;
}

void LocalSinkGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void LocalSinkGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        setTitleColor(m_channelMarker.getColor());

        LocalSink::MsgConfigureLocalSink* message = LocalSink::MsgConfigureLocalSink::create(m_settings, force);
        m_localSink->getInputMessageQueue()->push(message);
    }
}

void LocalSinkGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.setBandwidth(m_basebandSampleRate / (1<<m_settings.m_log2Decim));
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
    updateIndexLabel();

    getRollupContents()->restoreState(m_rollupState);
    blockApplySettings(false);
}

void LocalSinkGUI::displayRateAndShift()
{
    int shift = m_shiftFrequencyFactor * m_basebandSampleRate;
    double channelSampleRate = ((double) m_basebandSampleRate) / (1<<m_settings.m_log2Decim);
    QLocale loc;
    ui->offsetFrequencyText->setText(tr("%1 Hz").arg(loc.toString(shift)));
    ui->channelRateText->setText(tr("%1k").arg(QString::number(channelSampleRate / 1000.0, 'g', 5)));
    m_channelMarker.setCenterFrequency(shift);
    m_channelMarker.setBandwidth(channelSampleRate);
}

int LocalSinkGUI::getLocalDeviceIndexInCombo(int localDeviceIndex)
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
void LocalSinkGUI::leaveEvent(QEvent* event)
{
    m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void LocalSinkGUI::enterEvent(EnterEventType* event)
{
    m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void LocalSinkGUI::handleSourceMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()) != 0)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void LocalSinkGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

void LocalSinkGUI::onMenuDialogCalled(const QPoint &p)
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

        if (m_deviceUISet->m_deviceMIMOEngine)
        {
            dialog.setNumberOfStreams(m_localSink->getNumberOfDeviceStreams());
            dialog.setStreamIndex(m_settings.m_streamIndex);
        }

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

        if (m_deviceUISet->m_deviceMIMOEngine)
        {
            m_settings.m_streamIndex = dialog.getSelectedStreamIndex();
            m_channelMarker.clearStreamIndexes();
            m_channelMarker.addStreamIndex(m_settings.m_streamIndex);
            updateIndexLabel();
        }

        applySettings();
    }

    resetContextMenuType();
}

void LocalSinkGUI::updateDeviceSetList(const QList<int>& deviceSetIndexes)
{
    QList<int>::const_iterator it = deviceSetIndexes.begin();

    ui->localDevice->blockSignals(true);

    ui->localDevice->clear();

    for (; it != deviceSetIndexes.end(); ++it) {
        ui->localDevice->addItem(QString("R%1").arg(*it), *it);
    }

    ui->localDevice->blockSignals(false);
}

void LocalSinkGUI::on_decimationFactor_currentIndexChanged(int index)
{
    m_settings.m_log2Decim = index;
    applyDecimation();
}

void LocalSinkGUI::on_position_valueChanged(int value)
{
    m_settings.m_filterChainHash = value;
    applyPosition();
}

void LocalSinkGUI::on_localDevice_currentIndexChanged(int index)
{
    if (index >= 0)
    {
        m_settings.m_localDeviceIndex = ui->localDevice->currentData().toInt();
        applySettings();
    }
}

void LocalSinkGUI::on_localDevicePlay_toggled(bool checked)
{
    m_settings.m_play = checked;
    applySettings();
}

void LocalSinkGUI::applyDecimation()
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

void LocalSinkGUI::applyPosition()
{
    ui->filterChainIndex->setText(tr("%1").arg(m_settings.m_filterChainHash));
    QString s;
    m_shiftFrequencyFactor = HBFilterChainConverter::convertToString(m_settings.m_log2Decim, m_settings.m_filterChainHash, s);
    ui->filterChainText->setText(s);

    updateAbsoluteCenterFrequency();
    displayRateAndShift();
    applySettings();
}

void LocalSinkGUI::tick()
{
    if (++m_tickCount == 20) { // once per second
        m_tickCount = 0;
    }
}

void LocalSinkGUI::makeUIConnections()
{
    QObject::connect(ui->decimationFactor, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LocalSinkGUI::on_decimationFactor_currentIndexChanged);
    QObject::connect(ui->position, &QSlider::valueChanged, this, &LocalSinkGUI::on_position_valueChanged);
    QObject::connect(ui->localDevice, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LocalSinkGUI::on_localDevice_currentIndexChanged);
    QObject::connect(ui->localDevicePlay, &ButtonSwitch::toggled, this, &LocalSinkGUI::on_localDevicePlay_toggled);
}

void LocalSinkGUI::updateAbsoluteCenterFrequency()
{
    int shift = m_shiftFrequencyFactor * m_basebandSampleRate;
    setStatusFrequency(m_deviceCenterFrequency + shift);
}
