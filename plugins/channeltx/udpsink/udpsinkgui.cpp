///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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
#include "dsp/spectrumvis.h"
#include "dsp/dspengine.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "gui/basicchannelsettingsdialog.h"
#include "plugin/pluginapi.h"
#include "mainwindow.h"

#include "udpsinkgui.h"
#include "ui_udpsinkgui.h"

UDPSinkGUI* UDPSinkGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx)
{
    UDPSinkGUI* gui = new UDPSinkGUI(pluginAPI, deviceUISet, channelTx);
    return gui;
}

void UDPSinkGUI::destroy()
{
    delete this;
}

void UDPSinkGUI::setName(const QString& name)
{
    setObjectName(name);
}

QString UDPSinkGUI::getName() const
{
    return objectName();
}

qint64 UDPSinkGUI::getCenterFrequency() const {
    return m_channelMarker.getCenterFrequency();
}

void UDPSinkGUI::setCenterFrequency(qint64 centerFrequency)
{
    m_channelMarker.setCenterFrequency(centerFrequency);
    applySettings();
}

void UDPSinkGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray UDPSinkGUI::serialize() const
{
    return m_settings.serialize();
}

bool UDPSinkGUI::deserialize(const QByteArray& data)
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

bool UDPSinkGUI::handleMessage(const Message& message __attribute__((unused)))
{
    qDebug() << "UDPSinkGUI::handleMessage";
    return false;
}

void UDPSinkGUI::handleSourceMessages()
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

UDPSinkGUI::UDPSinkGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent) :
        RollupWidget(parent),
        ui(new Ui::UDPSinkGUI),
        m_pluginAPI(pluginAPI),
        m_deviceUISet(deviceUISet),
        m_channelPowerAvg(4, 1e-10),
        m_inPowerAvg(4, 1e-10),
        m_tickCount(0),
        m_channelMarker(this),
        m_rfBandwidthChanged(false),
        m_doApplySettings(true)
{
    ui->setupUi(this);
    connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));
    setAttribute(Qt::WA_DeleteOnClose, true);

    m_spectrumVis = new SpectrumVis(SDR_TX_SCALEF, ui->glSpectrum);
    m_udpSink = (UDPSink*) channelTx; //new UDPSink(m_deviceUISet->m_deviceSinkAPI);
    m_udpSink->setSpectrumSink(m_spectrumVis);
    m_udpSink->setMessageQueueToGUI(getInputMessageQueue());

    ui->fmDeviation->setEnabled(false);
    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);

    ui->glSpectrum->setCenterFrequency(0);
    ui->glSpectrum->setSampleRate(ui->sampleRate->text().toInt());
    ui->glSpectrum->setDisplayWaterfall(true);
    ui->glSpectrum->setDisplayMaxHold(true);
    m_spectrumVis->configure(m_spectrumVis->getInputMessageQueue(), 64, 10, FFTWindow::BlackmanHarris);

    ui->glSpectrum->connectTimer(MainWindow::getInstance()->getMasterTimer());
    connect(&MainWindow::getInstance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    m_channelMarker.blockSignals(true);
    m_channelMarker.setBandwidth(16000);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setColor(m_settings.m_rgbColor);
    m_channelMarker.setTitle("UDP Sample Sink");
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    m_deviceUISet->registerTxChannelInstance(UDPSink::m_channelIdURI, this);
    m_deviceUISet->addChannelMarker(&m_channelMarker);
    m_deviceUISet->addRollupWidget(this);

    connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));

    ui->spectrumGUI->setBuddies(m_spectrumVis->getInputMessageQueue(), m_spectrumVis, ui->glSpectrum);

    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));
    connect(m_udpSink, SIGNAL(levelChanged(qreal, qreal, int)), ui->volumeMeter, SLOT(levelChanged(qreal, qreal, int)));

    displaySettings();
    applySettings(true);
}

UDPSinkGUI::~UDPSinkGUI()
{
    m_deviceUISet->removeTxChannelInstance(this);
    delete m_udpSink; // TODO: check this: when the GUI closes it has to delete the modulator
    delete m_spectrumVis;
    delete ui;
}

void UDPSinkGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void UDPSinkGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        UDPSink::MsgConfigureChannelizer *msgChan = UDPSink::MsgConfigureChannelizer::create(
                m_settings.m_inputSampleRate,
                m_settings.m_inputFrequencyOffset);
        m_udpSink->getInputMessageQueue()->push(msgChan);

        UDPSink::MsgConfigureUDPSink* message = UDPSink::MsgConfigureUDPSink::create( m_settings, force);
        m_udpSink->getInputMessageQueue()->push(message);

        ui->applyBtn->setEnabled(false);
        ui->applyBtn->setStyleSheet("QPushButton { background:rgb(79,79,79); }");
    }
}

void UDPSinkGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.setUDPAddress(m_settings.m_udpAddress);
    m_channelMarker.setUDPReceivePort(m_settings.m_udpPort); // activate signal on the last setting only
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor);

    setTitleColor(m_settings.m_rgbColor);
    this->setWindowTitle(m_channelMarker.getTitle());

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

    ui->addressText->setText(tr("%1:%2").arg(m_settings.m_udpAddress).arg(m_settings.m_udpPort));

    blockApplySettings(false);
}

void UDPSinkGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void UDPSinkGUI::on_deltaFrequency_changed(qint64 value)
{
    m_settings.m_inputFrequencyOffset = value;
    m_channelMarker.setCenterFrequency(value);
    applySettings();
}

void UDPSinkGUI::on_sampleFormat_currentIndexChanged(int index)
{
    if ((index == (int) UDPSinkSettings::FormatNFM)) {
        ui->fmDeviation->setEnabled(true);
    } else {
        ui->fmDeviation->setEnabled(false);
    }

    if (index == (int) UDPSinkSettings::FormatAM) {
        ui->amModPercent->setEnabled(true);
    } else {
        ui->amModPercent->setEnabled(false);
    }

    setSampleFormat(index);

    ui->applyBtn->setEnabled(true);
    ui->applyBtn->setStyleSheet("QPushButton { background-color : green; }");
}

void UDPSinkGUI::on_sampleRate_textEdited(const QString& arg1 __attribute__((unused)))
{
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

void UDPSinkGUI::on_rfBandwidth_textEdited(const QString& arg1 __attribute__((unused)))
{
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

void UDPSinkGUI::on_fmDeviation_textEdited(const QString& arg1 __attribute__((unused)))
{
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

void UDPSinkGUI::on_amModPercent_textEdited(const QString& arg1 __attribute__((unused)))
{
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

void UDPSinkGUI::on_gainIn_valueChanged(int value)
{
    m_settings.m_gainIn = value / 10.0;
    ui->gainInText->setText(tr("%1").arg(m_settings.m_gainIn, 0, 'f', 1));
    applySettings();
}

void UDPSinkGUI::on_gainOut_valueChanged(int value)
{
    m_settings.m_gainOut = value / 10.0;
    ui->gainOutText->setText(tr("%1").arg(m_settings.m_gainOut, 0, 'f', 1));
    applySettings();
}

void UDPSinkGUI::on_squelch_valueChanged(int value)
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

void UDPSinkGUI::on_squelchGate_valueChanged(int value)
{
    m_settings.m_squelchGate = value / 100.0;
    ui->squelchGateText->setText(tr("%1").arg(roundf(value * 10.0), 0, 'f', 0));
    applySettings();
}

void UDPSinkGUI::on_channelMute_toggled(bool checked)
{
    m_settings.m_channelMute = checked;
    applySettings();
}

void UDPSinkGUI::on_applyBtn_clicked()
{
    if (m_rfBandwidthChanged)
    {
        m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
        m_rfBandwidthChanged = false;
    }

    ui->glSpectrum->setSampleRate(m_settings.m_inputSampleRate);

    applySettings();
}

void UDPSinkGUI::on_resetUDPReadIndex_clicked()
{
    m_udpSink->resetReadIndex();
}

void UDPSinkGUI::on_autoRWBalance_toggled(bool checked)
{
    m_settings.m_autoRWBalance = checked;
    applySettings();
}

void UDPSinkGUI::on_stereoInput_toggled(bool checked)
{
    m_settings.m_stereoInput = checked;
    applySettings();
}

void UDPSinkGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    if ((widget == ui->spectrumBox) && (m_udpSink != 0))
    {
        m_udpSink->setSpectrum(rollDown);
    }
}

void UDPSinkGUI::onMenuDialogCalled(const QPoint &p)
{
    BasicChannelSettingsDialog dialog(&m_channelMarker, this);
    dialog.move(p);
    dialog.exec();

    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    m_settings.m_udpAddress = m_channelMarker.getUDPAddress(),
    m_settings.m_udpPort =  m_channelMarker.getUDPReceivePort(),
    m_settings.m_rgbColor = m_channelMarker.getColor().rgb();

    setWindowTitle(m_channelMarker.getTitle());
    setTitleColor(m_settings.m_rgbColor);
    ui->addressText->setText(tr("%1:%2").arg(m_settings.m_udpAddress).arg(m_settings.m_udpPort));

    applySettings();
}

void UDPSinkGUI::leaveEvent(QEvent*)
{
    m_channelMarker.setHighlighted(false);
}

void UDPSinkGUI::enterEvent(QEvent*)
{
    m_channelMarker.setHighlighted(true);
}

void UDPSinkGUI::tick()
{
    m_channelPowerAvg.feed(m_udpSink->getMagSq());
    m_inPowerAvg.feed(m_udpSink->getInMagSq());

    if (m_tickCount % 4 == 0)
    {
        double powDb = CalcDb::dbPower(m_channelPowerAvg.average());
        ui->channelPower->setText(tr("%1 dB").arg(powDb, 0, 'f', 1));
        double inPowDb = CalcDb::dbPower(m_inPowerAvg.average());
        ui->inputPower->setText(tr("%1").arg(inPowDb, 0, 'f', 1));
    }

    int32_t bufferGauge = m_udpSink->getBufferGauge();
    ui->bufferGaugeNegative->setValue((bufferGauge < 0 ? -bufferGauge : 0));
    ui->bufferGaugePositive->setValue((bufferGauge < 0 ? 0 : bufferGauge));
    QString s = QString::number(bufferGauge, 'f', 0);
    ui->bufferRWBalanceText->setText(tr("%1").arg(s));

    if (m_udpSink->getSquelchOpen()) {
        ui->channelMute->setStyleSheet("QToolButton { background-color : green; }");
    } else {
        ui->channelMute->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
    }

    m_tickCount++;
}

void UDPSinkGUI::setSampleFormatIndex(const UDPSinkSettings::SampleFormat& sampleFormat)
{
    switch(sampleFormat)
    {
        case UDPSinkSettings::FormatS16LE:
            ui->sampleFormat->setCurrentIndex(0);
            ui->fmDeviation->setEnabled(false);
            ui->stereoInput->setChecked(true);
            ui->stereoInput->setEnabled(false);
            break;
        case UDPSinkSettings::FormatNFM:
            ui->sampleFormat->setCurrentIndex(1);
            ui->fmDeviation->setEnabled(true);
            ui->stereoInput->setEnabled(true);
            break;
        case UDPSinkSettings::FormatLSB:
            ui->sampleFormat->setCurrentIndex(2);
            ui->fmDeviation->setEnabled(false);
            ui->stereoInput->setEnabled(true);
            break;
        case UDPSinkSettings::FormatUSB:
            ui->sampleFormat->setCurrentIndex(3);
            ui->fmDeviation->setEnabled(false);
            ui->stereoInput->setEnabled(true);
            break;
        case UDPSinkSettings::FormatAM:
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

void UDPSinkGUI::setSampleFormat(int index)
{
    switch(index)
    {
    case 0:
        m_settings.m_sampleFormat = UDPSinkSettings::FormatS16LE;
        ui->fmDeviation->setEnabled(false);
        ui->stereoInput->setChecked(true);
        ui->stereoInput->setEnabled(false);
        break;
    case 1:
        m_settings.m_sampleFormat = UDPSinkSettings::FormatNFM;
        ui->fmDeviation->setEnabled(true);
        ui->stereoInput->setEnabled(true);
        break;
    case 2:
        m_settings.m_sampleFormat = UDPSinkSettings::FormatLSB;
        ui->fmDeviation->setEnabled(false);
        ui->stereoInput->setEnabled(true);
        break;
    case 3:
        m_settings.m_sampleFormat = UDPSinkSettings::FormatUSB;
        ui->fmDeviation->setEnabled(false);
        ui->stereoInput->setEnabled(true);
        break;
    case 4:
        m_settings.m_sampleFormat = UDPSinkSettings::FormatAM;
        ui->fmDeviation->setEnabled(false);
        ui->stereoInput->setEnabled(true);
        break;
    default:
        m_settings.m_sampleFormat = UDPSinkSettings::FormatS16LE;
        ui->fmDeviation->setEnabled(false);
        ui->stereoInput->setChecked(true);
        ui->stereoInput->setEnabled(false);
        break;
    }
}

