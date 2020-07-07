///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#include "sigmffilesinkgui.h"
#include "sigmffilesink.h"
#include "ui_sigmffilesinkgui.h"

SigMFFileSinkGUI* SigMFFileSinkGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *channelRx)
{
    SigMFFileSinkGUI* gui = new SigMFFileSinkGUI(pluginAPI, deviceUISet, channelRx);
    return gui;
}

void SigMFFileSinkGUI::destroy()
{
    delete this;
}

void SigMFFileSinkGUI::setName(const QString& name)
{
    setObjectName(name);
}

QString SigMFFileSinkGUI::getName() const
{
    return objectName();
}

qint64 SigMFFileSinkGUI::getCenterFrequency() const {
    return 0;
}

void SigMFFileSinkGUI::setCenterFrequency(qint64 centerFrequency)
{
    (void) centerFrequency;
}

void SigMFFileSinkGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray SigMFFileSinkGUI::serialize() const
{
    return m_settings.serialize();
}

bool SigMFFileSinkGUI::deserialize(const QByteArray& data)
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

bool SigMFFileSinkGUI::handleMessage(const Message& message)
{
    if (SigMFFileSink::MsgBasebandSampleRateNotification::match(message))
    {
        SigMFFileSink::MsgBasebandSampleRateNotification& notif = (SigMFFileSink::MsgBasebandSampleRateNotification&) message;
        //m_channelMarker.setBandwidth(notif.getSampleRate());
        m_basebandSampleRate = notif.getSampleRate();
        displayRateAndShift();
        return true;
    }
    else if (SigMFFileSink::MsgConfigureSigMFFileSink::match(message))
    {
        const SigMFFileSink::MsgConfigureSigMFFileSink& cfg = (SigMFFileSink::MsgConfigureSigMFFileSink&) message;
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

SigMFFileSinkGUI::SigMFFileSinkGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *channelrx, QWidget* parent) :
        RollupWidget(parent),
        ui(new Ui::SigMFFileSinkGUI),
        m_pluginAPI(pluginAPI),
        m_deviceUISet(deviceUISet),
        m_basebandSampleRate(0),
        m_tickCount(0)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose, true);
    connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_sigMFFileSink = (SigMFFileSink*) channelrx;
    m_sigMFFileSink->setMessageQueueToGUI(getInputMessageQueue());

	ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 8, -99999999, 99999999);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(m_settings.m_rgbColor);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("SigMF File Sink");
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    m_settings.setChannelMarker(&m_channelMarker);

    m_deviceUISet->registerRxChannelInstance(SigMFFileSink::m_channelIdURI, this);
    m_deviceUISet->addChannelMarker(&m_channelMarker);
    m_deviceUISet->addRollupWidget(this);

    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));
    //connect(&(m_deviceUISet->m_deviceSourceAPI->getMasterTimer()), SIGNAL(timeout()), this, SLOT(tick()));

    displaySettings();
    applySettings(true);
}

SigMFFileSinkGUI::~SigMFFileSinkGUI()
{
    m_deviceUISet->removeRxChannelInstance(this);
    delete m_sigMFFileSink; // TODO: check this: when the GUI closes it has to delete the demodulator
    delete ui;
}

void SigMFFileSinkGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void SigMFFileSinkGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        setTitleColor(m_channelMarker.getColor());

        SigMFFileSink::MsgConfigureSigMFFileSink* message = SigMFFileSink::MsgConfigureSigMFFileSink::create(m_settings, force);
        m_sigMFFileSink->getInputMessageQueue()->push(message);
    }
}

void SigMFFileSinkGUI::displaySettings()
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

    blockApplySettings(true);

    ui->decimationFactor->setCurrentIndex(m_settings.m_log2Decim);
    applyDecimation();
    displayStreamIndex();

    blockApplySettings(false);
}

void SigMFFileSinkGUI::displayStreamIndex()
{
    if (m_deviceUISet->m_deviceMIMOEngine) {
        setStreamIndicator(tr("%1").arg(m_settings.m_streamIndex));
    } else {
        setStreamIndicator("S"); // single channel indicator
    }
}

void SigMFFileSinkGUI::displayRateAndShift()
{
    int shift = m_shiftFrequencyFactor * m_basebandSampleRate;
    ui->deltaFrequency->setValue(shift);
    //QLocale loc;
    //ui->offsetFrequencyText->setText(tr("%1 Hz").arg(loc.toString(shift)));
    double channelSampleRate = ((double) m_basebandSampleRate) / (1<<m_settings.m_log2Decim);
    ui->channelRateText->setText(tr("%1k").arg(QString::number(channelSampleRate / 1000.0, 'g', 5)));
    m_channelMarker.setCenterFrequency(shift);
    m_channelMarker.setBandwidth(channelSampleRate);
}

void SigMFFileSinkGUI::leaveEvent(QEvent*)
{
    m_channelMarker.setHighlighted(false);
}

void SigMFFileSinkGUI::enterEvent(QEvent*)
{
    m_channelMarker.setHighlighted(true);
}

void SigMFFileSinkGUI::handleSourceMessages()
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

void SigMFFileSinkGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;
}

void SigMFFileSinkGUI::onMenuDialogCalled(const QPoint &p)
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
        dialog.setNumberOfStreams(m_sigMFFileSink->getNumberOfDeviceStreams());
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

void SigMFFileSinkGUI::on_decimationFactor_currentIndexChanged(int index)
{
    m_settings.m_log2Decim = index;
    applyDecimation();
}

void SigMFFileSinkGUI::on_position_valueChanged(int value)
{
    m_settings.m_filterChainHash = value;
    applyPosition();
}

void SigMFFileSinkGUI::on_record_toggled(bool checked)
{
    m_sigMFFileSink->record(checked);
}

void SigMFFileSinkGUI::applyDecimation()
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

void SigMFFileSinkGUI::applyPosition()
{
    ui->filterChainIndex->setText(tr("%1").arg(m_settings.m_filterChainHash));
    QString s;
    m_shiftFrequencyFactor = HBFilterChainConverter::convertToString(m_settings.m_log2Decim, m_settings.m_filterChainHash, s);
    ui->filterChainText->setText(s);

    displayRateAndShift();
    applySettings();
}

void SigMFFileSinkGUI::tick()
{
    if (++m_tickCount == 20) { // once per second
        m_tickCount = 0;
    }
}
