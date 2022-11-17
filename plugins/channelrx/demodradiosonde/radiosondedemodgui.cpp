///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#include <QDebug>
#include <QDesktopServices>
#include <QMessageBox>
#include <QAction>
#include <QRegExp>
#include <QClipboard>
#include <QFileDialog>
#include <QScrollBar>

#include "radiosondedemodgui.h"

#include "device/deviceuiset.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "ui_radiosondedemodgui.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/csv.h"
#include "util/db.h"
#include "util/units.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "gui/datetimedelegate.h"
#include "gui/decimaldelegate.h"
#include "gui/timedelegate.h"
#include "dsp/dspengine.h"
#include "dsp/glscopesettings.h"
#include "gui/crightclickenabler.h"
#include "channel/channelwebapiutils.h"
#include "maincore.h"
#include "feature/featurewebapiutils.h"

#include "radiosondedemod.h"
#include "radiosondedemodsink.h"

void RadiosondeDemodGUI::resizeTable()
{
    // Fill table with a row of dummy data that will size the columns nicely
    // Trailing chars are for sort arrow
    int row = ui->frames->rowCount();
    ui->frames->setRowCount(row + 1);
    ui->frames->setItem(row, FRAME_COL_DATE, new QTableWidgetItem("2015/04/15-"));
    ui->frames->setItem(row, FRAME_COL_TIME, new QTableWidgetItem("10:17:00"));
    ui->frames->setItem(row, FRAME_COL_SERIAL, new QTableWidgetItem("S1234567"));
    ui->frames->setItem(row, FRAME_COL_FRAME_NUMBER, new QTableWidgetItem("10000"));
    ui->frames->setItem(row, FRAME_COL_FLIGHT_PHASE, new QTableWidgetItem("Descent"));
    ui->frames->setItem(row, FRAME_COL_LATITUDE, new QTableWidgetItem("-90.00000"));
    ui->frames->setItem(row, FRAME_COL_LONGITUDE, new QTableWidgetItem("-180.00000"));
    ui->frames->setItem(row, FRAME_COL_ALTITUDE, new QTableWidgetItem("20000.0"));
    ui->frames->setItem(row, FRAME_COL_SPEED, new QTableWidgetItem("50.0"));
    ui->frames->setItem(row, FRAME_COL_VERTICAL_RATE, new QTableWidgetItem("50.0"));
    ui->frames->setItem(row, FRAME_COL_HEADING, new QTableWidgetItem("359.0"));
    ui->frames->setItem(row, FRAME_COL_PRESSURE, new QTableWidgetItem("100.0"));
    ui->frames->setItem(row, FRAME_COL_TEMP, new QTableWidgetItem("-50.1U"));
    ui->frames->setItem(row, FRAME_COL_HUMIDITY, new QTableWidgetItem("100.0"));
    ui->frames->setItem(row, FRAME_COL_BATTERY_VOLTAGE, new QTableWidgetItem("2.7"));
    ui->frames->setItem(row, FRAME_COL_BATTERY_STATUS, new QTableWidgetItem("Low"));
    ui->frames->setItem(row, FRAME_COL_PCB_TEMP, new QTableWidgetItem("21"));
    ui->frames->setItem(row, FRAME_COL_HUMIDITY_PWM, new QTableWidgetItem("1000"));
    ui->frames->setItem(row, FRAME_COL_TX_POWER, new QTableWidgetItem("7"));
    ui->frames->setItem(row, FRAME_COL_MAX_SUBFRAME_NO, new QTableWidgetItem("50"));
    ui->frames->setItem(row, FRAME_COL_SUBFRAME_NO, new QTableWidgetItem("50"));
    ui->frames->setItem(row, FRAME_COL_SUBFRAME, new QTableWidgetItem("00112233445566778899aabbccddeeff----"));
    ui->frames->setItem(row, FRAME_COL_GPS_TIME, new QTableWidgetItem("2015/04/15 10:17:00"));
    ui->frames->setItem(row, FRAME_COL_GPS_SATS, new QTableWidgetItem("12"));
    ui->frames->setItem(row, FRAME_COL_ECC, new QTableWidgetItem("12"));
    ui->frames->setItem(row, FRAME_COL_CORR, new QTableWidgetItem("-500"));
    ui->frames->resizeColumnsToContents();
    ui->frames->removeRow(row);
}

// Columns in table reordered
void RadiosondeDemodGUI::frames_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex)
{
    (void) oldVisualIndex;

    m_settings.m_frameColumnIndexes[logicalIndex] = newVisualIndex;
}

// Column in table resized (when hidden size is 0)
void RadiosondeDemodGUI::frames_sectionResized(int logicalIndex, int oldSize, int newSize)
{
    (void) oldSize;

    m_settings.m_frameColumnSizes[logicalIndex] = newSize;
}

// Right click in table header - show column select menu
void RadiosondeDemodGUI::framesColumnSelectMenu(QPoint pos)
{
    framesMenu->popup(ui->frames->horizontalHeader()->viewport()->mapToGlobal(pos));
}

// Hide/show column when menu selected
void RadiosondeDemodGUI::framesColumnSelectMenuChecked(bool checked)
{
    (void) checked;

    QAction* action = qobject_cast<QAction*>(sender());
    if (action != nullptr)
    {
        int idx = action->data().toInt(nullptr);
        ui->frames->setColumnHidden(idx, !action->isChecked());
    }
}

// Create column select menu item
QAction *RadiosondeDemodGUI::createCheckableItem(QString &text, int idx, bool checked, const char *slot)
{
    QAction *action = new QAction(text, this);
    action->setCheckable(true);
    action->setChecked(checked);
    action->setData(QVariant(idx));
    connect(action, SIGNAL(triggered()), this, slot);
    return action;
}

RadiosondeDemodGUI* RadiosondeDemodGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
    RadiosondeDemodGUI* gui = new RadiosondeDemodGUI(pluginAPI, deviceUISet, rxChannel);
    return gui;
}

void RadiosondeDemodGUI::destroy()
{
    delete this;
}

void RadiosondeDemodGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray RadiosondeDemodGUI::serialize() const
{
    return m_settings.serialize();
}

bool RadiosondeDemodGUI::deserialize(const QByteArray& data)
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

// Add row to table
void RadiosondeDemodGUI::frameReceived(const QByteArray& frame, const QDateTime& dateTime, int errorsCorrected, int threshold)
{
    RS41Frame *radiosonde;

    // Decode the frame
    radiosonde = RS41Frame::decode(frame);

    // Is scroll bar at bottom
    QScrollBar *sb = ui->frames->verticalScrollBar();
    bool scrollToBottom = sb->value() == sb->maximum();

    // Add to frames table
    ui->frames->setSortingEnabled(false);
    int row = ui->frames->rowCount();
    ui->frames->setRowCount(row + 1);

    QTableWidgetItem *dateItem = new QTableWidgetItem();
    QTableWidgetItem *timeItem = new QTableWidgetItem();
    QTableWidgetItem *serialItem = new QTableWidgetItem();
    QTableWidgetItem *frameNumberItem = new QTableWidgetItem();
    QTableWidgetItem *flightPhaseItem = new QTableWidgetItem();
    QTableWidgetItem *latitudeItem = new QTableWidgetItem();
    QTableWidgetItem *longitudeItem = new QTableWidgetItem();
    QTableWidgetItem *altitudeItem = new QTableWidgetItem();
    QTableWidgetItem *speedItem = new QTableWidgetItem();
    QTableWidgetItem *verticalRateItem = new QTableWidgetItem();
    QTableWidgetItem *headingItem = new QTableWidgetItem();
    QTableWidgetItem *pressureItem = new QTableWidgetItem();
    QTableWidgetItem *tempItem = new QTableWidgetItem();
    QTableWidgetItem *humidityItem = new QTableWidgetItem();
    QTableWidgetItem *batteryVoltageItem = new QTableWidgetItem();
    QTableWidgetItem *batteryStatusItem = new QTableWidgetItem();
    QTableWidgetItem *pcbTempItem = new QTableWidgetItem();
    QTableWidgetItem *humidityPWMItem = new QTableWidgetItem();
    QTableWidgetItem *txPowerItem = new QTableWidgetItem();
    QTableWidgetItem *maxSubframeNoItem = new QTableWidgetItem();
    QTableWidgetItem *subframeNoItem = new QTableWidgetItem();
    QTableWidgetItem *subframeItem = new QTableWidgetItem();
    QTableWidgetItem *gpsTimeItem = new QTableWidgetItem();
    QTableWidgetItem *gpsSatsItem = new QTableWidgetItem();
    QTableWidgetItem *eccItem = new QTableWidgetItem();
    QTableWidgetItem *thItem = new QTableWidgetItem();

    ui->frames->setItem(row, FRAME_COL_DATE, dateItem);
    ui->frames->setItem(row, FRAME_COL_TIME, timeItem);
    ui->frames->setItem(row, FRAME_COL_SERIAL, serialItem);
    ui->frames->setItem(row, FRAME_COL_FRAME_NUMBER, frameNumberItem);
    ui->frames->setItem(row, FRAME_COL_FLIGHT_PHASE, flightPhaseItem);
    ui->frames->setItem(row, FRAME_COL_LATITUDE, latitudeItem);
    ui->frames->setItem(row, FRAME_COL_LONGITUDE, longitudeItem);
    ui->frames->setItem(row, FRAME_COL_ALTITUDE, altitudeItem);
    ui->frames->setItem(row, FRAME_COL_SPEED, speedItem);
    ui->frames->setItem(row, FRAME_COL_VERTICAL_RATE, verticalRateItem);
    ui->frames->setItem(row, FRAME_COL_HEADING, headingItem);
    ui->frames->setItem(row, FRAME_COL_PRESSURE, pressureItem);
    ui->frames->setItem(row, FRAME_COL_TEMP, tempItem);
    ui->frames->setItem(row, FRAME_COL_HUMIDITY, humidityItem);
    ui->frames->setItem(row, FRAME_COL_BATTERY_VOLTAGE, batteryVoltageItem);
    ui->frames->setItem(row, FRAME_COL_BATTERY_STATUS, batteryStatusItem);
    ui->frames->setItem(row, FRAME_COL_PCB_TEMP, pcbTempItem);
    ui->frames->setItem(row, FRAME_COL_HUMIDITY_PWM, humidityPWMItem);
    ui->frames->setItem(row, FRAME_COL_TX_POWER, txPowerItem);
    ui->frames->setItem(row, FRAME_COL_MAX_SUBFRAME_NO, maxSubframeNoItem);
    ui->frames->setItem(row, FRAME_COL_SUBFRAME_NO, subframeNoItem);
    ui->frames->setItem(row, FRAME_COL_SUBFRAME, subframeItem);
    ui->frames->setItem(row, FRAME_COL_GPS_TIME, gpsTimeItem);
    ui->frames->setItem(row, FRAME_COL_GPS_SATS, gpsSatsItem);
    ui->frames->setItem(row, FRAME_COL_ECC, eccItem);
    ui->frames->setItem(row, FRAME_COL_CORR, thItem);

    dateItem->setData(Qt::DisplayRole, dateTime.date());
    timeItem->setData(Qt::DisplayRole, dateTime.time());

    RS41Subframe *subframe = nullptr;

    frameNumberItem->setData(Qt::DisplayRole, radiosonde->m_frameNumber);
    if (radiosonde->m_statusValid)
    {
        serialItem->setText(radiosonde->m_serial);
        flightPhaseItem->setText(radiosonde->m_flightPhase);
        batteryVoltageItem->setData(Qt::DisplayRole, radiosonde->m_batteryVoltage);
        batteryStatusItem->setText(radiosonde->m_batteryStatus);
        pcbTempItem->setData(Qt::DisplayRole, radiosonde->m_pcbTemperature);
        humidityPWMItem->setData(Qt::DisplayRole, (int)round(radiosonde->m_humiditySensorHeating / 1000.0 * 100.0));
        txPowerItem->setData(Qt::DisplayRole, (int)round(radiosonde->m_transmitPower / 7.0 * 100.0));
        maxSubframeNoItem->setData(Qt::DisplayRole, radiosonde->m_maxSubframeNumber);
        subframeNoItem->setData(Qt::DisplayRole, radiosonde->m_subframeNumber);
        subframeItem->setText(radiosonde->m_subframe.toHex());
        if (m_subframes.contains(radiosonde->m_serial))
        {
            subframe = m_subframes.value(radiosonde->m_serial);
        }
        else
        {
            subframe = new RS41Subframe();
            m_subframes.insert(radiosonde->m_serial, subframe);
        }
        subframe->update(radiosonde);
    }

    if (radiosonde->m_posValid)
    {
        latitudeItem->setData(Qt::DisplayRole, radiosonde->m_latitude);
        longitudeItem->setData(Qt::DisplayRole, radiosonde->m_longitude);
        altitudeItem->setData(Qt::DisplayRole, radiosonde->m_height);
        speedItem->setData(Qt::DisplayRole, Units::kmpsToKPH(radiosonde->m_speed/1000.0));
        verticalRateItem->setData(Qt::DisplayRole, radiosonde->m_verticalRate);
        headingItem->setData(Qt::DisplayRole, radiosonde->m_heading);
        gpsSatsItem->setData(Qt::DisplayRole, radiosonde->m_satellitesUsed);
    }

    if (radiosonde->m_gpsInfoValid)
    {
        gpsTimeItem->setData(Qt::DisplayRole, radiosonde->m_gpsDateTime);
    }

    if (radiosonde->m_measValid && subframe)
    {
        pressureItem->setData(Qt::DisplayRole, radiosonde->getPressureString(subframe));
        tempItem->setData(Qt::DisplayRole, radiosonde->getTemperatureString(subframe));
        humidityItem->setData(Qt::DisplayRole, radiosonde->getHumidityString(subframe));
    }

    eccItem->setData(Qt::DisplayRole, errorsCorrected);
    thItem->setData(Qt::DisplayRole, threshold);

    ui->frames->setSortingEnabled(true);
    if (scrollToBottom) {
        ui->frames->scrollToBottom();
    }
    filterRow(row);

    delete radiosonde;
}

bool RadiosondeDemodGUI::handleMessage(const Message& frame)
{
    if (RadiosondeDemod::MsgConfigureRadiosondeDemod::match(frame))
    {
        qDebug("RadiosondeDemodGUI::handleMessage: RadiosondeDemod::MsgConfigureRadiosondeDemod");
        const RadiosondeDemod::MsgConfigureRadiosondeDemod& cfg = (RadiosondeDemod::MsgConfigureRadiosondeDemod&) frame;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        ui->scopeGUI->updateSettings();
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (RadiosondeDemod::MsgMessage::match(frame))
    {
        RadiosondeDemod::MsgMessage& report = (RadiosondeDemod::MsgMessage&) frame;
        frameReceived(report.getMessage(), report.getDateTime(), report.getErrorsCorrected(), report.getThreshold());
        return true;
    }
    else if (DSPSignalNotification::match(frame))
    {
        const DSPSignalNotification& notif = (const DSPSignalNotification&) frame;
        m_deviceCenterFrequency = notif.getCenterFrequency();
        m_basebandSampleRate = notif.getSampleRate();
        ui->deltaFrequency->setValueRange(false, 7, -m_basebandSampleRate/2, m_basebandSampleRate/2);
        ui->deltaFrequencyLabel->setToolTip(tr("Range %1 %L2 Hz").arg(QChar(0xB1)).arg(m_basebandSampleRate/2));
        updateAbsoluteCenterFrequency();
        return true;
    }

    return false;
}

void RadiosondeDemodGUI::handleInputMessages()
{
    Message* frame;

    while ((frame = getInputMessageQueue()->pop()) != 0)
    {
        if (handleMessage(*frame))
        {
            delete frame;
        }
    }
}

void RadiosondeDemodGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void RadiosondeDemodGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void RadiosondeDemodGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings();
}

void RadiosondeDemodGUI::on_rfBW_valueChanged(int value)
{
    float bw = value * 100.0f;
    ui->rfBWText->setText(QString("%1k").arg(value / 10.0, 0, 'f', 1));
    m_channelMarker.setBandwidth(bw);
    m_settings.m_rfBandwidth = bw;
    applySettings();
}

void RadiosondeDemodGUI::on_fmDev_valueChanged(int value)
{
    ui->fmDevText->setText(QString("%1k").arg(value / 10.0, 0, 'f', 1));
    m_settings.m_fmDeviation = value * 100.0;
    applySettings();
}

void RadiosondeDemodGUI::on_threshold_valueChanged(int value)
{
    ui->thresholdText->setText(QString("%1").arg(value));
    m_settings.m_correlationThreshold = value;
    applySettings();
}

void RadiosondeDemodGUI::on_filterSerial_editingFinished()
{
    m_settings.m_filterSerial = ui->filterSerial->text();
    filter();
    applySettings();
}

void RadiosondeDemodGUI::on_clearTable_clicked()
{
    ui->frames->setRowCount(0);
}

void RadiosondeDemodGUI::on_udpEnabled_clicked(bool checked)
{
    m_settings.m_udpEnabled = checked;
    applySettings();
}

void RadiosondeDemodGUI::on_udpAddress_editingFinished()
{
    m_settings.m_udpAddress = ui->udpAddress->text();
    applySettings();
}

void RadiosondeDemodGUI::on_udpPort_editingFinished()
{
    m_settings.m_udpPort = ui->udpPort->text().toInt();
    applySettings();
}

void RadiosondeDemodGUI::on_channel1_currentIndexChanged(int index)
{
    m_settings.m_scopeCh1 = index;
    applySettings();
}

void RadiosondeDemodGUI::on_channel2_currentIndexChanged(int index)
{
    m_settings.m_scopeCh2 = index;
    applySettings();
}

void RadiosondeDemodGUI::on_frames_cellDoubleClicked(int row, int column)
{
    // Get serial in row double clicked
    QString serial = ui->frames->item(row, FRAME_COL_SERIAL)->text();
    if (column == FRAME_COL_SERIAL)
    {
        // Search for Serial on sondehub
        QDesktopServices::openUrl(QUrl(QString("https://sondehub.org/?f=%1#!mt=Mapnik&f=%1&q=%1").arg(serial)));
    }
    else if ((column == FRAME_COL_LATITUDE) || (column == FRAME_COL_LONGITUDE))
    {
        // Find serial on Map
        FeatureWebAPIUtils::mapFind(serial);
    }
}

void RadiosondeDemodGUI::filterRow(int row)
{
    bool hidden = false;
    if (m_settings.m_filterSerial != "")
    {
        QRegExp re(m_settings.m_filterSerial);
        QTableWidgetItem *fromItem = ui->frames->item(row, FRAME_COL_SERIAL);
        if (!re.exactMatch(fromItem->text()))
            hidden = true;
    }
    ui->frames->setRowHidden(row, hidden);
}

void RadiosondeDemodGUI::filter()
{
    for (int i = 0; i < ui->frames->rowCount(); i++)
    {
        filterRow(i);
    }
}

void RadiosondeDemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

void RadiosondeDemodGUI::onMenuDialogCalled(const QPoint &p)
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
            dialog.setNumberOfStreams(m_radiosondeDemod->getNumberOfDeviceStreams());
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

RadiosondeDemodGUI::RadiosondeDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
    ChannelGUI(parent),
    ui(new Ui::RadiosondeDemodGUI),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_basebandSampleRate(1),
    m_doApplySettings(true),
    m_tickCount(0)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/demodradiosonde/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_radiosondeDemod = reinterpret_cast<RadiosondeDemod*>(rxChannel);
    m_radiosondeDemod->setMessageQueueToGUI(getInputMessageQueue());

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick())); // 50 ms

    m_scopeVis = m_radiosondeDemod->getScopeSink();
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
    ui->scopeGUI->setDisplayMode(GLScopeSettings::DisplayXYV);
    ui->scopeGUI->focusOnTrace(0); // re-focus to take changes into account in the GUI

    GLScopeSettings::TriggerData triggerData;
    triggerData.m_triggerLevel = 0.1;
    triggerData.m_triggerLevelCoarse = 10;
    triggerData.m_triggerPositiveEdge = true;
    ui->scopeGUI->changeTrigger(0, triggerData);
    ui->scopeGUI->focusOnTrigger(0); // re-focus to take changes into account in the GUI

    m_scopeVis->setLiveRate(9600*6);
    //m_scopeVis->setFreeRun(false); // FIXME: add method rather than call m_scopeVis->configure()

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);
    ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(Qt::yellow);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle("Radiosonde Demodulator");
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    setTitleColor(m_channelMarker.getColor());
    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setScopeGUI(ui->scopeGUI);
    m_settings.setRollupState(&m_rollupState);

    m_deviceUISet->addChannelMarker(&m_channelMarker);

    connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()), this, SLOT(channelMarkerHighlightedByCursor()));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    // Resize the table using dummy data
    resizeTable();
    // Allow user to reorder columns
    ui->frames->horizontalHeader()->setSectionsMovable(true);
    // Allow user to sort table by clicking on headers
    ui->frames->setSortingEnabled(true);
    // Add context menu to allow hiding/showing of columns
    framesMenu = new QMenu(ui->frames);
    for (int i = 0; i < ui->frames->horizontalHeader()->count(); i++)
    {
        QString text = ui->frames->horizontalHeaderItem(i)->text();
        framesMenu->addAction(createCheckableItem(text, i, true, SLOT(framesColumnSelectMenuChecked())));
    }
    ui->frames->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->frames->horizontalHeader(), SIGNAL(customContextMenuRequested(QPoint)), SLOT(framesColumnSelectMenu(QPoint)));
    // Get signals when columns change
    connect(ui->frames->horizontalHeader(), SIGNAL(sectionMoved(int, int, int)), SLOT(frames_sectionMoved(int, int, int)));
    connect(ui->frames->horizontalHeader(), SIGNAL(sectionResized(int, int, int)), SLOT(frames_sectionResized(int, int, int)));
    ui->frames->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->frames, SIGNAL(customContextMenuRequested(QPoint)), SLOT(customContextMenuRequested(QPoint)));

    ui->frames->setItemDelegateForColumn(FRAME_COL_DATE, new DateTimeDelegate("yyyy/MM/dd"));
    ui->frames->setItemDelegateForColumn(FRAME_COL_TIME, new TimeDelegate());
    ui->frames->setItemDelegateForColumn(FRAME_COL_LATITUDE, new DecimalDelegate(5));
    ui->frames->setItemDelegateForColumn(FRAME_COL_LONGITUDE, new DecimalDelegate(5));
    ui->frames->setItemDelegateForColumn(FRAME_COL_ALTITUDE, new DecimalDelegate(1));
    ui->frames->setItemDelegateForColumn(FRAME_COL_SPEED, new DecimalDelegate(1));
    ui->frames->setItemDelegateForColumn(FRAME_COL_VERTICAL_RATE, new DecimalDelegate(1));
    ui->frames->setItemDelegateForColumn(FRAME_COL_HEADING, new DecimalDelegate(1));
    ui->frames->setItemDelegateForColumn(FRAME_COL_GPS_TIME, new DateTimeDelegate("yyyy/MM/dd hh:mm:ss"));

    ui->scopeContainer->setVisible(false);

    displaySettings();
    makeUIConnections();
    applySettings(true);
}

void RadiosondeDemodGUI::customContextMenuRequested(QPoint pos)
{
    QTableWidgetItem *item = ui->frames->itemAt(pos);
    if (item)
    {
        int row = item->row();
        QString serial = ui->frames->item(row, FRAME_COL_SERIAL)->text();

        QMenu* tableContextMenu = new QMenu(ui->frames);
        connect(tableContextMenu, &QMenu::aboutToHide, tableContextMenu, &QMenu::deleteLater);

        // Copy current cell
        QAction* copyAction = new QAction("Copy", tableContextMenu);
        const QString text = item->text();
        connect(copyAction, &QAction::triggered, this, [text]()->void {
            QClipboard *clipboard = QGuiApplication::clipboard();
            clipboard->setText(text);
        });
        tableContextMenu->addAction(copyAction);

        // View radiosonde on various websites
        QAction* mmsiRadiosondeHubAction = new QAction(QString("View %1 on sondehub.net...").arg(serial), tableContextMenu);
        connect(mmsiRadiosondeHubAction, &QAction::triggered, this, [serial]()->void {
            QDesktopServices::openUrl(QUrl(QString("https://sondehub.org/?f=%1#!mt=Mapnik&f=%1&q=%1").arg(serial)));
        });
        tableContextMenu->addAction(mmsiRadiosondeHubAction);

        // Find on Map
        tableContextMenu->addSeparator();
        QAction* findMapFeatureAction = new QAction(QString("Find %1 on map").arg(serial), tableContextMenu);
        connect(findMapFeatureAction, &QAction::triggered, this, [serial]()->void {
            FeatureWebAPIUtils::mapFind(serial);
        });
        tableContextMenu->addAction(findMapFeatureAction);

        tableContextMenu->popup(ui->frames->viewport()->mapToGlobal(pos));
    }
}

RadiosondeDemodGUI::~RadiosondeDemodGUI()
{
    delete ui;
    qDeleteAll(m_subframes);
}

void RadiosondeDemodGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void RadiosondeDemodGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        RadiosondeDemod::MsgConfigureRadiosondeDemod* frame = RadiosondeDemod::MsgConfigureRadiosondeDemod::create( m_settings, force);
        m_radiosondeDemod->getInputMessageQueue()->push(frame);
    }
}

void RadiosondeDemodGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());

    blockApplySettings(true);

    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());

    ui->rfBWText->setText(QString("%1k").arg(m_settings.m_rfBandwidth / 1000.0, 0, 'f', 1));
    ui->rfBW->setValue(m_settings.m_rfBandwidth / 100.0);

    ui->fmDevText->setText(QString("%1k").arg(m_settings.m_fmDeviation / 1000.0, 0, 'f', 1));
    ui->fmDev->setValue(m_settings.m_fmDeviation / 100.0);

    ui->thresholdText->setText(QString("%1").arg(m_settings.m_correlationThreshold));
    ui->threshold->setValue(m_settings.m_correlationThreshold);

    updateIndexLabel();

    ui->filterSerial->setText(m_settings.m_filterSerial);

    ui->udpEnabled->setChecked(m_settings.m_udpEnabled);
    ui->udpAddress->setText(m_settings.m_udpAddress);
    ui->udpPort->setText(QString::number(m_settings.m_udpPort));

    ui->channel1->setCurrentIndex(m_settings.m_scopeCh1);
    ui->channel2->setCurrentIndex(m_settings.m_scopeCh2);

    ui->logFilename->setToolTip(QString(".csv log filename: %1").arg(m_settings.m_logFilename));
    ui->logEnable->setChecked(m_settings.m_logEnabled);

    // Order and size columns
    QHeaderView *header = ui->frames->horizontalHeader();
    for (int i = 0; i < RADIOSONDEDEMOD_FRAME_COLUMNS; i++)
    {
        bool hidden = m_settings.m_frameColumnSizes[i] == 0;
        header->setSectionHidden(i, hidden);
        framesMenu->actions().at(i)->setChecked(!hidden);
        if (m_settings.m_frameColumnSizes[i] > 0)
            ui->frames->setColumnWidth(i, m_settings.m_frameColumnSizes[i]);
        header->moveSection(header->visualIndex(i), m_settings.m_frameColumnIndexes[i]);
    }

    filter();

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void RadiosondeDemodGUI::leaveEvent(QEvent* event)
{
    m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void RadiosondeDemodGUI::enterEvent(EnterEventType* event)
{
    m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void RadiosondeDemodGUI::tick()
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    m_radiosondeDemod->getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
    double powDbAvg = CalcDb::dbPower(magsqAvg);
    double powDbPeak = CalcDb::dbPower(magsqPeak);

    ui->channelPowerMeter->levelChanged(
            (100.0f + powDbAvg) / 100.0f,
            (100.0f + powDbPeak) / 100.0f,
            nbMagsqSamples);

    if (m_tickCount % 4 == 0) {
        ui->channelPower->setText(QString::number(powDbAvg, 'f', 1));
    }

    m_tickCount++;
}

void RadiosondeDemodGUI::on_logEnable_clicked(bool checked)
{
    m_settings.m_logEnabled = checked;
    applySettings();
}

void RadiosondeDemodGUI::on_logFilename_clicked()
{
    // Get filename to save to
    QFileDialog fileDialog(nullptr, "Select file to log received frames to", "", "*.csv");
    fileDialog.setAcceptMode(QFileDialog::AcceptSave);
    if (fileDialog.exec())
    {
        QStringList fileNames = fileDialog.selectedFiles();
        if (fileNames.size() > 0)
        {
            m_settings.m_logFilename = fileNames[0];
            ui->logFilename->setToolTip(QString(".csv log filename: %1").arg(m_settings.m_logFilename));
            applySettings();
        }
    }
}

// Read .csv log and process as received frames
void RadiosondeDemodGUI::on_logOpen_clicked()
{
    QFileDialog fileDialog(nullptr, "Select .csv log file to read", "", "*.csv");
    if (fileDialog.exec())
    {
        QStringList fileNames = fileDialog.selectedFiles();
        if (fileNames.size() > 0)
        {
            QFile file(fileNames[0]);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text))
            {
                QTextStream in(&file);
                QString error;
                QHash<QString, int> colIndexes = CSV::readHeader(in, {"Date", "Time", "Data"}, error);
                if (error.isEmpty())
                {
                    int dateCol = colIndexes.value("Date");
                    int timeCol = colIndexes.value("Time");
                    int dataCol = colIndexes.value("Data");
                    int maxCol = std::max({dateCol, timeCol, dataCol});

                    QMessageBox dialog(this);
                    dialog.setText("Reading frames");
                    dialog.addButton(QMessageBox::Cancel);
                    dialog.show();
                    QApplication::processEvents();
                    int count = 0;
                    bool cancelled = false;
                    QStringList cols;

                    QList<ObjectPipe*> radiosondePipes;
                    MainCore::instance()->getMessagePipes().getMessagePipes(this, "radiosonde", radiosondePipes);

                    while (!cancelled && CSV::readRow(in, &cols))
                    {
                        if (cols.size() > maxCol)
                        {
                            QDate date = QDate::fromString(cols[dateCol]);
                            QTime time = QTime::fromString(cols[timeCol]);
                            QDateTime dateTime(date, time);
                            QByteArray bytes = QByteArray::fromHex(cols[dataCol].toLatin1());

                            // Add to table
                            frameReceived(bytes, dateTime, 0, 0);

                            // Forward to Radiosonde feature
                            for (const auto& pipe : radiosondePipes)
                            {
                                MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
                                MainCore::MsgPacket *msg = MainCore::MsgPacket::create(m_radiosondeDemod, bytes, dateTime);
                                messageQueue->push(msg);
                            }

                            if (count % 100 == 0)
                            {
                                QApplication::processEvents();
                                if (dialog.clickedButton()) {
                                    cancelled = true;
                                }
                            }
                            count++;
                        }
                    }
                    dialog.close();
                }
                else
                {
                    QMessageBox::critical(this, "Radiosonde Demod", error);
                }
            }
            else
            {
                QMessageBox::critical(this, "Radiosonde Demod", QString("Failed to open file %1").arg(fileNames[0]));
            }
        }
    }
}

void RadiosondeDemodGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &RadiosondeDemodGUI::on_deltaFrequency_changed);
    QObject::connect(ui->rfBW, &QSlider::valueChanged, this, &RadiosondeDemodGUI::on_rfBW_valueChanged);
    QObject::connect(ui->fmDev, &QSlider::valueChanged, this, &RadiosondeDemodGUI::on_fmDev_valueChanged);
    QObject::connect(ui->threshold, &QDial::valueChanged, this, &RadiosondeDemodGUI::on_threshold_valueChanged);
    QObject::connect(ui->filterSerial, &QLineEdit::editingFinished, this, &RadiosondeDemodGUI::on_filterSerial_editingFinished);
    QObject::connect(ui->clearTable, &QPushButton::clicked, this, &RadiosondeDemodGUI::on_clearTable_clicked);
    QObject::connect(ui->udpEnabled, &QCheckBox::clicked, this, &RadiosondeDemodGUI::on_udpEnabled_clicked);
    QObject::connect(ui->udpAddress, &QLineEdit::editingFinished, this, &RadiosondeDemodGUI::on_udpAddress_editingFinished);
    QObject::connect(ui->udpPort, &QLineEdit::editingFinished, this, &RadiosondeDemodGUI::on_udpPort_editingFinished);
    QObject::connect(ui->frames, &QTableWidget::cellDoubleClicked, this, &RadiosondeDemodGUI::on_frames_cellDoubleClicked);
    QObject::connect(ui->logEnable, &ButtonSwitch::clicked, this, &RadiosondeDemodGUI::on_logEnable_clicked);
    QObject::connect(ui->logFilename, &QToolButton::clicked, this, &RadiosondeDemodGUI::on_logFilename_clicked);
    QObject::connect(ui->logOpen, &QToolButton::clicked, this, &RadiosondeDemodGUI::on_logOpen_clicked);
}

void RadiosondeDemodGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}
