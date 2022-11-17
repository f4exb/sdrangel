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
#include "mainwindow.h"

#include "remotesinkgui.h"
#include "remotesink.h"
#include "ui_remotesinkgui.h"

RemoteSinkGUI* RemoteSinkGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *channelRx)
{
    RemoteSinkGUI* gui = new RemoteSinkGUI(pluginAPI, deviceUISet, channelRx);
    return gui;
}

void RemoteSinkGUI::destroy()
{
    delete this;
}

void RemoteSinkGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray RemoteSinkGUI::serialize() const
{
    return m_settings.serialize();
}

bool RemoteSinkGUI::deserialize(const QByteArray& data)
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

bool RemoteSinkGUI::handleMessage(const Message& message)
{
    if (RemoteSink::MsgConfigureRemoteSink::match(message))
    {
        const RemoteSink::MsgConfigureRemoteSink& cfg = (RemoteSink::MsgConfigureRemoteSink&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        blockApplySettings(false);

        return true;
    }
    else if (DSPSignalNotification::match(message))
    {
        DSPSignalNotification& cfg = (DSPSignalNotification&) message;
        m_deviceCenterFrequency = cfg.getCenterFrequency();
        m_basebandSampleRate = cfg.getSampleRate();
        qDebug("RemoteSinkGUI::handleMessage: DSPSignalNotification: m_basebandSampleRate: %d", m_basebandSampleRate);
        displayRateAndShift();

        return true;
    }
    else
    {
        return false;
    }
}

RemoteSinkGUI::RemoteSinkGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *channelrx, QWidget* parent) :
        ChannelGUI(parent),
        ui(new Ui::RemoteSinkGUI),
        m_pluginAPI(pluginAPI),
        m_deviceUISet(deviceUISet),
        m_basebandSampleRate(0),
        m_deviceCenterFrequency(0),
        m_tickCount(0)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/remotesink/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_remoteSink = (RemoteSink*) channelrx;
    m_remoteSink->setMessageQueueToGUI(getInputMessageQueue());
    m_basebandSampleRate = m_remoteSink->getBasebandSampleRate();

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(m_settings.m_rgbColor);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("Remote source");
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setRollupState(&m_rollupState);

    m_deviceUISet->addChannelMarker(&m_channelMarker);

    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));

    displaySettings();
    makeUIConnections();
    applySettings(true);
}

RemoteSinkGUI::~RemoteSinkGUI()
{
    delete ui;
}

void RemoteSinkGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void RemoteSinkGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        setTitleColor(m_channelMarker.getColor());

        RemoteSink::MsgConfigureRemoteSink* message = RemoteSink::MsgConfigureRemoteSink::create(m_settings, force);
        m_remoteSink->getInputMessageQueue()->push(message);
    }
}

void RemoteSinkGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.setBandwidth(m_basebandSampleRate); // TODO
    m_channelMarker.setMovable(false); // do not let user move the center arbitrarily
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());

    blockApplySettings(true);
    ui->decimationFactor->setCurrentIndex(m_settings.m_log2Decim);
    ui->dataAddress->setText(m_settings.m_dataAddress);
    ui->dataPort->setText(tr("%1").arg(m_settings.m_dataPort));
    QString s = QString::number(128 + m_settings.m_nbFECBlocks, 'f', 0);
    QString s1 = QString::number(m_settings.m_nbFECBlocks, 'f', 0);
    ui->nominalNbBlocksText->setText(tr("%1/%2").arg(s).arg(s1));
    ui->nbTxBytes->setCurrentIndex(log2(m_settings.m_nbTxBytes));
    applyDecimation();
    updateIndexLabel();
    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void RemoteSinkGUI::displayRateAndShift()
{
    int shift = m_shiftFrequencyFactor * m_basebandSampleRate;
    double channelSampleRate = ((double) m_basebandSampleRate) / (1<<m_settings.m_log2Decim);
    QLocale loc;
    ui->offsetFrequencyText->setText(tr("%1 Hz").arg(loc.toString(shift)));
    ui->channelRateText->setText(tr("%1k").arg(QString::number(channelSampleRate / 1000.0, 'g', 5)));
    m_channelMarker.setCenterFrequency(shift);
    m_channelMarker.setBandwidth(channelSampleRate);
}

void RemoteSinkGUI::leaveEvent(QEvent* event)
{
    m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void RemoteSinkGUI::enterEvent(EnterEventType* event)
{
    m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void RemoteSinkGUI::handleSourceMessages()
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

void RemoteSinkGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

void RemoteSinkGUI::onMenuDialogCalled(const QPoint &p)
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
            dialog.setNumberOfStreams(m_remoteSink->getNumberOfDeviceStreams());
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

void RemoteSinkGUI::on_decimationFactor_currentIndexChanged(int index)
{
    m_settings.m_log2Decim = index;
    applyDecimation();
}

void RemoteSinkGUI::on_position_valueChanged(int value)
{
    m_settings.m_filterChainHash = value;
    applyPosition();
}

void RemoteSinkGUI::on_dataAddress_returnPressed()
{
    m_settings.m_dataAddress = ui->dataAddress->text();
    applySettings();
}

void RemoteSinkGUI::on_dataPort_returnPressed()
{
    bool dataOk;
    int dataPort = ui->dataPort->text().toInt(&dataOk);

    if((!dataOk) || (dataPort < 1024) || (dataPort > 65535))
    {
        return;
    }
    else
    {
        m_settings.m_dataPort = dataPort;
    }

    applySettings();
}

void RemoteSinkGUI::on_dataApplyButton_clicked(bool checked)
{
    (void) checked;
    m_settings.m_dataAddress = ui->dataAddress->text();

    bool dataOk;
    int udpDataPort = ui->dataPort->text().toInt(&dataOk);

    if((dataOk) && (udpDataPort >= 1024) && (udpDataPort < 65535))
    {
        m_settings.m_dataPort = udpDataPort;
    }

    applySettings();
}

void RemoteSinkGUI::on_nbFECBlocks_valueChanged(int value)
{
    m_settings.m_nbFECBlocks = value;
    int nbOriginalBlocks = 128;
    int nbFECBlocks = value;
    QString s = QString::number(nbOriginalBlocks + nbFECBlocks, 'f', 0);
    QString s1 = QString::number(nbFECBlocks, 'f', 0);
    ui->nominalNbBlocksText->setText(tr("%1/%2").arg(s).arg(s1));
    applySettings();
}

void RemoteSinkGUI::on_nbTxBytes_currentIndexChanged(int index)
{
    m_settings.m_nbTxBytes = 1 << index;
    applySettings();
}

void RemoteSinkGUI::applyDecimation()
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

void RemoteSinkGUI::applyPosition()
{
    ui->filterChainIndex->setText(tr("%1").arg(m_settings.m_filterChainHash));
    QString s;
    m_shiftFrequencyFactor = HBFilterChainConverter::convertToString(m_settings.m_log2Decim, m_settings.m_filterChainHash, s);
    ui->filterChainText->setText(s);

    updateAbsoluteCenterFrequency();
    displayRateAndShift();
    applySettings();
}

void RemoteSinkGUI::tick()
{
    if (++m_tickCount == 20) { // once per second
        m_tickCount = 0;
    }
}

void RemoteSinkGUI::makeUIConnections()
{
    QObject::connect(ui->decimationFactor, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RemoteSinkGUI::on_decimationFactor_currentIndexChanged);
    QObject::connect(ui->position, &QSlider::valueChanged, this, &RemoteSinkGUI::on_position_valueChanged);
    QObject::connect(ui->dataAddress, &QLineEdit::returnPressed, this, &RemoteSinkGUI::on_dataAddress_returnPressed);
    QObject::connect(ui->dataPort, &QLineEdit::returnPressed, this, &RemoteSinkGUI::on_dataPort_returnPressed);
    QObject::connect(ui->dataApplyButton, &QPushButton::clicked, this, &RemoteSinkGUI::on_dataApplyButton_clicked);
    QObject::connect(ui->nbFECBlocks, &QDial::valueChanged, this, &RemoteSinkGUI::on_nbFECBlocks_valueChanged);
    QObject::connect(ui->nbTxBytes, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RemoteSinkGUI::on_nbTxBytes_currentIndexChanged);
}

void RemoteSinkGUI::updateAbsoluteCenterFrequency()
{
    int shift = m_shiftFrequencyFactor * m_basebandSampleRate;
    setStatusFrequency(m_deviceCenterFrequency + shift);
}
