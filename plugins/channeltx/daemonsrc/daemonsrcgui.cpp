///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include "device/devicesinkapi.h"
#include "device/deviceuiset.h"
#include "gui/basicchannelsettingsdialog.h"
#include "mainwindow.h"

#include "daemonsrc.h"
#include "ui_daemonsrcgui.h"
#include "daemonsrcgui.h"

DaemonSourceGUI* DaemonSourceGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx)
{
    DaemonSourceGUI* gui = new DaemonSourceGUI(pluginAPI, deviceUISet, channelTx);
    return gui;
}

void DaemonSourceGUI::destroy()
{
    delete this;
}

void DaemonSourceGUI::setName(const QString& name)
{
    setObjectName(name);
}

QString DaemonSourceGUI::getName() const
{
    return objectName();
}

qint64 DaemonSourceGUI::getCenterFrequency() const {
    return 0;
}

void DaemonSourceGUI::setCenterFrequency(qint64 centerFrequency __attribute__((unused)))
{
}

void DaemonSourceGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray DaemonSourceGUI::serialize() const
{
    return m_settings.serialize();
}

bool DaemonSourceGUI::deserialize(const QByteArray& data)
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

bool DaemonSourceGUI::handleMessage(const Message& message)
{
    if (DaemonSource::MsgSampleRateNotification::match(message))
    {
        DaemonSource::MsgSampleRateNotification& notif = (DaemonSource::MsgSampleRateNotification&) message;
        m_channelMarker.setBandwidth(notif.getSampleRate());
        return true;
    }
    else if (DaemonSource::MsgConfigureDaemonSource::match(message))
    {
        const DaemonSource::MsgConfigureDaemonSource& cfg = (DaemonSource::MsgConfigureDaemonSource&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (DaemonSource::MsgReportStreamData::match(message))
    {
        const DaemonSource::MsgReportStreamData& report = (DaemonSource::MsgReportStreamData&) message;
        ui->sampleRate->setText(QString("%1").arg(report.get_sampleRate()));
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

DaemonSourceGUI::DaemonSourceGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx __attribute__((unused)), QWidget* parent) :
        RollupWidget(parent),
        ui(new Ui::DaemonSourceGUI),
        m_pluginAPI(pluginAPI),
        m_deviceUISet(deviceUISet),
        m_countUnrecoverable(0),
        m_countRecovered(0),
        m_lastCountUnrecoverable(0),
        m_lastCountRecovered(0),
        m_lastSampleCount(0),
        m_lastTimestampUs(0),
        m_resetCounts(true),
        m_tickCount(0)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose, true);
    connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_daemonSrc = (DaemonSource*) channelTx;
    m_daemonSrc->setMessageQueueToGUI(getInputMessageQueue());

    connect(&(m_deviceUISet->m_deviceSinkAPI->getMasterTimer()), SIGNAL(timeout()), this, SLOT(tick()));

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(m_settings.m_rgbColor);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("Daemon source");
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    m_settings.setChannelMarker(&m_channelMarker);

    m_deviceUISet->registerTxChannelInstance(DaemonSource::m_channelIdURI, this);
    m_deviceUISet->addChannelMarker(&m_channelMarker);
    m_deviceUISet->addRollupWidget(this);

    connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));

    m_time.start();

    displaySettings();
    applySettings(true);
}

DaemonSourceGUI::~DaemonSourceGUI()
{
    m_deviceUISet->removeTxChannelInstance(this);
    delete m_daemonSrc;
    delete ui;
}

void DaemonSourceGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void DaemonSourceGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        setTitleColor(m_channelMarker.getColor());

        DaemonSource::MsgConfigureDaemonSource* message = DaemonSource::MsgConfigureDaemonSource::create(m_settings, force);
        m_daemonSrc->getInputMessageQueue()->push(message);
    }
}

void DaemonSourceGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.setBandwidth(5000); // TODO
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());

    blockApplySettings(true);
    ui->dataAddress->setText(m_settings.m_dataAddress);
    ui->dataPort->setText(tr("%1").arg(m_settings.m_dataPort));
    blockApplySettings(false);
}

void DaemonSourceGUI::leaveEvent(QEvent*)
{
    m_channelMarker.setHighlighted(false);
}

void DaemonSourceGUI::enterEvent(QEvent*)
{
    m_channelMarker.setHighlighted(true);
}

void DaemonSourceGUI::handleSourceMessages()
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

void DaemonSourceGUI::onWidgetRolled(QWidget* widget __attribute__((unused)), bool rollDown __attribute__((unused)))
{
}

void DaemonSourceGUI::onMenuDialogCalled(const QPoint &p)
{
    BasicChannelSettingsDialog dialog(&m_channelMarker, this);
    dialog.move(p);
    dialog.exec();

    m_settings.m_rgbColor = m_channelMarker.getColor().rgb();
    m_settings.m_title = m_channelMarker.getTitle();

    setWindowTitle(m_settings.m_title);
    setTitleColor(m_settings.m_rgbColor);

    applySettings();
}

void DaemonSourceGUI::on_dataAddress_returnPressed()
{
    m_settings.m_dataAddress = ui->dataAddress->text();
    applySettings();
}

void DaemonSourceGUI::on_dataPort_returnPressed()
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

void DaemonSourceGUI::on_dataApplyButton_clicked(bool checked __attribute__((unused)))
{
    m_settings.m_dataAddress = ui->dataAddress->text();

    bool dataOk;
    int udpDataPort = ui->dataPort->text().toInt(&dataOk);

    if((dataOk) && (udpDataPort >= 1024) && (udpDataPort < 65535))
    {
        m_settings.m_dataPort = udpDataPort;
    }

    applySettings();
}

void DaemonSourceGUI::on_eventCountsReset_clicked(bool checked __attribute__((unused)))
{
    m_countUnrecoverable = 0;
    m_countRecovered = 0;
    m_time.start();
    displayEventCounts();
    displayEventTimer();
}

void DaemonSourceGUI::displayEventCounts()
{
    QString nstr = QString("%1").arg(m_countUnrecoverable, 3, 10, QChar('0'));
    ui->eventUnrecText->setText(nstr);
    nstr = QString("%1").arg(m_countRecovered, 3, 10, QChar('0'));
    ui->eventRecText->setText(nstr);
}

void DaemonSourceGUI::displayEventStatus(int recoverableCount, int unrecoverableCount)
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

void DaemonSourceGUI::displayEventTimer()
{
    int elapsedTimeMillis = m_time.elapsed();
    QTime recordLength(0, 0, 0, 0);
    recordLength = recordLength.addSecs(elapsedTimeMillis/1000);
    QString s_time = recordLength.toString("HH:mm:ss");
    ui->eventCountsTimeText->setText(s_time);
}

void DaemonSourceGUI::tick()
{
    if (++m_tickCount == 20) // once per second
    {
        DaemonSource::MsgQueryStreamData *msg = DaemonSource::MsgQueryStreamData::create();
        m_daemonSrc->getInputMessageQueue()->push(msg);

        displayEventTimer();

        m_tickCount = 0;
    }
}

void DaemonSourceGUI::channelMarkerChangedByCursor()
{
}
