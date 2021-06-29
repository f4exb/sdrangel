///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#include <QDockWidget>
#include <QMainWindow>
#include <QDateTime>
#include <QTime>
#include <QDebug>

#include "dsp/spectrumvis.h"
#include "dsp/scopevis.h"
#include "dsp/glscopesettings.h"
#include "device/deviceuiset.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "dsp/dspengine.h"
#include "gui/glspectrum.h"
#include "gui/crightclickenabler.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "maincore.h"

#include "ui_aismodgui.h"
#include "aismodgui.h"
#include "aismodrepeatdialog.h"
#include "aismodtxsettingsdialog.h"
#include "aismodsource.h"

AISModGUI* AISModGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx)
{
    AISModGUI* gui = new AISModGUI(pluginAPI, deviceUISet, channelTx);
    return gui;
}

void AISModGUI::destroy()
{
    delete this;
}

void AISModGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray AISModGUI::serialize() const
{
    return m_settings.serialize();
}

bool AISModGUI::deserialize(const QByteArray& data)
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

bool AISModGUI::handleMessage(const Message& message)
{
    if (AISMod::MsgConfigureAISMod::match(message))
    {
        const AISMod::MsgConfigureAISMod& cfg = (AISMod::MsgConfigureAISMod&) message;
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

void AISModGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void AISModGUI::handleSourceMessages()
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

void AISModGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void AISModGUI::on_mode_currentIndexChanged(int value)
{
    QString mode = ui->mode->currentText();

    // If m_doApplySettings is set, we are here from a call to displaySettings,
    // so we only want to display the current settings, not update them
    // as though a user had selected a new mode
    if (m_doApplySettings)
        m_settings.setMode(mode);

    ui->rfBWText->setText(QString("%1k").arg(m_settings.m_rfBandwidth / 1000.0, 0, 'f', 1));
    ui->rfBW->setValue(m_settings.m_rfBandwidth / 100.0);
    ui->fmDevText->setText(QString("%1k").arg(m_settings.m_fmDeviation / 1000.0, 0, 'f', 1));
    ui->fmDev->setValue(m_settings.m_fmDeviation / 100.0);
    ui->btText->setText(QString("%1").arg(m_settings.m_bt, 0, 'f', 1));
    ui->bt->setValue(m_settings.m_bt * 10);
    applySettings();

    // Remove custom mode when deselected, as we no longer know how to set it
    if (value < 2)
        ui->mode->removeItem(2);
}

void AISModGUI::on_rfBW_valueChanged(int value)
{
    float bw = value * 100.0f;
    ui->rfBWText->setText(QString("%1k").arg(value / 10.0, 0, 'f', 1));
    m_channelMarker.setBandwidth(bw);
    m_settings.m_rfBandwidth = bw;
    applySettings();
}

void AISModGUI::on_fmDev_valueChanged(int value)
{
    ui->fmDevText->setText(QString("%1k").arg(value / 10.0, 0, 'f', 1));
    m_settings.m_fmDeviation = value * 100.0;
    applySettings();
}

void AISModGUI::on_bt_valueChanged(int value)
{
    ui->btText->setText(QString("%1").arg(value / 10.0, 0, 'f', 1));
    m_settings.m_bt = value / 10.0;
    applySettings();
}

void AISModGUI::on_gain_valueChanged(int value)
{
    ui->gainText->setText(QString("%1dB").arg(value));
    m_settings.m_gain = value;
    applySettings();
}

void AISModGUI::on_channelMute_toggled(bool checked)
{
    m_settings.m_channelMute = checked;
    applySettings();
}

void AISModGUI::on_insertPosition_clicked()
{
    float latitude = MainCore::instance()->getSettings().getLatitude();
    float longitude = MainCore::instance()->getSettings().getLongitude();

    ui->latitude->setValue(latitude);
    ui->longitude->setValue(longitude);
}

void AISModGUI::on_txButton_clicked()
{
    transmit();
}

void AISModGUI::on_message_returnPressed()
{
    transmit();
}

void AISModGUI::on_msgId_currentIndexChanged(int index)
{
    m_settings.m_msgId = index + 1;
    applySettings();
}

void AISModGUI::on_mmsi_editingFinished()
{
    m_settings.m_mmsi = ui->mmsi->text();
    applySettings();
}

void AISModGUI::on_status_currentIndexChanged(int index)
{
    m_settings.m_status = index;
    applySettings();
}

void AISModGUI::on_latitude_valueChanged(double value)
{
    m_settings.m_latitude = (float)value;
    applySettings();
}

void AISModGUI::on_longitude_valueChanged(double value)
{
    m_settings.m_longitude = (float)value;
    applySettings();
}

void AISModGUI::on_course_valueChanged(double value)
{
    m_settings.m_course = (float)value;
    applySettings();
}

void AISModGUI::on_speed_valueChanged(double value)
{
    m_settings.m_speed = (float)value;
    applySettings();
}

void AISModGUI::on_heading_valueChanged(int value)
{
    m_settings.m_heading = value;
    applySettings();
}

void AISModGUI::on_message_editingFinished()
{
    m_settings.m_data = ui->message->text();
    applySettings();
}

// Convert decimal degrees to 1/10000 minutes
static int degToMinFracs(float decimal)
{
    return std::round(decimal * 60.0f * 10000.0f);
}

// Encode the message specified by the GUI controls in to a hex string and put in message field
void AISModGUI::on_encode_clicked()
{
    unsigned char bytes[168/8];
    int mmsi;
    int latitude;
    int longitude;

    mmsi = m_settings.m_mmsi.toInt();

    latitude = degToMinFracs(m_settings.m_latitude);
    longitude = degToMinFracs(m_settings.m_longitude);

    if (m_settings.m_msgId == 4)
    {
        // Base station report
        QDateTime currentDateTime = QDateTime::currentDateTimeUtc();
        QDate currentDate = currentDateTime.date();
        QTime currentTime = currentDateTime.time();

        int year = currentDate.year();
        int month = currentDate.month();
        int day = currentDate.day();
        int hour = currentTime.hour();
        int minute = currentTime.minute();
        int second = currentTime.second();

        bytes[0] = (m_settings.m_msgId << 2); // Repeat indicator = 0
        bytes[1] = (mmsi >> 22) & 0xff;
        bytes[2] = (mmsi >> 14) & 0xff;
        bytes[3] = (mmsi >> 6) & 0xff;
        bytes[4] = ((mmsi & 0x3f) << 2) | ((year >> 12) & 0x3);
        bytes[5] = (year >> 4) & 0xff;
        bytes[6] = ((year & 0xf) << 4) | month;
        bytes[7] = (day << 3) | ((hour >> 2) & 0x7);
        bytes[8] = ((hour & 0x3) << 6) | minute;
        bytes[9] = (second << 2) | (0 << 1) | ((longitude >> 27) & 1);
        bytes[10] = (longitude >> 19) & 0xff;
        bytes[11] = (longitude >> 11) & 0xff;
        bytes[12] = (longitude >> 3) & 0xff;
        bytes[13] = ((longitude & 0x7) << 5) | ((latitude >> 22) & 0x1f);
        bytes[14] = (latitude >> 14) & 0xff;
        bytes[15] = (latitude >> 6) & 0xff;
        bytes[16] = ((latitude & 0x3f) << 2);
        bytes[17] = 0;
        bytes[18] = 0;
        bytes[19] = 0;
        bytes[20] = 0;
    }
    else
    {
        // Position report
        int status;
        int rateOfTurn = 0x80; // Not available as not currently in GUI
        int speedOverGround;
        int courseOverGround;
        int timestamp;

        timestamp = QDateTime::currentDateTimeUtc().time().second();

        if (m_settings.m_speed >= 102.2)
            speedOverGround = 1022;
        else
            speedOverGround = std::round(m_settings.m_speed * 10.0);

        courseOverGround = std::floor(m_settings.m_course * 10.0);

        if (m_settings.m_status == 9) // Not defined (last in combo box)
            status = 15;
        else
            status = m_settings.m_status;

        bytes[0] = (m_settings.m_msgId << 2); // Repeat indicator = 0

        bytes[1] = (mmsi >> 22) & 0xff;
        bytes[2] = (mmsi >> 14) & 0xff;
        bytes[3] = (mmsi >> 6) & 0xff;
        bytes[4] = ((mmsi & 0x3f) << 2) | (status >> 2);

        bytes[5] = ((status & 0x3) << 6) | ((rateOfTurn >> 2) & 0x3f);
        bytes[6] = ((rateOfTurn & 0x3) << 6) | ((speedOverGround >> 4) & 0x3f);
        bytes[7] = ((speedOverGround & 0xf) << 4) | (0 << 3) | ((longitude >> 25) & 0x7); // Position accuracy = 0
        bytes[8] = (longitude >> 17) & 0xff;
        bytes[9] = (longitude >> 9) & 0xff;
        bytes[10] = (longitude >> 1) & 0xff;
        bytes[11] = ((longitude & 0x1) << 7) | ((latitude >> 20) & 0x7f);
        bytes[12] = (latitude >> 12) & 0xff;
        bytes[13] = (latitude >> 4) & 0xff;
        bytes[14] = ((latitude & 0xf) << 4) | ((courseOverGround >> 8) & 0xf);
        bytes[15] = courseOverGround & 0xff;
        bytes[16] = ((m_settings.m_heading >> 1) & 0xff);
        bytes[17] = ((m_settings.m_heading & 0x1) << 7) | ((timestamp & 0x3f) << 1);
        bytes[18] = 0;
        bytes[19] = 0;
        bytes[20] = 0;
    }

    QByteArray ba((const char *)bytes, sizeof(bytes));
    ui->message->setText(ba.toHex());

    m_settings.m_data = ui->message->text();
    applySettings();
}

void AISModGUI::on_repeat_toggled(bool checked)
{
    m_settings.m_repeat = checked;
    applySettings();
}

void AISModGUI::repeatSelect()
{
    AISModRepeatDialog dialog(m_settings.m_repeatDelay, m_settings.m_repeatCount);
    if (dialog.exec() == QDialog::Accepted)
    {
        m_settings.m_repeatDelay = dialog.m_repeatDelay;
        m_settings.m_repeatCount = dialog.m_repeatCount;
        applySettings();
    }
}

void AISModGUI::txSettingsSelect()
{
    AISModTXSettingsDialog dialog(m_settings.m_rampUpBits, m_settings.m_rampDownBits,
                                        m_settings.m_rampRange,
                                        m_settings.m_baud,
                                        m_settings.m_symbolSpan,
                                        m_settings.m_rfNoise,
                                        m_settings.m_writeToFile);
    if (dialog.exec() == QDialog::Accepted)
    {
        m_settings.m_rampUpBits = dialog.m_rampUpBits;
        m_settings.m_rampDownBits = dialog.m_rampDownBits;
        m_settings.m_rampRange = dialog.m_rampRange;
        m_settings.m_baud = dialog.m_baud;
        m_settings.m_symbolSpan = dialog.m_symbolSpan;
        m_settings.m_rfNoise = dialog.m_rfNoise;
        m_settings.m_writeToFile = dialog.m_writeToFile;
        displaySettings();
        applySettings();
    }
}

void AISModGUI::on_udpEnabled_clicked(bool checked)
{
    m_settings.m_udpEnabled = checked;
    applySettings();
}

void AISModGUI::on_udpAddress_editingFinished()
{
    m_settings.m_udpAddress = ui->udpAddress->text();
    applySettings();
}

void AISModGUI::on_udpPort_editingFinished()
{
    m_settings.m_udpPort = ui->udpPort->text().toInt();
    applySettings();
}

void AISModGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;
}

void AISModGUI::onMenuDialogCalled(const QPoint &p)
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

        m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
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
        dialog.setNumberOfStreams(m_aisMod->getNumberOfDeviceStreams());
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

AISModGUI::AISModGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent) :
    ChannelGUI(parent),
    ui(new Ui::AISModGUI),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_channelMarker(this),
    m_doApplySettings(true)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose, true);

    connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_aisMod = (AISMod*) channelTx;
    m_aisMod->setMessageQueueToGUI(getInputMessageQueue());

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    m_scopeVis = m_aisMod->getScopeSink();
    m_scopeVis->setGLScope(ui->glScope);
    ui->glScope->connectTimer(MainCore::instance()->getMasterTimer());
    ui->scopeGUI->setBuddies(m_scopeVis->getInputMessageQueue(), m_scopeVis, ui->glScope);

    // Scope settings to display the IQ waveforms
    ui->scopeGUI->setPreTrigger(1);
    GLScopeSettings::TraceData traceDataI, traceDataQ;
    traceDataI.m_projectionType = Projector::ProjectionReal;
    traceDataI.m_amp = 1.0;      // for -1 to +1
    traceDataI.m_ofs = 0.0;      // vertical offset
    traceDataQ.m_projectionType = Projector::ProjectionImag;
    traceDataQ.m_amp = 1.0;
    traceDataQ.m_ofs = 0.0;
    ui->scopeGUI->changeTrace(0, traceDataI);
    ui->scopeGUI->addTrace(traceDataQ);
    ui->scopeGUI->setDisplayMode(GLScopeSettings::DisplayPol);
    ui->scopeGUI->focusOnTrace(0); // re-focus to take changes into account in the GUI

    GLScopeSettings::TriggerData triggerData;
    triggerData.m_triggerLevel = 0.1;
    triggerData.m_triggerLevelCoarse = 10;
    triggerData.m_triggerPositiveEdge = true;
    ui->scopeGUI->changeTrigger(0, triggerData);
    ui->scopeGUI->focusOnTrigger(0); // re-focus to take changes into account in the GUI

    m_scopeVis->setLiveRate(AISModSettings::AISMOD_SAMPLE_RATE);
    //m_scopeVis->setFreeRun(false); // FIXME: add method rather than call m_scopeVis->configure()

    m_spectrumVis = m_aisMod->getSpectrumVis();
    m_spectrumVis->setGLSpectrum(ui->glSpectrum);

    // Extra /2 here because SSB?
    ui->glSpectrum->setCenterFrequency(0);
    ui->glSpectrum->setSampleRate(AISModSettings::AISMOD_SAMPLE_RATE);
    ui->glSpectrum->setSsbSpectrum(true);
    ui->glSpectrum->setDisplayCurrent(true);
    ui->glSpectrum->setLsbDisplay(false);
    ui->glSpectrum->setDisplayWaterfall(false);
    ui->glSpectrum->setDisplayMaxHold(false);
    ui->glSpectrum->setDisplayHistogram(false);

    CRightClickEnabler *repeatRightClickEnabler = new CRightClickEnabler(ui->repeat);
    connect(repeatRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(repeatSelect()));

    CRightClickEnabler *txRightClickEnabler = new CRightClickEnabler(ui->txButton);
    connect(txRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(txSettingsSelect()));

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(Qt::red);
    m_channelMarker.setBandwidth(12500);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("AIS Modulator");
    m_channelMarker.setSourceOrSinkStream(false);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    m_deviceUISet->addChannelMarker(&m_channelMarker);
    m_deviceUISet->addRollupWidget(this);

    connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));

    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));
    m_aisMod->setLevelMeter(ui->volumeMeter);

    m_settings.setChannelMarker(&m_channelMarker);

    ui->spectrumGUI->setBuddies(m_spectrumVis, ui->glSpectrum);

    ui->scopeContainer->setVisible(false);
    ui->spectrumContainer->setVisible(false);

    displaySettings();
    applySettings();
}

AISModGUI::~AISModGUI()
{
    delete ui;
}

void AISModGUI::transmit()
{
    QString data = ui->message->text();
    ui->transmittedText->appendPlainText(data + "\n");
    AISMod::MsgTXAISMod *msg = AISMod::MsgTXAISMod::create(data);
    m_aisMod->getInputMessageQueue()->push(msg);
}

void AISModGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void AISModGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        AISMod::MsgConfigureAISMod *msg = AISMod::MsgConfigureAISMod::create(m_settings, force);
        m_aisMod->getInputMessageQueue()->push(msg);
    }
}

void AISModGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    displayStreamIndex();

    blockApplySettings(true);

    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    if ((m_settings.m_rfBandwidth == 12500.0f) && (m_settings.m_bt == 0.3f))
        ui->mode->setCurrentIndex(0);
    else if ((m_settings.m_rfBandwidth == 25000.0f) && (m_settings.m_bt == 0.4f))
        ui->mode->setCurrentIndex(1);
    else
    {
        ui->mode->removeItem(2);
        ui->mode->addItem(m_settings.getMode());
        ui->mode->setCurrentIndex(2);
    }

    ui->rfBWText->setText(QString("%1k").arg(m_settings.m_rfBandwidth / 1000.0, 0, 'f', 1));
    ui->rfBW->setValue(m_settings.m_rfBandwidth / 100.0);

    ui->fmDevText->setText(QString("%1k").arg(m_settings.m_fmDeviation / 1000.0, 0, 'f', 1));
    ui->fmDev->setValue(m_settings.m_fmDeviation / 100.0);

    ui->btText->setText(QString("%1").arg(m_settings.m_bt, 0, 'f', 1));
    ui->bt->setValue(m_settings.m_bt * 10);

    ui->gainText->setText(QString("%1").arg((double)m_settings.m_gain, 0, 'f', 1));
    ui->gain->setValue(m_settings.m_gain);

    ui->udpEnabled->setChecked(m_settings.m_udpEnabled);
    ui->udpAddress->setText(m_settings.m_udpAddress);
    ui->udpPort->setText(QString::number(m_settings.m_udpPort));

    ui->channelMute->setChecked(m_settings.m_channelMute);
    ui->repeat->setChecked(m_settings.m_repeat);

    ui->msgId->setCurrentIndex(m_settings.m_msgId - 1);
    ui->mmsi->setText(m_settings.m_mmsi);
    ui->status->setCurrentIndex(m_settings.m_status);
    ui->latitude->setValue(m_settings.m_latitude);
    ui->longitude->setValue(m_settings.m_longitude);
    ui->course->setValue(m_settings.m_course);
    ui->speed->setValue(m_settings.m_speed);
    ui->heading->setValue(m_settings.m_heading);
    ui->message->setText(m_settings.m_data);

    blockApplySettings(false);
}

void AISModGUI::displayStreamIndex()
{
    if (m_deviceUISet->m_deviceMIMOEngine) {
        setStreamIndicator(tr("%1").arg(m_settings.m_streamIndex));
    } else {
        setStreamIndicator("S"); // single channel indicator
    }
}

void AISModGUI::leaveEvent(QEvent*)
{
    m_channelMarker.setHighlighted(false);
}

void AISModGUI::enterEvent(QEvent*)
{
    m_channelMarker.setHighlighted(true);
}

void AISModGUI::tick()
{
    double powDb = CalcDb::dbPower(m_aisMod->getMagSq());
    m_channelPowerDbAvg(powDb);
    ui->channelPower->setText(tr("%1 dB").arg(m_channelPowerDbAvg.asDouble(), 0, 'f', 1));
}
