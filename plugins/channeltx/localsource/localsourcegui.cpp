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
#include "gui/devicestreamselectiondialog.h"
#include "dsp/hbfilterchainconverter.h"
#include "mainwindow.h"

#include "localsourcegui.h"
#include "localsource.h"
#include "ui_localsourcegui.h"

LocalSourceGUI* LocalSourceGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx)
{
    LocalSourceGUI* gui = new LocalSourceGUI(pluginAPI, deviceUISet, channelTx);
    return gui;
}

void LocalSourceGUI::destroy()
{
    delete this;
}

void LocalSourceGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray LocalSourceGUI::serialize() const
{
    return m_settings.serialize();
}

bool LocalSourceGUI::deserialize(const QByteArray& data)
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

bool LocalSourceGUI::handleMessage(const Message& message)
{
    if (LocalSource::MsgBasebandSampleRateNotification::match(message))
    {
        LocalSource::MsgBasebandSampleRateNotification& notif = (LocalSource::MsgBasebandSampleRateNotification&) message;
        m_basebandSampleRate = notif.getBasebandSampleRate();
        displayRateAndShift();
        return true;
    }
    else if (LocalSource::MsgConfigureLocalSource::match(message))
    {
        const LocalSource::MsgConfigureLocalSource& cfg = (LocalSource::MsgConfigureLocalSource&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else
    {
        return false;
    }
}

LocalSourceGUI::LocalSourceGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channeltx, QWidget* parent) :
        ChannelGUI(parent),
        ui(new Ui::LocalSourceGUI),
        m_pluginAPI(pluginAPI),
        m_deviceUISet(deviceUISet),
        m_basebandSampleRate(0),
        m_tickCount(0)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose, true);
    connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_localSource = (LocalSource*) channeltx;
    m_localSource->setMessageQueueToGUI(getInputMessageQueue());

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(m_settings.m_rgbColor);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("Local Source");
    m_channelMarker.setSourceOrSinkStream(false);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    m_settings.setChannelMarker(&m_channelMarker);

    m_deviceUISet->addChannelMarker(&m_channelMarker);
    m_deviceUISet->addRollupWidget(this);

    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));

    updateLocalDevices();
    displaySettings();
    applySettings(true);
}

LocalSourceGUI::~LocalSourceGUI()
{
    delete ui;
}

void LocalSourceGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void LocalSourceGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        setTitleColor(m_channelMarker.getColor());

        LocalSource::MsgConfigureLocalSource* message = LocalSource::MsgConfigureLocalSource::create(m_settings, force);
        m_localSource->getInputMessageQueue()->push(message);
    }
}

void LocalSourceGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.setBandwidth(m_basebandSampleRate / (1<<m_settings.m_log2Interp)); // TODO
    m_channelMarker.setMovable(false); // do not let user move the center arbitrarily
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    displayStreamIndex();

    blockApplySettings(true);
    ui->interpolationFactor->setCurrentIndex(m_settings.m_log2Interp);
    ui->localDevicePlay->setChecked(m_settings.m_play);
    applyInterpolation();
    blockApplySettings(false);
}

void LocalSourceGUI::displayRateAndShift()
{
    int shift = m_shiftFrequencyFactor * m_basebandSampleRate;
    double channelSampleRate = ((double) m_basebandSampleRate) / (1<<m_settings.m_log2Interp);
    QLocale loc;
    ui->offsetFrequencyText->setText(tr("%1 Hz").arg(loc.toString(shift)));
    ui->channelRateText->setText(tr("%1k").arg(QString::number(channelSampleRate / 1000.0, 'g', 5)));
    m_channelMarker.setCenterFrequency(shift);
    m_channelMarker.setBandwidth(channelSampleRate);
}

void LocalSourceGUI::displayStreamIndex()
{
    if (m_deviceUISet->m_deviceMIMOEngine) {
        setStreamIndicator(tr("%1").arg(m_settings.m_streamIndex));
    } else {
        setStreamIndicator("S"); // single channel indicator
    }
}

void LocalSourceGUI::updateLocalDevices()
{
    std::vector<uint32_t> localDevicesIndexes;
    m_localSource->getLocalDevices(localDevicesIndexes);
    ui->localDevice->clear();
    std::vector<uint32_t>::const_iterator it = localDevicesIndexes.begin();

    for (; it != localDevicesIndexes.end(); ++it) {
        ui->localDevice->addItem(tr("%1").arg(*it), QVariant(*it));
    }
}

void LocalSourceGUI::leaveEvent(QEvent*)
{
    m_channelMarker.setHighlighted(false);
}

void LocalSourceGUI::enterEvent(QEvent*)
{
    m_channelMarker.setHighlighted(true);
}

void LocalSourceGUI::handleSourceMessages()
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

void LocalSourceGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;
}

void LocalSourceGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuChannelSettings)
    {
        BasicChannelSettingsDialog dialog(&m_channelMarker, this);
        dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
        dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
        dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
        dialog.setReverseAPIDeviceIndex(m_settings.m_reverseAPIDeviceIndex);
        dialog.setReverseAPIChannelIndex(m_settings.m_reverseAPIChannelIndex);

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
        setTitleColor(m_settings.m_rgbColor);

        applySettings();
    }
    else if ((m_contextMenuType == ContextMenuStreamSettings) && (m_deviceUISet->m_deviceMIMOEngine))
    {
        DeviceStreamSelectionDialog dialog(this);
        dialog.setNumberOfStreams(m_localSource->getNumberOfDeviceStreams());
        dialog.setStreamIndex(m_settings.m_streamIndex);
        dialog.move(p);
        dialog.exec();

        m_settings.m_streamIndex = dialog.getSelectedStreamIndex();
        m_channelMarker.clearStreamIndexes();
        m_channelMarker.addStreamIndex(m_settings.m_streamIndex);
        displayStreamIndex();
        applySettings();
    }

    resetContextMenuType();
}

void LocalSourceGUI::on_interpolationFactor_currentIndexChanged(int index)
{
    m_settings.m_log2Interp = index;
    applyInterpolation();
}

void LocalSourceGUI::on_position_valueChanged(int value)
{
    m_settings.m_filterChainHash = value;
    applyPosition();
}

void LocalSourceGUI::on_localDevice_currentIndexChanged(int index)
{
    m_settings.m_localDeviceIndex = ui->localDevice->itemData(index).toInt();
    applySettings();
}

void LocalSourceGUI::on_localDevicesRefresh_clicked(bool checked)
{
    (void) checked;
    updateLocalDevices();
}

void LocalSourceGUI::on_localDevicePlay_toggled(bool checked)
{
    m_settings.m_play = checked;
    applySettings();
}

void LocalSourceGUI::applyInterpolation()
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

void LocalSourceGUI::applyPosition()
{
    ui->filterChainIndex->setText(tr("%1").arg(m_settings.m_filterChainHash));
    QString s;
    m_shiftFrequencyFactor = HBFilterChainConverter::convertToString(m_settings.m_log2Interp, m_settings.m_filterChainHash, s);
    ui->filterChainText->setText(s);

    displayRateAndShift();
    applySettings();
}

void LocalSourceGUI::tick()
{
    if (++m_tickCount == 20) { // once per second
        m_tickCount = 0;
    }
}
