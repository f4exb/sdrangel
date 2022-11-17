///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
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

#include "dsddemodgui.h"

#include "device/deviceuiset.h"

#include "ui_dsddemodgui.h"
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

#include "dsddemodbaudrates.h"
#include "dsddemod.h"

#include <QDockWidget>
#include <QMainWindow>
#include <QDebug>

#include <complex>

DSDDemodGUI* DSDDemodGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
    DSDDemodGUI* gui = new DSDDemodGUI(pluginAPI, deviceUISet, rxChannel);
	return gui;
}

void DSDDemodGUI::destroy()
{
	delete this;
}

void DSDDemodGUI::resetToDefaults()
{
	m_settings.resetToDefaults();
	blockApplySettings(true);
	displaySettings();
	blockApplySettings(false);
	applySettings();
}

QByteArray DSDDemodGUI::serialize() const
{
    return m_settings.serialize();
}

bool DSDDemodGUI::deserialize(const QByteArray& data)
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

bool DSDDemodGUI::handleMessage(const Message& message)
{
    if (DSDDemod::MsgConfigureDSDDemod::match(message))
    {
        qDebug("DSDDemodGUI::handleMessage: DSDDemod::MsgConfigureDSDDemod");
        const DSDDemod::MsgConfigureDSDDemod& cfg = (DSDDemod::MsgConfigureDSDDemod&) message;
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
    else if (DSDDemod::MsgReportAvailableAMBEFeatures::match(message))
    {
        DSDDemod::MsgReportAvailableAMBEFeatures& report = (DSDDemod::MsgReportAvailableAMBEFeatures&) message;
        m_availableAMBEFeatures = report.getFeatures();
        updateAMBEFeaturesList();
        return true;
    }
    else
    {
        return false;
    }
}

void DSDDemodGUI::handleInputMessages()
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

void DSDDemodGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings();
}

void DSDDemodGUI::on_rfBW_valueChanged(int value)
{
	m_channelMarker.setBandwidth(value * 100);
	m_settings.m_rfBandwidth = value * 100.0;
    ui->rfBWText->setText(QString("%1k").arg(value / 10.0, 0, 'f', 1));
	applySettings();
}

void DSDDemodGUI::on_demodGain_valueChanged(int value)
{
    m_settings.m_demodGain = value / 100.0;
    ui->demodGainText->setText(QString("%1").arg(value / 100.0, 0, 'f', 2));
	applySettings();
}

void DSDDemodGUI::on_fmDeviation_valueChanged(int value)
{
    m_settings.m_fmDeviation = value * 100.0;
    ui->fmDeviationText->setText(QString("%1%2k").arg(QChar(0xB1, 0x00)).arg(value / 10.0, 0, 'f', 1));
	applySettings();
}

void DSDDemodGUI::on_volume_valueChanged(int value)
{
    m_settings.m_volume= value / 10.0;
    ui->volumeText->setText(QString("%1").arg(value / 10.0, 0, 'f', 1));
    applySettings();
}

void DSDDemodGUI::on_baudRate_currentIndexChanged(int index)
{
    m_settings.m_baudRate = DSDDemodBaudRates::getRate(index);
    applySettings();
}

void DSDDemodGUI::on_enableCosineFiltering_toggled(bool enable)
{
    m_settings.m_enableCosineFiltering = enable;
	applySettings();
}

void DSDDemodGUI::on_syncOrConstellation_toggled(bool checked)
{
    m_settings.m_syncOrConstellation = checked;
    applySettings();
}

void DSDDemodGUI::on_traceLength_valueChanged(int value)
{
    m_settings.m_traceLengthMutliplier = value;
    ui->traceLengthText->setText(QString("%1").arg(m_settings.m_traceLengthMutliplier*50));
    m_scopeVisXY->setPixelsPerFrame(m_settings.m_traceLengthMutliplier*960); // 48000 / 50. Chunks of 50 ms.
}

void DSDDemodGUI::on_traceStroke_valueChanged(int value)
{
    m_settings.m_traceStroke = value;
    ui->traceStrokeText->setText(QString("%1").arg(m_settings.m_traceStroke));
    m_scopeVisXY->setStroke(m_settings.m_traceStroke);
}

void DSDDemodGUI::on_traceDecay_valueChanged(int value)
{
    m_settings.m_traceDecay = value;
    ui->traceDecayText->setText(QString("%1").arg(m_settings.m_traceDecay));
    m_scopeVisXY->setDecay(m_settings.m_traceDecay);
}

void DSDDemodGUI::on_slot1On_toggled(bool checked)
{
    m_settings.m_slot1On = checked;
    applySettings();
}

void DSDDemodGUI::on_slot2On_toggled(bool checked)
{
    m_settings.m_slot2On = checked;
    applySettings();
}

void DSDDemodGUI::on_tdmaStereoSplit_toggled(bool checked)
{
    m_settings.m_tdmaStereo = checked;
    applySettings();
}

void DSDDemodGUI::on_squelchGate_valueChanged(int value)
{
    m_settings.m_squelchGate = value;
    ui->squelchGateText->setText(QString("%1").arg(value * 10.0, 0, 'f', 0));
	applySettings();
}

void DSDDemodGUI::on_squelch_valueChanged(int value)
{
	ui->squelchText->setText(QString("%1").arg(value / 1.0, 0, 'f', 0));
	m_settings.m_squelch = value;
	applySettings();
}

void DSDDemodGUI::on_audioMute_toggled(bool checked)
{
    m_settings.m_audioMute = checked;
    applySettings();
}

void DSDDemodGUI::on_highPassFilter_toggled(bool checked)
{
    m_settings.m_highPassFilter = checked;
    applySettings();
}

void DSDDemodGUI::on_symbolPLLLock_toggled(bool checked)
{
    if (checked) {
        ui->symbolPLLLock->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
    } else {
        ui->symbolPLLLock->setStyleSheet("QToolButton { background:rgb(53,53,53); }");
    }
    m_settings.m_pllLock = checked;
    applySettings();
}

void DSDDemodGUI::on_ambeSupport_clicked(bool checked)
{
    if (ui->ambeFeatures->currentIndex() < 0) {
        return;
    }

    m_settings.m_connectAMBE = checked;
    m_settings.m_ambeFeatureIndex = m_availableAMBEFeatures[ui->ambeFeatures->currentIndex()].m_featureIndex;
    applySettings();
}

void DSDDemodGUI::on_ambeFeatures_currentIndexChanged(int index)
{
    m_settings.m_ambeFeatureIndex = m_availableAMBEFeatures[index].m_featureIndex;
    applySettings();
}

void DSDDemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

void DSDDemodGUI::onMenuDialogCalled(const QPoint &p)
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
            dialog.setNumberOfStreams(m_dsdDemod->getNumberOfDeviceStreams());
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

void DSDDemodGUI::on_viewStatusLog_clicked()
{
    qDebug("DSDDemodGUI::on_viewStatusLog_clicked");
    m_dsdStatusTextDialog.exec();
}

DSDDemodGUI::DSDDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
	ChannelGUI(parent),
	ui(new Ui::DSDDemodGUI),
	m_pluginAPI(pluginAPI),
	m_deviceUISet(deviceUISet),
	m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_basebandSampleRate(1),
	m_doApplySettings(true),
	m_enableCosineFiltering(false),
	m_syncOrConstellation(false),
	m_slot1On(false),
	m_slot2On(false),
	m_tdmaStereo(false),
	m_squelchOpen(false),
    m_audioSampleRate(-1),
	m_tickCount(0),
	m_dsdStatusTextDialog(0)
{
	setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/demoddsd/readme.md";
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

	m_dsdDemod = (DSDDemod*) rxChannel;
	m_dsdDemod->setScopeXYSink(m_scopeVisXY);
	m_dsdDemod->setMessageQueueToGUI(getInputMessageQueue());

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
    m_channelMarker.setTitle("DSD Demodulator");
    m_channelMarker.blockSignals(false);
	m_channelMarker.setVisible(true); // activate signal on the last setting only

	m_deviceUISet->addChannelMarker(&m_channelMarker);

	connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()), this, SLOT(channelMarkerHighlightedByCursor()));

	m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setRollupState(&m_rollupState);

	updateMyPosition();
    updateAMBEFeaturesList();
	displaySettings();
    makeUIConnections();
	applySettings(true);
}

DSDDemodGUI::~DSDDemodGUI()
{
	delete m_scopeVisXY;
    ui->screenTV->setParent(nullptr); // Prefer memory leak to core dump... ~TVScreen() is buggy
	delete ui;
}

void DSDDemodGUI::updateMyPosition()
{
    float latitude = MainCore::instance()->getSettings().getLatitude();
    float longitude = MainCore::instance()->getSettings().getLongitude();

    if ((m_myLatitude != latitude) || (m_myLongitude != longitude))
    {
        m_dsdDemod->configureMyPosition(latitude, longitude);
        m_myLatitude = latitude;
        m_myLongitude = longitude;
    }
}

void DSDDemodGUI::displaySettings()
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

    ui->demodGain->setValue(m_settings.m_demodGain * 100.0);
    ui->demodGainText->setText(QString("%1").arg(ui->demodGain->value() / 100.0, 0, 'f', 2));

    ui->volume->setValue(m_settings.m_volume * 10.0);
    ui->volumeText->setText(QString("%1").arg(ui->volume->value() / 10.0, 0, 'f', 1));

    ui->enableCosineFiltering->setChecked(m_settings.m_enableCosineFiltering);
    ui->syncOrConstellation->setChecked(m_settings.m_syncOrConstellation);
    ui->slot1On->setChecked(m_settings.m_slot1On);
    ui->slot2On->setChecked(m_settings.m_slot2On);
    ui->tdmaStereoSplit->setChecked(m_settings.m_tdmaStereo);
    ui->audioMute->setChecked(m_settings.m_audioMute);
    ui->symbolPLLLock->setChecked(m_settings.m_pllLock);
    ui->highPassFilter->setChecked(m_settings.m_highPassFilter);

    ui->baudRate->setCurrentIndex(DSDDemodBaudRates::getRateIndex(m_settings.m_baudRate));

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

void DSDDemodGUI::updateAMBEFeaturesList()
{
    ui->ambeFeatures->blockSignals(true);
    ui->ambeSupport->blockSignals(true);
    ui->ambeFeatures->clear();
    ui->ambeSupport->setEnabled(m_availableAMBEFeatures.count() > 0);
    int selectedFeatureIndex = -1;
    bool unsetAMBE = false;

    if (m_availableAMBEFeatures.count() == 0)
    {
        ui->ambeSupport->setChecked(false);
        unsetAMBE = m_settings.m_connectAMBE;
        m_settings.m_connectAMBE = false;
    }
    else
    {
        ui->ambeSupport->setChecked(m_settings.m_connectAMBE);
    }

    for (int i = 0; i < m_availableAMBEFeatures.count(); i++)
    {
        ui->ambeFeatures->addItem(tr("F:%1").arg(m_availableAMBEFeatures[i].m_featureIndex), m_availableAMBEFeatures[i].m_featureIndex);

        if (m_availableAMBEFeatures[i].m_featureIndex == m_settings.m_ambeFeatureIndex) {
            selectedFeatureIndex = i;
        }
    }

    if (selectedFeatureIndex > 0) {
        ui->ambeFeatures->setCurrentIndex(selectedFeatureIndex);
    }

    ui->ambeSupport->blockSignals(false);
    ui->ambeFeatures->blockSignals(false);

    if (unsetAMBE) {
        applySettings();
    }
}

void DSDDemodGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
		qDebug() << "DSDDemodGUI::applySettings";

        DSDDemod::MsgConfigureDSDDemod* message = DSDDemod::MsgConfigureDSDDemod::create( m_settings, force);
        m_dsdDemod->getInputMessageQueue()->push(message);
	}
}

void DSDDemodGUI::leaveEvent(QEvent* event)
{
	m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void DSDDemodGUI::enterEvent(EnterEventType* event)
{
	m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void DSDDemodGUI::blockApplySettings(bool block)
{
	m_doApplySettings = !block;
}

void DSDDemodGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void DSDDemodGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void DSDDemodGUI::audioSelect()
{
    qDebug("DSDDemodGUI::audioSelect");
    AudioSelectDialog audioSelect(DSPEngine::instance()->getAudioDeviceManager(), m_settings.m_audioDeviceName);
    audioSelect.exec();

    if (audioSelect.m_selected)
    {
        m_settings.m_audioDeviceName = audioSelect.m_audioDeviceName;
        applySettings();
    }
}

void DSDDemodGUI::tick()
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    m_dsdDemod->getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
    double powDbAvg = CalcDb::dbPower(magsqAvg);
    double powDbPeak = CalcDb::dbPower(magsqPeak);

    ui->channelPowerMeter->levelChanged(
            (100.0f + powDbAvg) / 100.0f,
            (100.0f + powDbPeak) / 100.0f,
            nbMagsqSamples);

    if (m_tickCount % 4 == 0) {
        ui->channelPower->setText(tr("%1 dB").arg(powDbAvg, 0, 'f', 1));
    }

    int audioSampleRate = m_dsdDemod->getAudioSampleRate();
	bool squelchOpen = m_dsdDemod->getSquelchOpen();

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

	// "slow" updates

	if (m_dsdDemod->isRunning() & (m_tickCount % 10 == 0))
	{
	    ui->inLevelText->setText(QString::number(m_dsdDemod->getDecoder().getInLevel()));
        ui->inCarrierPosText->setText(QString::number(m_dsdDemod->getDecoder().getCarrierPos()));
        ui->zcPosText->setText(QString::number(m_dsdDemod->getDecoder().getZeroCrossingPos()));
        ui->symbolSyncQualityText->setText(QString::number(m_dsdDemod->getDecoder().getSymbolSyncQuality()));

        if (m_dsdDemod->getDecoder().getVoice1On()) {
            ui->slot1On->setStyleSheet("QToolButton { background-color : green; }");
        } else {
            ui->slot1On->setStyleSheet("QToolButton { background-color : rgb(79,79,79); }");
        }

        if (m_dsdDemod->getDecoder().getVoice2On()) {
            ui->slot2On->setStyleSheet("QToolButton { background-color : green; }");
        } else {
            ui->slot2On->setStyleSheet("QToolButton { background-color : rgb(79,79,79); }");
        }

        const char *frameTypeText = m_dsdDemod->getDecoder().getFrameTypeText();

	    if (frameTypeText[0] == '\0') {
	        ui->syncText->setStyleSheet("QLabel { background:rgb(53,53,53); }"); // turn off background
	    } else {
            ui->syncText->setStyleSheet("QLabel { background:rgb(37,53,39); }"); // turn on background
	    }

	    ui->syncText->setText(QString(frameTypeText));

	    const char *formatStatusText = m_dsdDemod->updateAndGetStatusText();
	    ui->formatStatusText->setText(QString(formatStatusText));

	    if (ui->activateStatusLog->isChecked()) {
	        m_dsdStatusTextDialog.addLine(QString(formatStatusText));
	    }

	    if (formatStatusText[0] == '\0') {
	        ui->formatStatusText->setStyleSheet("QLabel { background:rgb(53,53,53); }"); // turn off background
	    } else {
            ui->formatStatusText->setStyleSheet("QLabel { background:rgb(37,53,39); }"); // turn on background
	    }

        if (m_squelchOpen && ui->symbolPLLLock->isChecked() && m_dsdDemod->getDecoder().getSymbolPLLLocked()) {
            ui->symbolPLLLock->setStyleSheet("QToolButton { background-color : green; }");
        } else {
            ui->symbolPLLLock->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
        }
	}

	m_tickCount++;
}

void DSDDemodGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &DSDDemodGUI::on_deltaFrequency_changed);
    QObject::connect(ui->rfBW, &QSlider::valueChanged, this, &DSDDemodGUI::on_rfBW_valueChanged);
    QObject::connect(ui->demodGain, &QSlider::valueChanged, this, &DSDDemodGUI::on_demodGain_valueChanged);
    QObject::connect(ui->volume, &QDial::valueChanged, this, &DSDDemodGUI::on_volume_valueChanged);
    QObject::connect(ui->baudRate, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DSDDemodGUI::on_baudRate_currentIndexChanged);
    QObject::connect(ui->enableCosineFiltering, &ButtonSwitch::toggled, this, &DSDDemodGUI::on_enableCosineFiltering_toggled);
    QObject::connect(ui->syncOrConstellation, &QToolButton::toggled, this, &DSDDemodGUI::on_syncOrConstellation_toggled);
    QObject::connect(ui->traceLength, &QDial::valueChanged, this, &DSDDemodGUI::on_traceLength_valueChanged);
    QObject::connect(ui->traceStroke, &QDial::valueChanged, this, &DSDDemodGUI::on_traceStroke_valueChanged);
    QObject::connect(ui->traceDecay, &QDial::valueChanged, this, &DSDDemodGUI::on_traceDecay_valueChanged);
    QObject::connect(ui->tdmaStereoSplit, &QToolButton::toggled, this, &DSDDemodGUI::on_tdmaStereoSplit_toggled);
    QObject::connect(ui->fmDeviation, &QSlider::valueChanged, this, &DSDDemodGUI::on_fmDeviation_valueChanged);
    QObject::connect(ui->squelchGate, &QDial::valueChanged, this, &DSDDemodGUI::on_squelchGate_valueChanged);
    QObject::connect(ui->squelch, &QDial::valueChanged, this, &DSDDemodGUI::on_squelch_valueChanged);
    QObject::connect(ui->highPassFilter, &ButtonSwitch::toggled, this, &DSDDemodGUI::on_highPassFilter_toggled);
    QObject::connect(ui->audioMute, &QToolButton::toggled, this, &DSDDemodGUI::on_audioMute_toggled);
    QObject::connect(ui->symbolPLLLock, &QToolButton::toggled, this, &DSDDemodGUI::on_symbolPLLLock_toggled);
    QObject::connect(ui->viewStatusLog, &QPushButton::clicked, this, &DSDDemodGUI::on_viewStatusLog_clicked);
    QObject::connect(ui->ambeSupport, &QCheckBox::clicked, this, &DSDDemodGUI::on_ambeSupport_clicked);
    QObject::connect(ui->ambeFeatures, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DSDDemodGUI::on_ambeFeatures_currentIndexChanged);
}

void DSDDemodGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}
