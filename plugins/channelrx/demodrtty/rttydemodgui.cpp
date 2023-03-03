///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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
#include <QDebug>
#include <QAction>
#include <QClipboard>
#include <QFileDialog>
#include <QScrollBar>
#include <QMenu>

#include "rttydemodgui.h"

#include "device/deviceuiset.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "ui_rttydemodgui.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "dsp/dspengine.h"
#include "dsp/glscopesettings.h"
#include "gui/crightclickenabler.h"
#include "gui/tabletapandhold.h"
#include "gui/dialogpositioner.h"
#include "channel/channelwebapiutils.h"
#include "feature/featurewebapiutils.h"
#include "maincore.h"

#include "rttydemod.h"
#include "rttydemodsink.h"

RttyDemodGUI* RttyDemodGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
    RttyDemodGUI* gui = new RttyDemodGUI(pluginAPI, deviceUISet, rxChannel);
    return gui;
}

void RttyDemodGUI::destroy()
{
    delete this;
}

void RttyDemodGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray RttyDemodGUI::serialize() const
{
    return m_settings.serialize();
}

bool RttyDemodGUI::deserialize(const QByteArray& data)
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

void RttyDemodGUI::characterReceived(QString c)
{
    // Is the scroll bar at the bottom?
    int scrollPos = ui->text->verticalScrollBar()->value();
    bool atBottom = scrollPos >= ui->text->verticalScrollBar()->maximum();

    // Move cursor to end where we want to append new text
    // (user may have moved it by clicking / highlighting text)
    ui->text->moveCursor(QTextCursor::End);

    // Restore scroll position
    ui->text->verticalScrollBar()->setValue(scrollPos);

    if ((c == '\r') && (m_previousChar[1] == '\r') && m_settings.m_suppressCRLF)
    {
        // Don't insert yet
    }
    else if ((c == '\n') && (m_previousChar[0] == '\r') && (m_previousChar[1] == '\r') && m_settings.m_suppressCRLF)
    {
        // Change \r\r\n to \r
    }
    else if ((c != '\n') && (m_previousChar[0] == '\r') && (m_previousChar[1] == '\r') && m_settings.m_suppressCRLF)
    {
        ui->text->insertPlainText("\r"); // Insert \r we skipped
        ui->text->insertPlainText(c);
    }
    else if (c == '\b')
    {
        ui->text->textCursor().deletePreviousChar();
    }
    else
    {
        ui->text->insertPlainText(c);
    }

    // Scroll to bottom, if we we're previously at the bottom
    if (atBottom) {
        ui->text->verticalScrollBar()->setValue(ui->text->verticalScrollBar()->maximum());
    }

    // Save last 2 previous characters
    m_previousChar[0] = m_previousChar[1];
    m_previousChar[1] = c;
}

bool RttyDemodGUI::handleMessage(const Message& message)
{
    if (RttyDemod::MsgConfigureRttyDemod::match(message))
    {
        qDebug("RttyDemodGUI::handleMessage: RttyDemod::MsgConfigureRttyDemod");
        const RttyDemod::MsgConfigureRttyDemod& cfg = (RttyDemod::MsgConfigureRttyDemod&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        ui->scopeGUI->updateSettings();
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (DSPSignalNotification::match(message))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) message;
        m_deviceCenterFrequency = notif.getCenterFrequency();
        m_basebandSampleRate = notif.getSampleRate();
        ui->deltaFrequency->setValueRange(false, 7, -m_basebandSampleRate/2, m_basebandSampleRate/2);
        ui->deltaFrequencyLabel->setToolTip(tr("Range %1 %L2 Hz").arg(QChar(0xB1)).arg(m_basebandSampleRate/2));
        updateAbsoluteCenterFrequency();
        return true;
    }
    else if (RttyDemod::MsgCharacter::match(message))
    {
        RttyDemod::MsgCharacter& report = (RttyDemod::MsgCharacter&) message;
        QString c = report.getCharacter();
        characterReceived(c);
        return true;
    }
    else if (RttyDemod::MsgModeEstimate::match(message))
    {
        RttyDemod::MsgModeEstimate& report = (RttyDemod::MsgModeEstimate&) message;
        ui->baudRate->setToolTip(QString("Baud rate (symbols per second)\n\nEstimate: %1 baud").arg(report.getBaudRate()));
        ui->frequencyShift->setToolTip(QString("Frequency shift in Hz (Difference between mark and space frequency)\n\nEstimate: %1 Hz").arg(report.getFrequencyShift()));
        ui->modeEst->setText(QString("%1/%2").arg(report.getBaudRate()).arg(report.getFrequencyShift()));
        return true;
    }

    return false;
}

void RttyDemodGUI::handleInputMessages()
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

void RttyDemodGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void RttyDemodGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void RttyDemodGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings();
}

void RttyDemodGUI::on_rfBW_valueChanged(int value)
{
    float bw = value;
    ui->rfBWText->setText(formatFrequency((int)bw));
    m_channelMarker.setBandwidth(bw);
    m_settings.m_rfBandwidth = bw;
    applySettings();
}

void RttyDemodGUI::on_baudRate_currentIndexChanged(int index)
{
    (void) index;
    m_settings.m_baudRate = ui->baudRate->currentText().toFloat();
    applySettings();
}

void RttyDemodGUI::on_frequencyShift_valueChanged(int value)
{
    ui->frequencyShiftText->setText(formatFrequency(value));
    m_settings.m_frequencyShift = value;
    applySettings();
}

void RttyDemodGUI::on_squelch_valueChanged(int value)
{
    ui->squelchText->setText(QString("%1 dB").arg(value));
    m_settings.m_squelch = value;
    applySettings();
}

void RttyDemodGUI::on_characterSet_currentIndexChanged(int index)
{
    m_settings.m_characterSet = (Baudot::CharacterSet) index;
    applySettings();
}

void RttyDemodGUI::on_suppressCRLF_clicked(bool checked)
{
    m_settings.m_suppressCRLF = checked;
    applySettings();
}

void RttyDemodGUI::on_mode_currentIndexChanged(int index)
{
    (void) index;

    QString mode = ui->mode->currentText();

    bool custom = mode == "Custom";
    if (!custom)
    {
        QStringList settings = mode.split("/");
        int baudRate = settings[0].toInt();
        int frequencyShift = settings[1].toInt();
        int bandwidth = frequencyShift * 2 + baudRate;
        ui->baudRate->setCurrentText(settings[0]);
        ui->frequencyShift->setValue(frequencyShift);
        ui->rfBW->setValue(bandwidth);
    }

    ui->baudRateLabel->setEnabled(custom);
    ui->baudRate->setEnabled(custom);
    ui->frequencyShiftLabel->setEnabled(custom);
    ui->frequencyShift->setEnabled(custom);
    ui->frequencyShiftText->setEnabled(custom);
    ui->rfBWLabel->setEnabled(custom);
    ui->rfBW->setEnabled(custom);
    ui->rfBWText->setEnabled(custom);

    //m_settings.m_mode = index;
    applySettings();
}


void RttyDemodGUI::on_filter_currentIndexChanged(int index)
{
    m_settings.m_filter = (RttyDemodSettings::FilterType)index;
    applySettings();
}

void RttyDemodGUI::on_atc_clicked(bool checked)
{
    m_settings.m_atc = checked;
    applySettings();
}

void RttyDemodGUI::on_endian_clicked(bool checked)
{
    m_settings.m_msbFirst = checked;
    if (checked) {
        ui->endian->setText("MSB");
    } else {
        ui->endian->setText("LSB");
    }
    applySettings();
}

void RttyDemodGUI::on_spaceHigh_clicked(bool checked)
{
    m_settings.m_spaceHigh = checked;
    if (checked) {
        ui->spaceHigh->setText("M-S");
    } else {
        ui->spaceHigh->setText("S-M");
    }
    applySettings();
}

void RttyDemodGUI::on_clearTable_clicked()
{
    ui->text->clear();
}

void RttyDemodGUI::on_udpEnabled_clicked(bool checked)
{
    m_settings.m_udpEnabled = checked;
    applySettings();
}

void RttyDemodGUI::on_udpAddress_editingFinished()
{
    m_settings.m_udpAddress = ui->udpAddress->text();
    applySettings();
}

void RttyDemodGUI::on_udpPort_editingFinished()
{
    m_settings.m_udpPort = ui->udpPort->text().toInt();
    applySettings();
}

void RttyDemodGUI::on_channel1_currentIndexChanged(int index)
{
    m_settings.m_scopeCh1 = index;
    applySettings();
}

void RttyDemodGUI::on_channel2_currentIndexChanged(int index)
{
    m_settings.m_scopeCh2 = index;
    applySettings();
}

void RttyDemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

void RttyDemodGUI::onMenuDialogCalled(const QPoint &p)
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
            dialog.setNumberOfStreams(m_rttyDemod->getNumberOfDeviceStreams());
            dialog.setStreamIndex(m_settings.m_streamIndex);
        }

        dialog.move(p);
        new DialogPositioner(&dialog, false);
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

RttyDemodGUI::RttyDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
    ChannelGUI(parent),
    ui(new Ui::RttyDemodGUI),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_doApplySettings(true),
    m_tickCount(0)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/demodrtty/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_rttyDemod = reinterpret_cast<RttyDemod*>(rxChannel);
    m_rttyDemod->setMessageQueueToGUI(getInputMessageQueue());

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick())); // 50 ms

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);
    ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);

    m_scopeVis = m_rttyDemod->getScopeSink();
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

    m_scopeVis->setLiveRate(RttyDemodSettings::RTTYDEMOD_CHANNEL_SAMPLE_RATE);
    m_scopeVis->configure(500, 1, 0, 0, true);   // not working!
    //m_scopeVis->setFreeRun(false); // FIXME: add method rather than call m_scopeVis->configure()

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(Qt::yellow);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle("RTTY Demodulator");
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

    ui->scopeContainer->setVisible(false);

    // Hide developer only settings
    ui->filterSettingsWidget->setVisible(false);
    ui->filterLine->setVisible(false);

    displaySettings();
    makeUIConnections();
    applySettings(true);
}

RttyDemodGUI::~RttyDemodGUI()
{
    delete ui;
}

void RttyDemodGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void RttyDemodGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        RttyDemod::MsgConfigureRttyDemod* message = RttyDemod::MsgConfigureRttyDemod::create( m_settings, force);
        m_rttyDemod->getInputMessageQueue()->push(message);
    }
}

QString RttyDemodGUI::formatFrequency(int frequency) const
{
    QString suffix = "";
    if (width() > 450) {
        suffix = " Hz";
    }
    return QString("%1%2").arg(frequency).arg(suffix);
}

void RttyDemodGUI::displaySettings()
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

    ui->mode->setCurrentText("Custom");
    ui->rfBWText->setText(formatFrequency((int)m_settings.m_rfBandwidth));
    ui->rfBW->setValue(m_settings.m_rfBandwidth);
    QString baudRate;
    if (m_settings.m_baudRate < 46.0f && m_settings.m_baudRate > 45.0f) {
        baudRate = "45.45";
    } else {
        baudRate = QString("%1").arg(m_settings.m_baudRate);
    }
    ui->baudRate->setCurrentIndex(ui->baudRate->findText(baudRate));
    ui->frequencyShiftText->setText(formatFrequency(m_settings.m_frequencyShift));
    ui->frequencyShift->setValue(m_settings.m_frequencyShift);
    ui->squelchText->setText(QString("%1 dB").arg(m_settings.m_squelch));
    ui->squelch->setValue(m_settings.m_squelch);
    ui->characterSet->setCurrentIndex((int)m_settings.m_characterSet);
    ui->suppressCRLF->setChecked(m_settings.m_suppressCRLF);
    ui->filter->setCurrentIndex((int)m_settings.m_filter);
    ui->atc->setChecked(m_settings.m_atc);
    ui->endian->setChecked(m_settings.m_msbFirst);
    if (m_settings.m_msbFirst) {
        ui->endian->setText("MSB");
    } else {
        ui->endian->setText("LSB");
    }
    ui->spaceHigh->setChecked(m_settings.m_spaceHigh);
    if (m_settings.m_spaceHigh) {
        ui->spaceHigh->setText("M-S");
    } else {
        ui->spaceHigh->setText("S-M");
    }

    updateIndexLabel();

    ui->udpEnabled->setChecked(m_settings.m_udpEnabled);
    ui->udpAddress->setText(m_settings.m_udpAddress);
    ui->udpPort->setText(QString::number(m_settings.m_udpPort));

    ui->channel1->setCurrentIndex(m_settings.m_scopeCh1);
    ui->channel2->setCurrentIndex(m_settings.m_scopeCh2);

    ui->logFilename->setToolTip(QString(".txt log filename: %1").arg(m_settings.m_logFilename));
    ui->logEnable->setChecked(m_settings.m_logEnabled);

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void RttyDemodGUI::leaveEvent(QEvent* event)
{
    m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void RttyDemodGUI::enterEvent(EnterEventType* event)
{
    m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void RttyDemodGUI::tick()
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    m_rttyDemod->getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
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

void RttyDemodGUI::on_logEnable_clicked(bool checked)
{
    m_settings.m_logEnabled = checked;
    applySettings();
}

void RttyDemodGUI::on_logFilename_clicked()
{
    // Get filename to save to
    QFileDialog fileDialog(nullptr, "Select file to log received text to", "", "*.txt");
    fileDialog.setAcceptMode(QFileDialog::AcceptSave);
    if (fileDialog.exec())
    {
        QStringList fileNames = fileDialog.selectedFiles();
        if (fileNames.size() > 0)
        {
            m_settings.m_logFilename = fileNames[0];
            ui->logFilename->setToolTip(QString(".txt log filename: %1").arg(m_settings.m_logFilename));
            applySettings();
        }
    }
}

void RttyDemodGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &RttyDemodGUI::on_deltaFrequency_changed);
    QObject::connect(ui->rfBW, &QSlider::valueChanged, this, &RttyDemodGUI::on_rfBW_valueChanged);
    QObject::connect(ui->baudRate, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RttyDemodGUI::on_baudRate_currentIndexChanged);
    QObject::connect(ui->frequencyShift, &QSlider::valueChanged, this, &RttyDemodGUI::on_frequencyShift_valueChanged);
    QObject::connect(ui->squelch, &QDial::valueChanged, this, &RttyDemodGUI::on_squelch_valueChanged);
    QObject::connect(ui->characterSet, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RttyDemodGUI::on_characterSet_currentIndexChanged);
    QObject::connect(ui->suppressCRLF, &ButtonSwitch::clicked, this, &RttyDemodGUI::on_suppressCRLF_clicked);
    QObject::connect(ui->mode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RttyDemodGUI::on_mode_currentIndexChanged);
    QObject::connect(ui->filter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RttyDemodGUI::on_filter_currentIndexChanged);
    QObject::connect(ui->atc, &QCheckBox::clicked, this, &RttyDemodGUI::on_atc_clicked);
    QObject::connect(ui->endian, &QCheckBox::clicked, this, &RttyDemodGUI::on_endian_clicked);
    QObject::connect(ui->spaceHigh, &QCheckBox::clicked, this, &RttyDemodGUI::on_spaceHigh_clicked);
    QObject::connect(ui->clearTable, &QPushButton::clicked, this, &RttyDemodGUI::on_clearTable_clicked);
    QObject::connect(ui->udpEnabled, &QCheckBox::clicked, this, &RttyDemodGUI::on_udpEnabled_clicked);
    QObject::connect(ui->udpAddress, &QLineEdit::editingFinished, this, &RttyDemodGUI::on_udpAddress_editingFinished);
    QObject::connect(ui->udpPort, &QLineEdit::editingFinished, this, &RttyDemodGUI::on_udpPort_editingFinished);
    QObject::connect(ui->logEnable, &ButtonSwitch::clicked, this, &RttyDemodGUI::on_logEnable_clicked);
    QObject::connect(ui->logFilename, &QToolButton::clicked, this, &RttyDemodGUI::on_logFilename_clicked);
    QObject::connect(ui->channel1, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RttyDemodGUI::on_channel1_currentIndexChanged);
    QObject::connect(ui->channel2, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RttyDemodGUI::on_channel2_currentIndexChanged);
}

void RttyDemodGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}

