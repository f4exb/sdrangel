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

#include <complex>


#include "device/deviceuiset.h"
#include "dsp/scopevisxy.h"
#include "dsp/dspcommands.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "gui/crightclickenabler.h"
#include "gui/audioselectdialog.h"
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
	applySettings();
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
        applySettings(true);
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void M17DemodGUI::resizeEvent(QResizeEvent* size)
{
    int maxWidth = getRollupContents()->maximumWidth();
    int minHeight = getRollupContents()->minimumHeight() + getAdditionalHeight();
    resize(width() < maxWidth ? width() : maxWidth, minHeight);
    size->accept();
}

bool M17DemodGUI::handleMessage(const Message& message)
{
    if (M17Demod::MsgConfigureM17Demod::match(message))
    {
        qDebug("M17DemodGUI::handleMessage: M17Demod::MsgConfigureM17Demod");
        const M17Demod::MsgConfigureM17Demod& cfg = (M17Demod::MsgConfigureM17Demod&) message;
        m_settings = cfg.getSettings();
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
    applySettings();
}

void M17DemodGUI::on_rfBW_valueChanged(int value)
{
	m_channelMarker.setBandwidth(value * 100);
	m_settings.m_rfBandwidth = value * 100.0;
    ui->rfBWText->setText(QString("%1k").arg(value / 10.0, 0, 'f', 1));
	applySettings();
}

void M17DemodGUI::on_fmDeviation_valueChanged(int value)
{
    m_settings.m_fmDeviation = value * 100.0;
    ui->fmDeviationText->setText(QString("%1%2k").arg(QChar(0xB1, 0x00)).arg(value / 10.0, 0, 'f', 1));
	applySettings();
}

void M17DemodGUI::on_volume_valueChanged(int value)
{
    m_settings.m_volume= value / 10.0;
    ui->volumeText->setText(QString("%1").arg(value / 10.0, 0, 'f', 1));
    applySettings();
}

void M17DemodGUI::on_baudRate_currentIndexChanged(int index)
{
    m_settings.m_baudRate = M17DemodBaudRates::getRate(index);
    applySettings();
}

void M17DemodGUI::on_syncOrConstellation_toggled(bool checked)
{
    m_settings.m_syncOrConstellation = checked;
    applySettings();
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
	applySettings();
}

void M17DemodGUI::on_squelch_valueChanged(int value)
{
	ui->squelchText->setText(QString("%1").arg(value / 1.0, 0, 'f', 0));
	m_settings.m_squelch = value;
	applySettings();
}

void M17DemodGUI::on_audioMute_toggled(bool checked)
{
    m_settings.m_audioMute = checked;
    applySettings();
}

void M17DemodGUI::on_highPassFilter_toggled(bool checked)
{
    m_settings.m_highPassFilter = checked;
    applySettings();
}

void M17DemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
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
	m_tickCount(0)
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

	updateMyPosition();
	displaySettings();
    makeUIConnections();
	applySettings(true);
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

    ui->volume->setValue(m_settings.m_volume * 10.0);
    ui->volumeText->setText(QString("%1").arg(ui->volume->value() / 10.0, 0, 'f', 1));

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

    updateIndexLabel();

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void M17DemodGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
		qDebug() << "M17DemodGUI::applySettings";

        M17Demod::MsgConfigureM17Demod* message = M17Demod::MsgConfigureM17Demod::create( m_settings, force);
        m_m17Demod->getInputMessageQueue()->push(message);
	}
}

void M17DemodGUI::leaveEvent(QEvent* event)
{
	m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void M17DemodGUI::enterEvent(QEvent* event)
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
    applySettings();
}

void M17DemodGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void M17DemodGUI::audioSelect()
{
    qDebug("M17DemodGUI::audioSelect");
    AudioSelectDialog audioSelect(DSPEngine::instance()->getAudioDeviceManager(), m_settings.m_audioDeviceName);
    audioSelect.exec();

    if (audioSelect.m_selected)
    {
        m_settings.m_audioDeviceName = audioSelect.m_audioDeviceName;
        applySettings();
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

    if (m_tickCount % 10 == 0)
    {
        bool dcd;
        float evm;
        float deviation;
        float offset;
        int status;
        float clock;
        int sampleIndex;
        int syncIndex;
        int clockIndex;
        int viterbiCost;

        m_m17Demod->getDiagnostics(dcd, evm, deviation, offset, status, clock, sampleIndex, syncIndex, clockIndex, viterbiCost);

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

        ui->syncText->setText(getStatus(status));
        ui->evmText->setText(tr("%1").arg(evm*100.0f, 3, 'f', 1));
        ui->deviationText->setText(tr("%1").arg(deviation, 2, 'f', 1));
        ui->offsetText->setText(tr("%1").arg(offset, 3, 'f', 2));
        ui->viterbiText->setText(tr("%1").arg(viterbiCost));
        ui->clockText->setText(tr("%1").arg(clock, 2, 'f', 1));
        ui->sampleText->setText(tr("%1, %2, %3").arg(sampleIndex).arg(syncIndex).arg(clockIndex));

        if (m_m17Demod->getLSFCount() != m_lsfCount)
        {
            ui->sourceText->setText(m_m17Demod->getSrcCall());
            ui->destText->setText(m_m17Demod->getDestcCall());
            ui->typeText->setText(m_m17Demod->getTypeInfo());
            ui->crcText->setText(tr("%1").arg(m_m17Demod->getCRC(), 4, 16, QChar('0')));
            m_lsfCount = m_m17Demod->getLSFCount();
        }
    }

	m_tickCount++;
}

QString M17DemodGUI::getStatus(int status)
{
    if (status == 0) {
        return "Unlocked";
    } else if (status == 1) {
        return "LSF";
    } else if (status == 2) {
        return "Stream";
    } else if (status == 3) {
        return "Packet";
    } else if (status == 4) {
        return "BERT";
    } else if (status == 5) {
        return "Frame";
    } else {
        return "Unknown";
    }
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
}

void M17DemodGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}
