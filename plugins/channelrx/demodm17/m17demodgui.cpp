///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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
#include <QScrollBar>
#include <QMessageBox>
#include <QFileDialog>

#include <complex>
#include <math.h>

#include "device/deviceuiset.h"
#include "dsp/scopevisxy.h"
#include "dsp/dspcommands.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "util/csv.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "gui/crightclickenabler.h"
#include "gui/audioselectdialog.h"
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "dsp/dspengine.h"
#include "maincore.h"

#include "m17demod.h"
#include "m17demodbaudrates.h"
#include "ui_m17demodgui.h"
#include "m17demodgui.h"


M17DemodGUI* M17DemodGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
    M17DemodGUI* gui = new M17DemodGUI(pluginAPI, deviceUISet, rxChannel);
	return gui;
}

void M17DemodGUI::destroy()
{
	delete this;
}

void M17DemodGUI::resetToDefaults()
{
	m_settings.resetToDefaults();
	blockApplySettings(true);
	displaySettings();
	blockApplySettings(false);
	applySettings(QList<QString>(), true);
}

QByteArray M17DemodGUI::serialize() const
{
    return m_settings.serialize();
}

bool M17DemodGUI::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        displaySettings();
        applySettings(QList<QString>(), true);
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

bool M17DemodGUI::handleMessage(const Message& message)
{
    if (M17Demod::MsgConfigureM17Demod::match(message))
    {
        qDebug("M17DemodGUI::handleMessage: M17Demod::MsgConfigureM17Demod");
        const M17Demod::MsgConfigureM17Demod& cfg = (M17Demod::MsgConfigureM17Demod&) message;

        if (cfg.getForce()) {
            m_settings = cfg.getSettings();
        } else {
            m_settings.applySettings(cfg.getSettingsKeys(), cfg.getSettings());
        }

        blockApplySettings(true);
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
    else if (M17Demod::MsgReportSMS::match(message))
    {
        const M17Demod::MsgReportSMS& report = (M17Demod::MsgReportSMS&) message;
        QDateTime dt = QDateTime::currentDateTime();
        QString dateStr = dt.toString("HH:mm:ss");
        QTextCursor cursor = ui->smsLog->textCursor();
        cursor.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
        QString s(tr("=== %1 %2 to %3 ===\n%4\n")
            .arg(dateStr)
            .arg(report.getSource())
            .arg(report.getDest())
            .arg(report.getSMS())
        );
        cursor.insertText(s);
        ui->smsLog->verticalScrollBar()->setValue(ui->smsLog->verticalScrollBar()->maximum());

        if (ui->activateStatusLog->isChecked()) {
            m_m17StatusTextDialog.addLine(tr("SMS: %1").arg(report.getSMS()));
        }

        return true;
    }
    else if (M17Demod::MsgReportAPRS::match(message))
    {
        const M17Demod::MsgReportAPRS& report = (M17Demod::MsgReportAPRS&) message;
        // Is scroll bar at bottom
        QScrollBar *sb = ui->aprsPackets->verticalScrollBar();
        bool scrollToBottom = sb->value() == sb->maximum();

        ui->aprsPackets->setSortingEnabled(false);
        int row = ui->aprsPackets->rowCount();
        ui->aprsPackets->setRowCount(row + 1);

        QTableWidgetItem *fromItem = new QTableWidgetItem();
        QTableWidgetItem *toItem = new QTableWidgetItem();
        QTableWidgetItem *viaItem = new QTableWidgetItem();
        QTableWidgetItem *typeItem = new QTableWidgetItem();
        QTableWidgetItem *pidItem = new QTableWidgetItem();
        QTableWidgetItem *dataASCIIItem = new QTableWidgetItem();
        ui->aprsPackets->setItem(row, 0, fromItem);
        ui->aprsPackets->setItem(row, 1, toItem);
        ui->aprsPackets->setItem(row, 2, viaItem);
        ui->aprsPackets->setItem(row, 3, typeItem);
        ui->aprsPackets->setItem(row, 4, pidItem);
        ui->aprsPackets->setItem(row, 5, dataASCIIItem);
        fromItem->setText(report.getFrom());
        toItem->setText(report.getTo());
        viaItem->setText(report.getVia());
        typeItem->setText(report.getType());
        pidItem->setText(report.getPID());
        dataASCIIItem->setText(report.getData());
        ui->aprsPackets->setSortingEnabled(true);

        if (scrollToBottom) {
            ui->aprsPackets->scrollToBottom();
        }

        if (ui->activateStatusLog->isChecked())
        {
            QString s(tr("APRS: %1 to %2 via %3 typ %4 pid %5: %6")
                .arg(report.getFrom())
                .arg(report.getTo())
                .arg(report.getVia())
                .arg(report.getType())
                .arg(report.getPID())
                .arg(report.getData())
            );
            m_m17StatusTextDialog.addLine(s);
        }

        return true;
    }
    else
    {
        return false;
    }
}

void M17DemodGUI::handleInputMessages()
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

void M17DemodGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings(QList<QString>({"inputFrequencyOffset"}));
}

void M17DemodGUI::on_rfBW_valueChanged(int value)
{
	m_channelMarker.setBandwidth(value * 100);
	m_settings.m_rfBandwidth = value * 100.0;
    ui->rfBWText->setText(QString("%1k").arg(value / 10.0, 0, 'f', 1));
	applySettings(QList<QString>({"rfBandwidth"}));
}

void M17DemodGUI::on_fmDeviation_valueChanged(int value)
{
    m_settings.m_fmDeviation = value * 100.0;
    ui->fmDeviationText->setText(QString("%1%2k").arg(QChar(0xB1, 0x00)).arg(value / 10.0, 0, 'f', 1));
	applySettings(QList<QString>({"fmDeviation"}));
}

void M17DemodGUI::on_volume_valueChanged(int value)
{
    m_settings.m_volume= value / 100.0;
    ui->volumeText->setText(QString("%1").arg(value / 100.0, 0, 'f', 2));
    applySettings(QList<QString>({"volume"}));
}

void M17DemodGUI::on_baudRate_currentIndexChanged(int index)
{
    m_settings.m_baudRate = M17DemodBaudRates::getRate(index);
    applySettings(QList<QString>({"baudRate"}));
}

void M17DemodGUI::on_syncOrConstellation_toggled(bool checked)
{
    m_settings.m_syncOrConstellation = checked;
    applySettings(QList<QString>({"syncOrConstellation"}));
}

void M17DemodGUI::on_traceLength_valueChanged(int value)
{
    m_settings.m_traceLengthMutliplier = value;
    ui->traceLengthText->setText(QString("%1").arg(m_settings.m_traceLengthMutliplier*50));
    m_scopeVisXY->setPixelsPerFrame(m_settings.m_traceLengthMutliplier*960); // 48000 / 50. Chunks of 50 ms.
}

void M17DemodGUI::on_traceStroke_valueChanged(int value)
{
    m_settings.m_traceStroke = value;
    ui->traceStrokeText->setText(QString("%1").arg(m_settings.m_traceStroke));
    m_scopeVisXY->setStroke(m_settings.m_traceStroke);
}

void M17DemodGUI::on_traceDecay_valueChanged(int value)
{
    m_settings.m_traceDecay = value;
    ui->traceDecayText->setText(QString("%1").arg(m_settings.m_traceDecay));
    m_scopeVisXY->setDecay(m_settings.m_traceDecay);
}

void M17DemodGUI::on_squelchGate_valueChanged(int value)
{
    m_settings.m_squelchGate = value;
    ui->squelchGateText->setText(QString("%1").arg(value * 10.0, 0, 'f', 0));
	applySettings(QList<QString>({"squelchGate"}));
}

void M17DemodGUI::on_squelch_valueChanged(int value)
{
	ui->squelchText->setText(QString("%1").arg(value / 1.0, 0, 'f', 0));
	m_settings.m_squelch = value;
	applySettings(QList<QString>({"squelch"}));
}

void M17DemodGUI::on_audioMute_toggled(bool checked)
{
    m_settings.m_audioMute = checked;
    applySettings(QList<QString>({"audioMute"}));
}

void M17DemodGUI::on_highPassFilter_toggled(bool checked)
{
    m_settings.m_highPassFilter = checked;
    applySettings(QList<QString>({"highPassFilter"}));
}

void M17DemodGUI::on_aprsClearTable_clicked()
{
    ui->aprsPackets->setRowCount(0);
}

void M17DemodGUI::on_totButton_toggled(bool checked)
{
    m_showBERTotalOrCurrent = checked;
    ui->curButton->blockSignals(true);
    ui->curButton->setChecked(!checked);
    ui->curButton->blockSignals(false);
}

void M17DemodGUI::on_curButton_toggled(bool checked)
{
    m_showBERTotalOrCurrent = !checked;
    ui->totButton->blockSignals(true);
    ui->totButton->setChecked(!checked);
    ui->totButton->blockSignals(false);
}

void M17DemodGUI::on_berHistory_valueChanged(int value)
{
    m_berHistory = value*20;
    ui->berHistoryText->setText(tr("%1").arg(m_berHistory/2));
}

void M17DemodGUI::on_berButton_toggled(bool checked)
{
    m_showBERNumbersOrRates = !checked;
}

void M17DemodGUI::on_berReset_clicked()
{
    m_m17Demod->resetPRBS();
    m_lastBERErrors = 0;
    m_lastBERBits = 0;
    m_berPoints.clear();
    m_currentErrors.clear();
}

void M17DemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
}

void M17DemodGUI::onMenuDialogCalled(const QPoint &p)
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
            dialog.setNumberOfStreams(m_m17Demod->getNumberOfDeviceStreams());
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

        QList<QString> settingsKeys({
            "m_rgbColor",
            "title",
            "useReverseAPI",
            "reverseAPIAddress",
            "reverseAPIPort",
            "reverseAPIDeviceIndex",
            "reverseAPIChannelIndex"
        });

        if (m_deviceUISet->m_deviceMIMOEngine)
        {
            settingsKeys.append("streamIndex");
            m_settings.m_streamIndex = dialog.getSelectedStreamIndex();
            m_channelMarker.clearStreamIndexes();
            m_channelMarker.addStreamIndex(m_settings.m_streamIndex);
            updateIndexLabel();
        }

        applySettings(settingsKeys);
    }

    resetContextMenuType();
}

void M17DemodGUI::on_viewStatusLog_clicked()
{
    qDebug("M17DemodGUI::on_viewStatusLog_clicked");
    m_m17StatusTextDialog.exec();
}

M17DemodGUI::M17DemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
	ChannelGUI(parent),
	ui(new Ui::M17DemodGUI),
	m_pluginAPI(pluginAPI),
	m_deviceUISet(deviceUISet),
	m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_basebandSampleRate(1),
	m_doApplySettings(true),
	m_enableCosineFiltering(false),
	m_syncOrConstellation(false),
	m_squelchOpen(false),
    m_audioSampleRate(-1),
    m_lsfCount(0),
	m_tickCount(0),
    m_lastBERErrors(0),
    m_lastBERBits(0),
    m_showBERTotalOrCurrent(true),
    m_showBERNumbersOrRates(true),
    m_berHistory(120)
{
	setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/demodm17/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));

	ui->screenTV->setColor(true);
	ui->screenTV->resizeTVScreen(200,200);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    CRightClickEnabler *audioMuteRightClickEnabler = new CRightClickEnabler(ui->audioMute);
    connect(audioMuteRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(audioSelect()));

	m_scopeVisXY = new ScopeVisXY(ui->screenTV);
	m_scopeVisXY->setScale(2.0);
	m_scopeVisXY->setPixelsPerFrame(4001);
	m_scopeVisXY->setPlotRGB(qRgb(0, 220, 250));
	m_scopeVisXY->setGridRGB(qRgb(255, 255, 128));

	for (float x = -0.84; x < 1.0; x += 0.56)
	{
		for (float y = -0.84; y < 1.0; y += 0.56)
		{
			m_scopeVisXY->addGraticulePoint(std::complex<float>(x, y));
		}
	}

	m_scopeVisXY->calculateGraticule(200,200);

	m_m17Demod = (M17Demod*) rxChannel;
	m_m17Demod->setScopeXYSink(m_scopeVisXY);
	m_m17Demod->setMessageQueueToGUI(getInputMessageQueue());

	connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    ui->audioMute->setStyleSheet("QToolButton { background:rgb(79,79,79); }");

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
	ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);
    ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);

    m_channelMarker.blockSignals(true);
	m_channelMarker.setColor(Qt::cyan);
	m_channelMarker.setBandwidth(10000);
	m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("M17 Demodulator");
    m_channelMarker.blockSignals(false);
	m_channelMarker.setVisible(true); // activate signal on the last setting only

	m_deviceUISet->addChannelMarker(&m_channelMarker);

	connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()), this, SLOT(channelMarkerHighlightedByCursor()));

	m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setRollupState(&m_rollupState);

    ui->dcdLabel->setPixmap(QIcon(":/carrier.png").pixmap(QSize(20, 20)));
    ui->lockLabel->setPixmap(QIcon(":/locked.png").pixmap(QSize(20, 20)));
    ui->berHistoryText->setText(tr("%1").arg(m_berHistory/2));
    ui->berHistory->setValue(m_berHistory/20);

    m_berChart.setTheme(QChart::ChartThemeDark);
    m_berChart.legend()->hide();
    ui->berChart->setChart(&m_berChart);
    ui->berChart->setRenderHint(QPainter::Antialiasing);
    m_berChart.addAxis(&m_berChartXAxis, Qt::AlignBottom);
    QValueAxis *berChartYAxis = new QValueAxis();
    m_berChart.addAxis(berChartYAxis, Qt::AlignLeft);
    m_berChart.layout()->setContentsMargins(0, 0, 0, 0);
    m_berChart.setMargins(QMargins(1, 1, 1, 1));

	updateMyPosition();
	displaySettings();
    makeUIConnections();
	applySettings(QList<QString>(), true);
    DialPopup::addPopupsToChildDials(this);
}

M17DemodGUI::~M17DemodGUI()
{
	delete m_scopeVisXY;
    ui->screenTV->setParent(nullptr); // Prefer memory leak to core dump... ~TVScreen() is buggy
	delete ui;
}

void M17DemodGUI::updateMyPosition()
{
    float latitude = MainCore::instance()->getSettings().getLatitude();
    float longitude = MainCore::instance()->getSettings().getLongitude();

    if ((m_myLatitude != latitude) || (m_myLongitude != longitude))
    {
        m_m17Demod->configureMyPosition(latitude, longitude);
        m_myLatitude = latitude;
        m_myLongitude = longitude;
    }
}

void M17DemodGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setColor(m_settings.m_rgbColor);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.blockSignals(false);
    setTitleColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());

    blockApplySettings(true);

    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());

    ui->rfBW->setValue(m_settings.m_rfBandwidth / 100.0);
    ui->rfBWText->setText(QString("%1k").arg(ui->rfBW->value() / 10.0, 0, 'f', 1));

    ui->fmDeviation->setValue(m_settings.m_fmDeviation / 100.0);
    ui->fmDeviationText->setText(QString("%1%2k").arg(QChar(0xB1, 0x00)).arg(ui->fmDeviation->value() / 10.0, 0, 'f', 1));

    ui->squelch->setValue(m_settings.m_squelch);
    ui->squelchText->setText(QString("%1").arg(ui->squelch->value() / 1.0, 0, 'f', 0));

    ui->squelchGate->setValue(m_settings.m_squelchGate);
    ui->squelchGateText->setText(QString("%1").arg(ui->squelchGate->value() * 10.0, 0, 'f', 0));

    ui->volume->setValue(m_settings.m_volume * 100.0);
    ui->volumeText->setText(QString("%1").arg(ui->volume->value() / 100.0, 0, 'f', 2));

    ui->syncOrConstellation->setChecked(m_settings.m_syncOrConstellation);
    ui->audioMute->setChecked(m_settings.m_audioMute);
    ui->highPassFilter->setChecked(m_settings.m_highPassFilter);

    ui->baudRate->setCurrentIndex(M17DemodBaudRates::getRateIndex(m_settings.m_baudRate));

    ui->traceLength->setValue(m_settings.m_traceLengthMutliplier);
    ui->traceLengthText->setText(QString("%1").arg(m_settings.m_traceLengthMutliplier*50));
    m_scopeVisXY->setPixelsPerFrame(m_settings.m_traceLengthMutliplier*960); // 48000 / 50. Chunks of 50 ms.

    ui->traceStroke->setValue(m_settings.m_traceStroke);
    ui->traceStrokeText->setText(QString("%1").arg(m_settings.m_traceStroke));
    m_scopeVisXY->setStroke(m_settings.m_traceStroke);

    ui->traceDecay->setValue(m_settings.m_traceDecay);
    ui->traceDecayText->setText(QString("%1").arg(m_settings.m_traceDecay));
    m_scopeVisXY->setDecay(m_settings.m_traceDecay);

    ui->totButton->setChecked(m_showBERTotalOrCurrent);
    ui->curButton->setChecked(!m_showBERTotalOrCurrent);

    ui->berHistory->setValue(m_berHistory/20);
    ui->berHistoryText->setText(tr("%1").arg(m_berHistory/2));

    updateIndexLabel();

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void M17DemodGUI::applySettings(const QList<QString>& settingsKeys, bool force)
{
	if (m_doApplySettings)
	{
		qDebug() << "M17DemodGUI::applySettings";

        M17Demod::MsgConfigureM17Demod* message = M17Demod::MsgConfigureM17Demod::create( m_settings, settingsKeys, force);
        m_m17Demod->getInputMessageQueue()->push(message);
	}
}

void M17DemodGUI::leaveEvent(QEvent* event)
{
	m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void M17DemodGUI::enterEvent(EnterEventType* event)
{
	m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void M17DemodGUI::blockApplySettings(bool block)
{
	m_doApplySettings = !block;
}

void M17DemodGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings(QList<QString>({"inputFrequencyOffset"}));
}

void M17DemodGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void M17DemodGUI::audioSelect()
{
    AudioSelectDialog audioSelect(DSPEngine::instance()->getAudioDeviceManager(), m_settings.m_audioDeviceName);
    audioSelect.exec();

    if (audioSelect.m_selected)
    {
        m_settings.m_audioDeviceName = audioSelect.m_audioDeviceName;
        applySettings(QList<QString>({"audioDeviceName"}));
    }
}

void M17DemodGUI::tick()
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    m_m17Demod->getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
    double powDbAvg = CalcDb::dbPower(magsqAvg);
    double powDbPeak = CalcDb::dbPower(magsqPeak);

    ui->channelPowerMeter->levelChanged(
            (100.0f + powDbAvg) / 100.0f,
            (100.0f + powDbPeak) / 100.0f,
            nbMagsqSamples);

    if (m_tickCount % 4 == 0) {
        ui->channelPower->setText(tr("%1 dB").arg(powDbAvg, 0, 'f', 1));
    }

    int audioSampleRate = m_m17Demod->getAudioSampleRate();
	bool squelchOpen = m_m17Demod->getSquelchOpen();

	if ((audioSampleRate != m_audioSampleRate) || (squelchOpen != m_squelchOpen))
	{
        if (audioSampleRate < 0) {
			ui->audioMute->setStyleSheet("QToolButton { background-color : red; }");
        } else if (squelchOpen) {
			ui->audioMute->setStyleSheet("QToolButton { background-color : green; }");
		} else {
			ui->audioMute->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
		}

        m_audioSampleRate = audioSampleRate;
        m_squelchOpen = squelchOpen;
	}

    if (m_m17Demod->isRunning() && (m_tickCount % 10 == 0))
    {
        bool dcd;
        float evm;
        float deviation;
        float offset;
        int status;
        int sync_word_type;
        float clock;
        int sampleIndex;
        int syncIndex;
        int clockIndex;
        int viterbiCost;

        m_m17Demod->getDiagnostics(
            dcd,
            evm,
            deviation,
            offset,
            status,
            sync_word_type,
            clock,
            sampleIndex,
            syncIndex,
            clockIndex,
            viterbiCost
        );

        if (dcd) {
            ui->dcdLabel->setStyleSheet("QLabel { background-color : green; }");
        } else {
            ui->dcdLabel->setStyleSheet(tr("QLabel { background-color : %1; }").arg(palette().button().color().name()));
        }

        if (status == 0) { // unlocked
            ui->lockLabel->setStyleSheet(tr("QLabel { background-color : %1; }").arg(palette().button().color().name()));
        } else {
            ui->lockLabel->setStyleSheet("QLabel { background-color : green; }");
        }

        ui->syncText->setText(getStatus(status, sync_word_type, m_m17Demod->getStreamElsePacket(), m_m17Demod->getStdPacketProtocol()));
        ui->evmText->setText(tr("%1").arg(evm*100.0f, 3, 'f', 1));
        ui->deviationText->setText(tr("%1").arg(deviation/1.5f, 3, 'f', 2));
        ui->offsetText->setText(tr("%1").arg(offset/1.5f, 3, 'f', 2));
        ui->viterbiText->setText(tr("%1").arg(viterbiCost));
        ui->clockText->setText(tr("%1").arg(clock, 2, 'f', 1));
        ui->sampleText->setText(tr("%1, %2, %3").arg(sampleIndex).arg(syncIndex).arg(clockIndex));

        if (m_m17Demod->getLSFCount() != m_lsfCount)
        {
            ui->sourceText->setText(m_m17Demod->getSrcCall());
            ui->destText->setText(m_m17Demod->getDestcCall());
            ui->typeText->setText(m_m17Demod->getTypeInfo());
            ui->crcText->setText(tr("%1").arg(m_m17Demod->getCRC(), 4, 16, QChar('0')));

            if (m_m17Demod->getHasGNSS())
            {
                const MainSettings& mainSettings = MainCore::instance()->getSettings();
                float lat, lon;
                getLatLonFromGNSSMeta(m_m17Demod->getMeta(), lat, lon);
                float qrb = distance(mainSettings.getLatitude(), mainSettings.getLongitude(), lat, lon);
                float qtf = bearing(mainSettings.getLatitude(), mainSettings.getLongitude(), lat, lon);
                qDebug("M17DemodGUI::tick: GNSS: lat: %f lon: %f", lat, lon);
                ui->qrbText->setText(tr("%1").arg(qrb, 0, 'f', 1));
                ui->qtfText->setText(tr("%1").arg(qtf, 0, 'f', 1));
            }

            if (ui->activateStatusLog->isChecked() && (m_m17Demod->getLSFCount() != 0))
            {
                QString s = tr("Src: %1 Dst: %2 Typ: %3 CRC: %4")
                    .arg(m_m17Demod->getSrcCall())
                    .arg(m_m17Demod->getDestcCall())
                    .arg(m_m17Demod->getTypeInfo())
                    .arg(m_m17Demod->getCRC(), 4, 16, QChar('0'));
                m_m17StatusTextDialog.addLine(s);
            }

            m_lsfCount = m_m17Demod->getLSFCount();
        }

        if (((status == 5) || (status == 4)) && (sync_word_type == 3))
        {
            uint32_t bertErrors, bertBits;
            m_m17Demod->getBERT(bertErrors, bertBits);
            uint32_t bertErrorsDelta = bertErrors >= m_lastBERErrors ? bertErrors - m_lastBERErrors : 0;
            uint32_t bertBitsDelta = bertBits >= m_lastBERBits ? bertBits - m_lastBERBits : 0;
            m_lastBERErrors = bertErrors;
            m_lastBERBits = bertBits;

            while (m_berPoints.size() >= (int) m_berHistory) {
                m_berPoints.pop_front();
            }

            m_berPoints.append(BERPoint{
                QDateTime::currentDateTime(),
                bertErrors,
                bertBits,
                bertErrorsDelta,
                bertBitsDelta
            });
            m_currentErrors.append(bertErrorsDelta);

            QLineSeries *series;
            uint32_t min, max;
            qreal fmin, fmax;

            if (m_showBERTotalOrCurrent)
            {
                ui->berCounts->setText(tr("%1/%2").arg(bertErrors).arg(bertBits));

                if (m_showBERNumbersOrRates) {
                    series = addBERSeries(true, min, max);
                } else {
                    series = addBERSeriesRate(true, fmin, fmax);
                }

                if (bertBits > 0) {
                    ui->berRatio->setText(tr("%1").arg((double) bertErrors / (double) bertBits, 0, 'e', 2));
                }
            }
            else
            {
                ui->berCounts->setText(tr("%1/%2").arg(bertErrorsDelta).arg(bertBitsDelta));

                if (m_showBERNumbersOrRates) {
                    series = addBERSeries(false, min, max);
                } else {
                    series = addBERSeriesRate(false, fmin, fmax);
                }

                if (bertBitsDelta > 0) {
                    ui->berRatio->setText(tr("%1").arg((double) bertErrorsDelta / (double) bertBitsDelta, 0, 'e', 2));
                }
            }

            if (series)
            {
                m_berChart.removeAllSeries();
                m_berChart.removeAxis(&m_berChartXAxis);
                QAbstractAxis *berChartYAxis = m_berChart.axes(Qt::Vertical).at(0);
                m_berChart.removeAxis(berChartYAxis);

                m_berChart.addSeries(series);

                m_berChartXAxis.setRange(m_berPoints.front().m_dateTime, m_berPoints.back().m_dateTime);
                m_berChartXAxis.setFormat("hh:mm:ss");
                m_berChart.addAxis(&m_berChartXAxis, Qt::AlignBottom);
                series->attachAxis(&m_berChartXAxis);

                if (m_showBERNumbersOrRates)
                {
                    berChartYAxis = new QValueAxis();
                }
                else
                {
                    berChartYAxis = new QLogValueAxis();
                    ((QLogValueAxis*) berChartYAxis)->setLabelFormat("%.0e");
                    ((QLogValueAxis*) berChartYAxis)->setBase(10.0);
                    ((QLogValueAxis*) berChartYAxis)->setMinorTickCount(-1);

                    // make sure a major tick is always on the graph
                    if ((fmin != 0) && (fmax != 0))
                    {
                        if (fmax < 10*fmin)
                        {
                            int lmin = log10(fmin);
                            int lmax = log10(fmax);

                            if (lmin == lmax)
                            {
                                ((QLogValueAxis*) berChartYAxis)->setMin(fmin);
                                ((QLogValueAxis*) berChartYAxis)->setMax(pow(10.0, lmin));
                            }
                            else
                            {
                                ((QLogValueAxis*) berChartYAxis)->setMin(fmin);
                                ((QLogValueAxis*) berChartYAxis)->setMax(fmax);
                            }
                        }
                        else
                        {
                            ((QLogValueAxis*) berChartYAxis)->setMin(fmin);
                            ((QLogValueAxis*) berChartYAxis)->setMax(fmax);
                        }
                    }
                }

                m_berChart.addAxis(berChartYAxis, Qt::AlignLeft);
                series->attachAxis(berChartYAxis);
            }
        }
    }

	m_tickCount++;
}

QString M17DemodGUI::getStatus(int status, int sync_word_type, bool streamElsePacket, int packetProtocol)
{
    if (status == 0) {
        return "Unlocked";
    } else if (((status == 5) || (status == 4)) && (sync_word_type == 3)) {
        return "BERT";
    } else if (streamElsePacket) {
        return "Stream";
    } else if (packetProtocol == 0) {
        return "Raw";
    } else if (packetProtocol == 1) {
        return "AX.25";
    } else if (packetProtocol == 2) {
        return "APRS";
    } else if (packetProtocol == 3) {
        return "6LoWPAN";
    } else if (packetProtocol == 4) {
        return "IPv4";
    } else if (packetProtocol == 5) {
        return "SMS";
    } else if (packetProtocol == 6) {
        return "Winlink";
    } else {
        return "Unknown";
    }
}

QLineSeries *M17DemodGUI::addBERSeries(bool total, uint32_t& min, uint32_t& max)
{
    if (m_berPoints.size() < 2) {
        return nullptr;
    }

    QLineSeries *series = new QLineSeries();

    if (total)
    {
        min = m_berPoints.front().m_totalErrors;
        max = m_berPoints.back().m_totalErrors;
    }
    else
    {
        min = *std::min_element(m_currentErrors.begin(), m_currentErrors.end());
        max = *std::max_element(m_currentErrors.begin(), m_currentErrors.end());
    }

    for (auto berPoint : m_berPoints)
    {
        double x = berPoint.m_dateTime.toMSecsSinceEpoch();
        double y;

        if (total) {
            y = berPoint.m_totalErrors;
        } else {
            y = berPoint.m_currentErrors;
        }

        series->append(x, y);
    }

    return series;
}

QLineSeries *M17DemodGUI::addBERSeriesRate(bool total, qreal& min, qreal& max)
{
    if (m_berPoints.size() < 2) {
        return nullptr;
    }

    QLineSeries *series = new QLineSeries();
    min = 0;
    max = 0;

    for (auto berPoint : m_berPoints)
    {
        qreal x = berPoint.m_dateTime.toMSecsSinceEpoch();

        if (total)
        {
            if ((berPoint.m_totalErrors != 0) && (berPoint.m_totalBits != 0))
            {
                qreal y = ((qreal) berPoint.m_totalErrors) / ((qreal) berPoint.m_totalBits);
                series->append(x, y);
                max = std::max(max, y);

                if (min == 0) {
                    min = y;
                } else {
                    min = std::min(min, y);
                }
            }
        }
        else
        {
            if ((berPoint.m_currentErrors != 0) && (berPoint.m_currentBits != 0))
            {
                qreal y = ((qreal) berPoint.m_currentErrors) / ((qreal) berPoint.m_currentBits);
                series->append(x, y);
                max = std::max(max, y);

                if (min == 0) {
                    min = y;
                } else {
                    min = std::min(min, y);
                }
            }
        }
    }

    return series;
}

void M17DemodGUI::getLatLonFromGNSSMeta(const std::array<uint8_t, 14>& meta, float& lat, float& lon)
{
    int latInt = meta[2];
    int latFrac = (meta[3]<<8) + meta[4];
    lat = latInt + latFrac / 65536.0f;
    int lonInt = meta[5];
    int lonFrac = (meta[6]<<8) + meta[7];
    lon = lonInt + lonFrac / 65536.0f;
    lat = (meta[8] & 1) == 1 ? -lat : lat;
    lon = ((meta[8]>>1) & 1) == 1 ? -lon : lon;
}

float M17DemodGUI::bearing(float latFrom, float lonFrom, float latTo, float lonTo)
{
    double lat1 = latFrom * (M_PI / 180.0);
    double lon1 = lonFrom * (M_PI / 180.0);
    double lat2 = latTo * (M_PI / 180.0);
    double lon2 = lonTo * (M_PI / 180.0);
    double dLon = lon2 - lon1;
    double y = sin(dLon) * cos(lat2);
    double x = (cos(lat1)*sin(lat2)) -
                (sin(lat1)*cos(lat2)*cos(dLon));
    double bear_rad = atan2(y,x);

    if (bear_rad > 0) {
        return bear_rad * (180.0 / M_PI);
    } else {
        return 360.0 + (bear_rad * (180.0 / M_PI));
    }
}

float M17DemodGUI::distance(float latFrom, float lonFrom, float latTo, float lonTo)
{
    double lat1 = latFrom * (M_PI / 180.0);
    double lon1 = lonFrom * (M_PI / 180.0);
    double lat2 = latTo * (M_PI / 180.0);
    double lon2 = lonTo * (M_PI / 180.0);
    return acos(sin(lat1)*sin(lat2) + cos(lat1)*cos(lat2)*cos(lon2-lon1)) * 6371.0;
}

void M17DemodGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &M17DemodGUI::on_deltaFrequency_changed);
    QObject::connect(ui->rfBW, &QSlider::valueChanged, this, &M17DemodGUI::on_rfBW_valueChanged);
    QObject::connect(ui->volume, &QDial::valueChanged, this, &M17DemodGUI::on_volume_valueChanged);
    QObject::connect(ui->baudRate, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &M17DemodGUI::on_baudRate_currentIndexChanged);
    QObject::connect(ui->syncOrConstellation, &QToolButton::toggled, this, &M17DemodGUI::on_syncOrConstellation_toggled);
    QObject::connect(ui->traceLength, &QDial::valueChanged, this, &M17DemodGUI::on_traceLength_valueChanged);
    QObject::connect(ui->traceStroke, &QDial::valueChanged, this, &M17DemodGUI::on_traceStroke_valueChanged);
    QObject::connect(ui->traceDecay, &QDial::valueChanged, this, &M17DemodGUI::on_traceDecay_valueChanged);
    QObject::connect(ui->fmDeviation, &QSlider::valueChanged, this, &M17DemodGUI::on_fmDeviation_valueChanged);
    QObject::connect(ui->squelchGate, &QDial::valueChanged, this, &M17DemodGUI::on_squelchGate_valueChanged);
    QObject::connect(ui->squelch, &QDial::valueChanged, this, &M17DemodGUI::on_squelch_valueChanged);
    QObject::connect(ui->highPassFilter, &ButtonSwitch::toggled, this, &M17DemodGUI::on_highPassFilter_toggled);
    QObject::connect(ui->audioMute, &QToolButton::toggled, this, &M17DemodGUI::on_audioMute_toggled);
    QObject::connect(ui->viewStatusLog, &QPushButton::clicked, this, &M17DemodGUI::on_viewStatusLog_clicked);
    QObject::connect(ui->aprsClearTable, &QPushButton::clicked, this, &M17DemodGUI::on_aprsClearTable_clicked);
    QObject::connect(ui->totButton, &ButtonSwitch::toggled, this, &M17DemodGUI::on_totButton_toggled);
    QObject::connect(ui->curButton, &ButtonSwitch::toggled, this, &M17DemodGUI::on_curButton_toggled);
    QObject::connect(ui->berButton, &ButtonSwitch::toggled, this, &M17DemodGUI::on_berButton_toggled);
    QObject::connect(ui->berHistory, &QDial::valueChanged, this, &M17DemodGUI::on_berHistory_valueChanged);
    QObject::connect(ui->berReset, &QPushButton::clicked, this, &M17DemodGUI::on_berReset_clicked);
}

void M17DemodGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}
