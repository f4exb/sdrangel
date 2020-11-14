///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018-2019 Edouard Griffiths, F4EXB                              //
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

#include <QTime>

#include "device/deviceapi.h"
#include "device/deviceuiset.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "mainwindow.h"

#include "remotesource.h"
#include "ui_remotesourcegui.h"

#include "remotesourcegui.h"

RemoteSourceGUI* RemoteSourceGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx)
{
    RemoteSourceGUI* gui = new RemoteSourceGUI(pluginAPI, deviceUISet, channelTx);
    return gui;
}

void RemoteSourceGUI::destroy()
{
    delete this;
}

void RemoteSourceGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray RemoteSourceGUI::serialize() const
{
    return m_settings.serialize();
}

bool RemoteSourceGUI::deserialize(const QByteArray& data)
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

bool RemoteSourceGUI::handleMessage(const Message& message)
{
    if (RemoteSource::MsgConfigureRemoteSource::match(message))
    {
        const RemoteSource::MsgConfigureRemoteSource& cfg = (RemoteSource::MsgConfigureRemoteSource&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (RemoteSource::MsgReportStreamData::match(message))
    {
        const RemoteSource::MsgReportStreamData& report = (RemoteSource::MsgReportStreamData&) message;

        int remoteSampleRate = report.get_sampleRate();

        if (remoteSampleRate != m_remoteSampleRate)
        {
            m_channelMarker.setBandwidth(remoteSampleRate);
            m_remoteSampleRate = remoteSampleRate;
        }

        ui->sampleRate->setText(QString("%1").arg(remoteSampleRate));
        QString nominalNbBlocksText = QString("%1/%2")
                .arg(report.get_nbOriginalBlocks() + report.get_nbFECBlocks())
                .arg(report.get_nbFECBlocks());
        ui->nominalNbBlocksText->setText(nominalNbBlocksText);
        QString queueLengthText = QString("%1/%2").arg(report.get_queueLength()).arg(report.get_queueSize());
        ui->queueLengthText->setText(queueLengthText);
        int queueLengthPercent = (report.get_queueLength()*100)/report.get_queueSize();
        ui->queueLengthGauge->setValue(queueLengthPercent);
        int unrecoverableCount = report.get_nbUncorrectableErrors();
        int recoverableCount = report.get_nbCorrectableErrors();
        uint64_t timestampUs = report.get_tv_sec()*1000000ULL + report.get_tv_usec();

        if (!m_resetCounts)
        {
            int recoverableCountDelta = recoverableCount - m_lastCountRecovered;
            int unrecoverableCountDelta = unrecoverableCount - m_lastCountUnrecoverable;
            displayEventStatus(recoverableCountDelta, unrecoverableCountDelta);
            m_countRecovered += recoverableCountDelta;
            m_countUnrecoverable += unrecoverableCountDelta;
            displayEventCounts();
        }

        uint32_t sampleCountDelta, sampleCount;
        sampleCount = report.get_readSamplesCount();

        if (sampleCount < m_lastSampleCount) {
            sampleCountDelta = (0xFFFFFFFFU - sampleCount) + m_lastSampleCount + 1;
        } else {
            sampleCountDelta = sampleCount - m_lastSampleCount;
        }

        if (sampleCountDelta == 0) {
            ui->allFramesDecoded->setStyleSheet("QToolButton { background-color : blue; }");
        }

        double remoteStreamRate = sampleCountDelta*1e6 / (double) (timestampUs - m_lastTimestampUs);

        if (remoteStreamRate != 0) {
            ui->streamRateText->setText(QString("%1").arg(remoteStreamRate, 0, 'f', 0));
        }

        m_resetCounts = false;
        m_lastCountRecovered = recoverableCount;
        m_lastCountUnrecoverable = unrecoverableCount;
        m_lastSampleCount = sampleCount;
        m_lastTimestampUs = timestampUs;

        return true;
    }
    else
    {
        return false;
    }
}

RemoteSourceGUI::RemoteSourceGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent) :
        ChannelGUI(parent),
        ui(new Ui::RemoteSourceGUI),
        m_pluginAPI(pluginAPI),
        m_deviceUISet(deviceUISet),
        m_remoteSampleRate(48000),
        m_countUnrecoverable(0),
        m_countRecovered(0),
        m_lastCountUnrecoverable(0),
        m_lastCountRecovered(0),
        m_lastSampleCount(0),
        m_lastTimestampUs(0),
        m_resetCounts(true),
        m_tickCount(0)
{
    (void) channelTx;
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose, true);
    connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_remoteSrc = (RemoteSource*) channelTx;
    m_remoteSrc->setMessageQueueToGUI(getInputMessageQueue());

    connect(&(m_deviceUISet->m_deviceAPI->getMasterTimer()), SIGNAL(timeout()), this, SLOT(tick()));

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(m_settings.m_rgbColor);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("Remote source");
    m_channelMarker.setSourceOrSinkStream(false);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    m_settings.setChannelMarker(&m_channelMarker);

    m_deviceUISet->addChannelMarker(&m_channelMarker);
    m_deviceUISet->addRollupWidget(this);

    connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));

    m_time.start();

    displaySettings();
    applySettings(true);
}

RemoteSourceGUI::~RemoteSourceGUI()
{
    delete ui;
}

void RemoteSourceGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void RemoteSourceGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        setTitleColor(m_channelMarker.getColor());

        RemoteSource::MsgConfigureRemoteSource* message = RemoteSource::MsgConfigureRemoteSource::create(m_settings, force);
        m_remoteSrc->getInputMessageQueue()->push(message);
    }
}

void RemoteSourceGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.setBandwidth(m_remoteSampleRate);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    displayStreamIndex();

    blockApplySettings(true);
    ui->dataAddress->setText(m_settings.m_dataAddress);
    ui->dataPort->setText(tr("%1").arg(m_settings.m_dataPort));
    blockApplySettings(false);
}

void RemoteSourceGUI::displayStreamIndex()
{
    if (m_deviceUISet->m_deviceMIMOEngine) {
        setStreamIndicator(tr("%1").arg(m_settings.m_streamIndex));
    } else {
        setStreamIndicator("S"); // single channel indicator
    }
}

void RemoteSourceGUI::leaveEvent(QEvent*)
{
    m_channelMarker.setHighlighted(false);
}

void RemoteSourceGUI::enterEvent(QEvent*)
{
    m_channelMarker.setHighlighted(true);
}

void RemoteSourceGUI::handleSourceMessages()
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

void RemoteSourceGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;
}

void RemoteSourceGUI::onMenuDialogCalled(const QPoint &p)
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
        dialog.setNumberOfStreams(m_remoteSrc->getNumberOfDeviceStreams());
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

void RemoteSourceGUI::on_dataAddress_returnPressed()
{
    m_settings.m_dataAddress = ui->dataAddress->text();
    applySettings();
}

void RemoteSourceGUI::on_dataPort_returnPressed()
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

void RemoteSourceGUI::on_dataApplyButton_clicked(bool checked)
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

void RemoteSourceGUI::on_eventCountsReset_clicked(bool checked)
{
    (void) checked;
    m_countUnrecoverable = 0;
    m_countRecovered = 0;
    m_time.start();
    displayEventCounts();
    displayEventTimer();
}

void RemoteSourceGUI::displayEventCounts()
{
    QString nstr = QString("%1").arg(m_countUnrecoverable, 3, 10, QChar('0'));
    ui->eventUnrecText->setText(nstr);
    nstr = QString("%1").arg(m_countRecovered, 3, 10, QChar('0'));
    ui->eventRecText->setText(nstr);
}

void RemoteSourceGUI::displayEventStatus(int recoverableCount, int unrecoverableCount)
{

    if (unrecoverableCount == 0)
    {
        if (recoverableCount == 0) {
            ui->allFramesDecoded->setStyleSheet("QToolButton { background-color : green; }");
        } else {
            ui->allFramesDecoded->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
        }
    }
    else
    {
        ui->allFramesDecoded->setStyleSheet("QToolButton { background-color : red; }");
    }
}

void RemoteSourceGUI::displayEventTimer()
{
    int elapsedTimeMillis = m_time.elapsed();
    QTime recordLength(0, 0, 0, 0);
    recordLength = recordLength.addSecs(elapsedTimeMillis/1000);
    QString s_time = recordLength.toString("HH:mm:ss");
    ui->eventCountsTimeText->setText(s_time);
}

void RemoteSourceGUI::tick()
{
    if (++m_tickCount == 20) // once per second
    {
        RemoteSource::MsgQueryStreamData *msg = RemoteSource::MsgQueryStreamData::create();
        m_remoteSrc->getInputMessageQueue()->push(msg);

        displayEventTimer();

        m_tickCount = 0;
    }
}

void RemoteSourceGUI::channelMarkerChangedByCursor()
{
}
