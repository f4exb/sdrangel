///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#include "udpsourcegui.h"

#include "device/deviceuiset.h"
#include "dsp/spectrumvis.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "plugin/pluginapi.h"
#include "maincore.h"

#include "ui_udpsourcegui.h"

UDPSourceGUI* UDPSourceGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx)
{
    UDPSourceGUI* gui = new UDPSourceGUI(pluginAPI, deviceUISet, channelTx);
    return gui;
}

void UDPSourceGUI::destroy()
{
    delete this;
}

void UDPSourceGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray UDPSourceGUI::serialize() const
{
    return m_settings.serialize();
}

bool UDPSourceGUI::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data))
    {
        displaySettings();
        applySettings(true);
        return true;
    } else {
        resetToDefaults();
        return false;
    }
}

bool UDPSourceGUI::handleMessage(const Message& message)
{
    if (UDPSource::MsgConfigureUDPSource::match(message))
    {
        const UDPSource::MsgConfigureUDPSource& cfg = (UDPSource::MsgConfigureUDPSource&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        ui->spectrumGUI->updateSettings();
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (DSPSignalNotification::match(message))
    {
        const DSPSignalNotification& notif = (const DSPSignalNotification&) message;
        m_deviceCenterFrequency = notif.getCenterFrequency();
        m_basebandSampleRate = notif.getSampleRate();
        ui->deltaFrequency->setValueRange(false, 8, -m_basebandSampleRate/2, m_basebandSampleRate/2);
        ui->deltaFrequencyLabel->setToolTip(tr("Range %1 %L2 Hz").arg(QChar(0xB1)).arg(m_basebandSampleRate/2));
        updateAbsoluteCenterFrequency();
        return true;
    }
    else
    {
        return false;
    }
}

void UDPSourceGUI::handleSourceMessages()
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

UDPSourceGUI::UDPSourceGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent) :
        ChannelGUI(parent),
        ui(new Ui::UDPSourceGUI),
        m_pluginAPI(pluginAPI),
        m_deviceUISet(deviceUISet),
        m_tickCount(0),
        m_channelMarker(this),
        m_deviceCenterFrequency(0),
        m_basebandSampleRate(1),
        m_rfBandwidthChanged(false),
        m_doApplySettings(true)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channeltx/udpsource/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_udpSource = (UDPSource*) channelTx;
    m_spectrumVis = m_udpSource->getSpectrumVis();
    m_spectrumVis->setGLSpectrum(ui->glSpectrum);
    m_udpSource->setMessageQueueToGUI(getInputMessageQueue());

    ui->fmDeviation->setEnabled(false);
    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 8, -99999999, 99999999);

    ui->glSpectrum->setCenterFrequency(0);
    ui->glSpectrum->setSampleRate(ui->sampleRate->text().toInt());
    ui->glSpectrum->setDisplayWaterfall(true);
    ui->glSpectrum->setDisplayMaxHold(true);

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    m_channelMarker.blockSignals(true);
    m_channelMarker.setBandwidth(16000);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setColor(m_settings.m_rgbColor);
    m_channelMarker.setTitle("UDP Sample Source");
    m_channelMarker.setSourceOrSinkStream(false);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    m_deviceUISet->addChannelMarker(&m_channelMarker);

    connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));

    ui->spectrumGUI->setBuddies(m_spectrumVis, ui->glSpectrum);

    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));
    m_udpSource->setLevelMeter(ui->volumeMeter);

    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setSpectrumGUI(ui->spectrumGUI);
    m_settings.setRollupState(&m_rollupState);

    displaySettings();
    makeUIConnections();
    applySettings(true);
    DialPopup::addPopupsToChildDials(this);
}

UDPSourceGUI::~UDPSourceGUI()
{
    delete ui;
}

void UDPSourceGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void UDPSourceGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        UDPSource::MsgConfigureChannelizer *msgChan = UDPSource::MsgConfigureChannelizer::create(
                m_settings.m_inputSampleRate,
                m_settings.m_inputFrequencyOffset);
        m_udpSource->getInputMessageQueue()->push(msgChan);

        UDPSource::MsgConfigureUDPSource* message = UDPSource::MsgConfigureUDPSource::create( m_settings, force);
        m_udpSource->getInputMessageQueue()->push(message);

        ui->applyBtn->setEnabled(false);
        ui->applyBtn->setStyleSheet("QPushButton { background:rgb(79,79,79); }");
    }
}

void UDPSourceGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor);

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());
    updateIndexLabel();

    blockApplySettings(true);

    ui->deltaFrequency->setValue(m_settings.m_inputFrequencyOffset);
    ui->sampleRate->setText(QString("%1").arg(roundf(m_settings.m_inputSampleRate), 0));
    ui->glSpectrum->setSampleRate(m_settings.m_inputSampleRate);
    ui->rfBandwidth->setText(QString("%1").arg(roundf(m_settings.m_rfBandwidth), 0));
    ui->fmDeviation->setText(QString("%1").arg(m_settings.m_fmDeviation, 0));
    ui->amModPercent->setText(QString("%1").arg(roundf(m_settings.m_amModFactor * 100.0), 0));

    setSampleFormatIndex(m_settings.m_sampleFormat);

    ui->channelMute->setChecked(m_settings.m_channelMute);
    ui->autoRWBalance->setChecked(m_settings.m_autoRWBalance);
    ui->stereoInput->setChecked(m_settings.m_stereoInput);

    ui->gainInText->setText(tr("%1").arg(m_settings.m_gainIn, 0, 'f', 1));
    ui->gainIn->setValue(roundf(m_settings.m_gainIn * 10.0));

    ui->gainOutText->setText(tr("%1").arg(m_settings.m_gainOut, 0, 'f', 1));
    ui->gainOut->setValue(roundf(m_settings.m_gainOut * 10.0));

    if (m_settings.m_squelchEnabled) {
        ui->squelchText->setText(tr("%1").arg(m_settings.m_squelch, 0, 'f', 0));
    } else {
        ui->squelchText->setText("---");
    }

    ui->squelch->setValue(roundf(m_settings.m_squelch));

    ui->squelchGateText->setText(tr("%1").arg(roundf(m_settings.m_squelchGate * 1000.0), 0, 'f', 0));
    ui->squelchGate->setValue(roundf(m_settings.m_squelchGate * 100.0));

    ui->localUDPAddress->setText(m_settings.m_udpAddress);
    ui->localUDPPort->setText(tr("%1").arg(m_settings.m_udpPort));
    ui->multicastAddress->setText(m_settings.m_multicastAddress);
    ui->multicastJoin->setChecked(m_settings.m_multicastJoin);

    ui->applyBtn->setEnabled(false);
    ui->applyBtn->setStyleSheet("QPushButton { background:rgb(79,79,79); }");

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void UDPSourceGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void UDPSourceGUI::on_deltaFrequency_changed(qint64 value)
{
    m_settings.m_inputFrequencyOffset = value;
    m_channelMarker.setCenterFrequency(value);
    updateAbsoluteCenterFrequency();
    applySettings();
}

void UDPSourceGUI::on_sampleFormat_currentIndexChanged(int index)
{
    if (index == (int) UDPSourceSettings::FormatNFM) {
        ui->fmDeviation->setEnabled(true);
    } else {
        ui->fmDeviation->setEnabled(false);
    }

    if (index == (int) UDPSourceSettings::FormatAM) {
        ui->amModPercent->setEnabled(true);
    } else {
        ui->amModPercent->setEnabled(false);
    }

    setSampleFormat(index);

    ui->applyBtn->setEnabled(true);
    ui->applyBtn->setStyleSheet("QPushButton { background-color : green; }");
}

void UDPSourceGUI::on_localUDPAddress_editingFinished()
{
    m_settings.m_udpAddress = ui->localUDPAddress->text();
    ui->applyBtn->setEnabled(true);
    ui->applyBtn->setStyleSheet("QPushButton { background-color : green; }");
}

void UDPSourceGUI::on_localUDPPort_editingFinished()
{
    bool ok;
    quint16 udpPort = ui->localUDPPort->text().toInt(&ok);

    if((!ok) || (udpPort < 1024)) {
        udpPort = 9998;
    }

    m_settings.m_udpPort = udpPort;
    ui->localUDPPort->setText(tr("%1").arg(m_settings.m_udpPort));

    ui->applyBtn->setEnabled(true);
    ui->applyBtn->setStyleSheet("QPushButton { background-color : green; }");
}

void UDPSourceGUI::on_multicastAddress_editingFinished()
{
    m_settings.m_multicastAddress = ui->multicastAddress->text();
    ui->applyBtn->setEnabled(true);
    ui->applyBtn->setStyleSheet("QPushButton { background-color : green; }");
}

void UDPSourceGUI::on_multicastJoin_toggled(bool checked)
{
    m_settings.m_multicastJoin = checked;
    ui->applyBtn->setEnabled(true);
    ui->applyBtn->setStyleSheet("QPushButton { background-color : green; }");
}

void UDPSourceGUI::on_sampleRate_textEdited(const QString& arg1)
{
    (void) arg1;
    bool ok;
    Real inputSampleRate = ui->sampleRate->text().toDouble(&ok);

    if ((!ok) || (inputSampleRate < 1000)) {
        m_settings.m_inputSampleRate = 48000;
        ui->sampleRate->setText(QString("%1").arg(m_settings.m_inputSampleRate, 0));
    } else {
        m_settings.m_inputSampleRate = inputSampleRate;
    }

    ui->applyBtn->setEnabled(true);
    ui->applyBtn->setStyleSheet("QPushButton { background-color : green; }");
}

void UDPSourceGUI::on_rfBandwidth_textEdited(const QString& arg1)
{
    (void) arg1;
    bool ok;
    Real rfBandwidth = ui->rfBandwidth->text().toDouble(&ok);

    if ((!ok) || (rfBandwidth > m_settings.m_inputSampleRate))
    {
        m_settings.m_rfBandwidth = m_settings.m_inputSampleRate;
        ui->rfBandwidth->setText(QString("%1").arg(m_settings.m_rfBandwidth, 0));
    }
    else
    {
        m_settings.m_rfBandwidth = rfBandwidth;
    }

    m_rfBandwidthChanged = true;

    ui->applyBtn->setEnabled(true);
    ui->applyBtn->setStyleSheet("QPushButton { background-color : green; }");
}

void UDPSourceGUI::on_fmDeviation_textEdited(const QString& arg1)
{
    (void) arg1;
    bool ok;
    int fmDeviation = ui->fmDeviation->text().toInt(&ok);

    if ((!ok) || (fmDeviation < 1)) {
        m_settings.m_fmDeviation = 2500;
        ui->fmDeviation->setText(QString("%1").arg(m_settings.m_fmDeviation));
    } else {
        m_settings.m_fmDeviation = fmDeviation;
    }

    ui->applyBtn->setEnabled(true);
    ui->applyBtn->setStyleSheet("QPushButton { background-color : green; }");
}

void UDPSourceGUI::on_amModPercent_textEdited(const QString& arg1)
{
    (void) arg1;
    bool ok;
    int amModPercent = ui->amModPercent->text().toInt(&ok);

    if ((!ok) || (amModPercent < 1) || (amModPercent > 100))
    {
        m_settings.m_amModFactor = 0.95;
        ui->amModPercent->setText(QString("%1").arg(95));
    } else {
        m_settings.m_amModFactor = amModPercent / 100.0;
    }

    ui->applyBtn->setEnabled(true);
    ui->applyBtn->setStyleSheet("QPushButton { background-color : green; }");
}

void UDPSourceGUI::on_gainIn_valueChanged(int value)
{
    m_settings.m_gainIn = value / 10.0;
    ui->gainInText->setText(tr("%1").arg(m_settings.m_gainIn, 0, 'f', 1));
    applySettings();
}

void UDPSourceGUI::on_gainOut_valueChanged(int value)
{
    m_settings.m_gainOut = value / 10.0;
    ui->gainOutText->setText(tr("%1").arg(m_settings.m_gainOut, 0, 'f', 1));
    applySettings();
}

void UDPSourceGUI::on_squelch_valueChanged(int value)
{
    m_settings.m_squelchEnabled = (value != -100);
    m_settings.m_squelch = value * 1.0;

    if (m_settings.m_squelchEnabled) {
        ui->squelchText->setText(tr("%1").arg(m_settings.m_squelch, 0, 'f', 0));
    } else {
        ui->squelchText->setText("---");
    }

    applySettings();
}

void UDPSourceGUI::on_squelchGate_valueChanged(int value)
{
    m_settings.m_squelchGate = value / 100.0;
    ui->squelchGateText->setText(tr("%1").arg(roundf(value * 10.0), 0, 'f', 0));
    applySettings();
}

void UDPSourceGUI::on_channelMute_toggled(bool checked)
{
    m_settings.m_channelMute = checked;
    applySettings();
}

void UDPSourceGUI::on_applyBtn_clicked()
{
    if (m_rfBandwidthChanged)
    {
        m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
        m_rfBandwidthChanged = false;
    }

    ui->glSpectrum->setSampleRate(m_settings.m_inputSampleRate);

    applySettings();
}

void UDPSourceGUI::on_resetUDPReadIndex_clicked()
{
    m_udpSource->resetReadIndex();
}

void UDPSourceGUI::on_autoRWBalance_toggled(bool checked)
{
    m_settings.m_autoRWBalance = checked;
    applySettings();
}

void UDPSourceGUI::on_stereoInput_toggled(bool checked)
{
    m_settings.m_stereoInput = checked;
    applySettings();
}

void UDPSourceGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    if ((widget == ui->spectrumBox) && (m_udpSource != 0))
    {
        m_udpSource->setSpectrum(rollDown);
    }

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

void UDPSourceGUI::onMenuDialogCalled(const QPoint &p)
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
            dialog.setNumberOfStreams(m_udpSource->getNumberOfDeviceStreams());
            dialog.setStreamIndex(m_settings.m_streamIndex);
        }

        dialog.move(p);
        new DialogPositioner(&dialog, false);
        dialog.exec();

        m_settings.m_rgbColor = m_channelMarker.getColor().rgb();
        m_settings.m_useReverseAPI = dialog.useReverseAPI();
        m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
        m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
        m_settings.m_reverseAPIDeviceIndex = dialog.getReverseAPIDeviceIndex();
        m_settings.m_reverseAPIChannelIndex = dialog.getReverseAPIChannelIndex();

        setWindowTitle(m_channelMarker.getTitle());
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

void UDPSourceGUI::leaveEvent(QEvent* event)
{
    m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void UDPSourceGUI::enterEvent(EnterEventType* event)
{
    m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void UDPSourceGUI::tick()
{
    m_channelPowerAvg(m_udpSource->getMagSq());
    m_inPowerAvg(m_udpSource->getInMagSq());

    if (m_tickCount % 4 == 0)
    {
        double powDb = CalcDb::dbPower(m_channelPowerAvg.asDouble());
        ui->channelPower->setText(tr("%1 dB").arg(powDb, 0, 'f', 1));
        double inPowDb = CalcDb::dbPower(m_inPowerAvg.asDouble());
        ui->inputPower->setText(tr("%1").arg(inPowDb, 0, 'f', 1));
    }

    int32_t bufferGauge = m_udpSource->getBufferGauge();
    ui->bufferGaugeNegative->setValue((bufferGauge < 0 ? -bufferGauge : 0));
    ui->bufferGaugePositive->setValue((bufferGauge < 0 ? 0 : bufferGauge));
    QString s = QString::number(bufferGauge, 'f', 0);
    ui->bufferRWBalanceText->setText(tr("%1").arg(s));

    if (m_udpSource->getSquelchOpen()) {
        ui->channelMute->setStyleSheet("QToolButton { background-color : green; }");
    } else {
        ui->channelMute->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
    }

    m_tickCount++;
}

void UDPSourceGUI::setSampleFormatIndex(const UDPSourceSettings::SampleFormat& sampleFormat)
{
    switch(sampleFormat)
    {
        case UDPSourceSettings::FormatSnLE:
            ui->sampleFormat->setCurrentIndex(0);
            ui->fmDeviation->setEnabled(false);
            ui->stereoInput->setChecked(true);
            ui->stereoInput->setEnabled(false);
            break;
        case UDPSourceSettings::FormatNFM:
            ui->sampleFormat->setCurrentIndex(1);
            ui->fmDeviation->setEnabled(true);
            ui->stereoInput->setEnabled(true);
            break;
        case UDPSourceSettings::FormatLSB:
            ui->sampleFormat->setCurrentIndex(2);
            ui->fmDeviation->setEnabled(false);
            ui->stereoInput->setEnabled(true);
            break;
        case UDPSourceSettings::FormatUSB:
            ui->sampleFormat->setCurrentIndex(3);
            ui->fmDeviation->setEnabled(false);
            ui->stereoInput->setEnabled(true);
            break;
        case UDPSourceSettings::FormatAM:
            ui->sampleFormat->setCurrentIndex(4);
            ui->fmDeviation->setEnabled(false);
            ui->stereoInput->setEnabled(true);
            break;
        default:
            ui->sampleFormat->setCurrentIndex(0);
            ui->fmDeviation->setEnabled(false);
            ui->stereoInput->setChecked(true);
            ui->stereoInput->setEnabled(false);
            break;
    }
}

void UDPSourceGUI::setSampleFormat(int index)
{
    switch(index)
    {
    case 0:
        m_settings.m_sampleFormat = UDPSourceSettings::FormatSnLE;
        ui->fmDeviation->setEnabled(false);
        ui->stereoInput->setChecked(true);
        ui->stereoInput->setEnabled(false);
        break;
    case 1:
        m_settings.m_sampleFormat = UDPSourceSettings::FormatNFM;
        ui->fmDeviation->setEnabled(true);
        ui->stereoInput->setEnabled(true);
        break;
    case 2:
        m_settings.m_sampleFormat = UDPSourceSettings::FormatLSB;
        ui->fmDeviation->setEnabled(false);
        ui->stereoInput->setEnabled(true);
        break;
    case 3:
        m_settings.m_sampleFormat = UDPSourceSettings::FormatUSB;
        ui->fmDeviation->setEnabled(false);
        ui->stereoInput->setEnabled(true);
        break;
    case 4:
        m_settings.m_sampleFormat = UDPSourceSettings::FormatAM;
        ui->fmDeviation->setEnabled(false);
        ui->stereoInput->setEnabled(true);
        break;
    default:
        m_settings.m_sampleFormat = UDPSourceSettings::FormatSnLE;
        ui->fmDeviation->setEnabled(false);
        ui->stereoInput->setChecked(true);
        ui->stereoInput->setEnabled(false);
        break;
    }
}

void UDPSourceGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &UDPSourceGUI::on_deltaFrequency_changed);
    QObject::connect(ui->sampleFormat, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &UDPSourceGUI::on_sampleFormat_currentIndexChanged);
    QObject::connect(ui->localUDPAddress, &QLineEdit::editingFinished, this, &UDPSourceGUI::on_localUDPAddress_editingFinished);
    QObject::connect(ui->localUDPPort, &QLineEdit::editingFinished, this, &UDPSourceGUI::on_localUDPPort_editingFinished);
    QObject::connect(ui->multicastAddress, &QLineEdit::editingFinished, this, &UDPSourceGUI::on_multicastAddress_editingFinished);
    QObject::connect(ui->multicastJoin, &ButtonSwitch::toggled, this, &UDPSourceGUI::on_multicastJoin_toggled);
    QObject::connect(ui->sampleRate, &QLineEdit::textEdited, this, &UDPSourceGUI::on_sampleRate_textEdited);
    QObject::connect(ui->rfBandwidth, &QLineEdit::textEdited, this, &UDPSourceGUI::on_rfBandwidth_textEdited);
    QObject::connect(ui->fmDeviation, &QLineEdit::textEdited, this, &UDPSourceGUI::on_fmDeviation_textEdited);
    QObject::connect(ui->amModPercent, &QLineEdit::textEdited, this, &UDPSourceGUI::on_amModPercent_textEdited);
    QObject::connect(ui->applyBtn, &QPushButton::clicked, this, &UDPSourceGUI::on_applyBtn_clicked);
    QObject::connect(ui->gainIn, &QDial::valueChanged, this, &UDPSourceGUI::on_gainIn_valueChanged);
    QObject::connect(ui->gainOut, &QDial::valueChanged, this, &UDPSourceGUI::on_gainOut_valueChanged);
    QObject::connect(ui->squelch, &QSlider::valueChanged, this, &UDPSourceGUI::on_squelch_valueChanged);
    QObject::connect(ui->squelchGate, &QDial::valueChanged, this, &UDPSourceGUI::on_squelchGate_valueChanged);
    QObject::connect(ui->channelMute, &QToolButton::toggled, this, &UDPSourceGUI::on_channelMute_toggled);
    QObject::connect(ui->resetUDPReadIndex, &QPushButton::clicked, this, &UDPSourceGUI::on_resetUDPReadIndex_clicked);
    QObject::connect(ui->autoRWBalance, &ButtonSwitch::toggled, this, &UDPSourceGUI::on_autoRWBalance_toggled);
    QObject::connect(ui->stereoInput, &QToolButton::toggled, this, &UDPSourceGUI::on_stereoInput_toggled);
}

void UDPSourceGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}
