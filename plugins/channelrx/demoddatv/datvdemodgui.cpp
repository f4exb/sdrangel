///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 F4HKW                                                      //
// for F4EXB / SDRAngel                                                          //
// using LeanSDR Framework (C) 2016 F4DAV                                        //
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
#include <QMediaMetaData>

#include "device/deviceuiset.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "gui/crightclickenabler.h"
#include "gui/audioselectdialog.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "mainwindow.h"

#include "ui_datvdemodgui.h"
#include "datvdemodreport.h"
#include "datvdvbs2ldpcdialog.h"
#include "datvdemodgui.h"

const char* const DATVDemodGUI::m_strChannelID = "sdrangel.channel.demoddatv";
const QList<int> DATVDemodGUI::m_symbolRates = {
    25000,
    33000,
    66000,
    125000,
    250000,
    333000,
    500000,
    1000000,
    1500000,
    2000000
};

DATVDemodGUI* DATVDemodGUI::create(PluginAPI* objPluginAPI,
        DeviceUISet *deviceUISet,
        BasebandSampleSink *rxChannel)
{
    DATVDemodGUI* gui = new DATVDemodGUI(objPluginAPI, deviceUISet, rxChannel);
    return gui;
}

void DATVDemodGUI::destroy()
{
    delete this;
}

void DATVDemodGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
	applySettings(true);
}

QByteArray DATVDemodGUI::serialize() const
{
    return m_settings.serialize();
}

bool DATVDemodGUI::deserialize(const QByteArray& arrData)
{
    if (m_settings.deserialize(arrData))
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

bool DATVDemodGUI::handleMessage(const Message& message)
{
    if (DATVDemodReport::MsgReportModcodCstlnChange::match(message))
    {
        DATVDemodReport::MsgReportModcodCstlnChange& notif = (DATVDemodReport::MsgReportModcodCstlnChange&) message;
        m_settings.m_fec = notif.getCodeRate();
        m_settings.m_modulation = notif.getModulation();
        m_settings.validateSystemConfiguration();
        qDebug("DATVDemodReport::MsgReportModcodCstlnChange: m_modulation: %d m_fec: %d",
            m_settings.m_modulation, m_settings.m_fec);
        displaySystemConfiguration();
        return true;
    }
    else if (DATVDemod::MsgConfigureDATVDemod::match(message))
    {
        DATVDemod::MsgConfigureDATVDemod& cfg = (DATVDemod::MsgConfigureDATVDemod&) message;
        m_settings = cfg.getSettings();
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        return true;
    }
    else if (DSPSignalNotification::match(message))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) message;
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

void DATVDemodGUI::handleInputMessages()
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

void DATVDemodGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_centerFrequency = m_channelMarker.getCenterFrequency();
    applySettings();
}

void DATVDemodGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}


void DATVDemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

void DATVDemodGUI::onMenuDialogCalled(const QPoint &p)
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
            dialog.setNumberOfStreams(m_datvDemod->getNumberOfDeviceStreams());
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

DATVDemodGUI::DATVDemodGUI(PluginAPI* objPluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* objParent) :
    ChannelGUI(objParent),
    ui(new Ui::DATVDemodGUI),
    m_objPluginAPI(objPluginAPI),
    m_deviceUISet(deviceUISet),
    m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_basebandSampleRate(1),
    m_blnBasicSettingsShown(false),
    m_blnDoApplySettings(true),
    m_modcodModulationIndex(-1),
    m_modcodCodeRateIndex(-1),
    m_cstlnSetByModcod(false)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/demoddatv/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));

    ui->screenTV->setColor(true);
    ui->screenTV->resizeTVScreen(256,256);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    ui->merMeter->setColorTheme(LevelMeterSignalDB::ColorCyanAndBlue);
    ui->merMeter->setRange(0, 30);
    ui->merMeter->setAverageSmoothing(32);

    ui->cnrMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);
    ui->cnrMeter->setRange(0, 30);
    ui->cnrMeter->setAverageSmoothing(2);

    m_datvDemod = (DATVDemod*) rxChannel;
    m_datvDemod->setMessageQueueToGUI(getInputMessageQueue());

    m_datvDemod->SetTVScreen(ui->screenTV);
    m_datvDemod->SetVideoRender(ui->screenTV_2);

    if (m_settings.m_playerEnable) {
        connect(m_datvDemod->getVideoStream(), &DATVideostream::fifoData, this, &DATVDemodGUI::on_StreamDataAvailable);
    } else {
        connect(m_datvDemod->getUDPStream(), &DATVUDPStream::fifoData, this, &DATVDemodGUI::on_StreamDataAvailable);
    }

    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setRollupState(&m_rollupState);

    connect(ui->screenTV_2, &DATVideoRender::onMetaDataChanged, this, &DATVDemodGUI::on_StreamMetaDataChanged);

    m_intPreviousDecodedData=0;
    m_intLastDecodedData=0;
    m_intLastSpeed=0;
    m_intReadyDecodedData=0;
    m_objTimer.setInterval(1000);
    connect(&m_objTimer, SIGNAL(timeout()), this, SLOT(tick()));
    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tickMeter())); // 50 ms
    m_objTimer.start();

    ui->fullScreen->setVisible(false);

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 8, -99999999, 99999999);

    ui->rfBandwidth->setColorMapper(ColorMapper(ColorMapper::GrayYellow));
    ui->rfBandwidth->setValueRange(true, 8, 0, 50000000);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(Qt::magenta);
    m_channelMarker.setBandwidth(6000000);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("DATV Demodulator");
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true);

    connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()), this, SLOT(channelMarkerHighlightedByCursor()));

    m_deviceUISet->addChannelMarker(&m_channelMarker);

    // QPixmap pixmapTarget = QPixmap(":/film.png");
    // pixmapTarget = pixmapTarget.scaled(16, 16, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    // ui->videoPlay->setAlignment(Qt::AlignCenter);
    // ui->videoPlay->setPixmap(pixmapTarget);

	CRightClickEnabler *audioMuteRightClickEnabler = new CRightClickEnabler(ui->audioMute);
	connect(audioMuteRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(audioSelect()));

    CRightClickEnabler *ldpcToolRightClickEnabler = new CRightClickEnabler(ui->softLDPC);
    connect(ldpcToolRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(ldpcToolSelect()));

    ui->playerIndicator->setStyleSheet("QLabel { background-color: gray; border-radius: 8px; }");
    ui->udpIndicator->setStyleSheet("QLabel { background-color: gray; border-radius: 8px; }");
    resetToDefaults(); // does applySettings()
    makeUIConnections();
}

DATVDemodGUI::~DATVDemodGUI()
{
    ui->screenTV->setParent(nullptr);   // Prefer memory leak to core dump... ~TVScreen() is buggy
    ui->screenTV_2->setParent(nullptr); // Prefer memory leak to core dump... ~DATVideoRender() is buggy
    delete ui;
}

void DATVDemodGUI::blockApplySettings(bool blnBlock)
{
    m_blnDoApplySettings = !blnBlock;
}

void DATVDemodGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_centerFrequency);
    m_channelMarker.setColor(m_settings.m_rgbColor);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);

    blockApplySettings(true);

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());

    ui->deltaFrequency->setValue(m_settings.m_centerFrequency);
    ui->chkAllowDrift->setChecked(m_settings.m_allowDrift);
    ui->chkHardMetric->setChecked(m_settings.m_hardMetric);
    ui->chkFastlock->setChecked(m_settings.m_fastLock);
    ui->chkViterbi->setChecked(m_settings.m_viterbi);
    ui->softLDPC->setChecked(m_settings.m_softLDPC);
    ui->maxBitflips->setValue(m_settings.m_maxBitflips);
    ui->datvStdSR->setValue(indexFromSymbolRate(m_settings.m_symbolRate));

    if (m_settings.m_standard == DATVDemodSettings::dvb_version::DVB_S)
    {
        ui->chkAllowDrift->setEnabled(true);
        ui->chkHardMetric->setEnabled(true);
        ui->chkFastlock->setEnabled(true);
        ui->chkViterbi->setEnabled(true);
        ui->maxBitflips->setEnabled(false);
        ui->chkAllowDrift->setStyleSheet("QCheckBox { color: white }");
        ui->chkHardMetric->setStyleSheet("QCheckBox { color: white }");
        ui->chkFastlock->setStyleSheet("QCheckBox { color: white }");
        ui->chkViterbi->setStyleSheet("QCheckBox { color: white }");
        ui->maxBitflips->setStyleSheet("QSpinBox { color: gray }");
        ui->maxBitflipsLabel->setStyleSheet("QLabel { color: gray }");
    }
    else
    {
        ui->chkAllowDrift->setEnabled(false);
        ui->chkHardMetric->setEnabled(false);
        ui->chkFastlock->setEnabled(false);
        ui->chkViterbi->setEnabled(false);
        ui->maxBitflips->setEnabled(true);
        ui->chkAllowDrift->setStyleSheet("QCheckBox { color: gray }");
        ui->chkHardMetric->setStyleSheet("QCheckBox { color: gray }");
        ui->chkFastlock->setStyleSheet("QCheckBox { color: gray }");
        ui->chkViterbi->setStyleSheet("QCheckBox { color: gray }");
        ui->maxBitflips->setStyleSheet("QSpinBox { color: white }");
        ui->maxBitflipsLabel->setStyleSheet("QLabel { color: white }");
    }

    if (m_settings.m_standard == DATVDemodSettings::dvb_version::DVB_S)
    {
        ui->softLDPC->setEnabled(false);
        ui->softLDPC->setStyleSheet("QCheckBox { color: gray }");
    }
    else
    {
        ui->softLDPC->setEnabled(true);
        ui->softLDPC->setStyleSheet("QCheckBox { color: white }");
    }

    if (m_settings.m_standard == DATVDemodSettings::dvb_version::DVB_S)
    {
        ui->statusText->clear();
        ui->statusText->setStyleSheet("QLabel { background:rgb(79,79,79); }");
    }

    ui->cmbFilter->setCurrentIndex((int) m_settings.m_filter);
    displayRRCParameters(((int) m_settings.m_filter == 2));

    ui->spiRollOff->setValue((int) roundf(m_settings.m_rollOff * 100.0f));
    ui->audioMute->setChecked(m_settings.m_audioMute);
    displaySystemConfiguration();
    ui->cmbStandard->setCurrentIndex((int) m_settings.m_standard);
    ui->spiNotchFilters->setValue(m_settings.m_notchFilters);
    ui->rfBandwidth->setValue(m_settings.m_rfBandwidth);
    ui->spiSymbolRate->setValue(m_settings.m_symbolRate);
    ui->spiExcursion->setValue(m_settings.m_excursion);
    ui->audioVolume->setValue(m_settings.m_audioVolume);
    ui->audioVolumeText->setText(tr("%1").arg(m_settings.m_audioVolume));
    ui->videoMute->setChecked(m_settings.m_videoMute);
    ui->udpTS->setChecked(m_settings.m_udpTS);
    ui->udpTSAddress->setText(m_settings.m_udpTSAddress);
    ui->udpTSPort->setText(tr("%1").arg(m_settings.m_udpTSPort));
    ui->playerEnable->setChecked(m_settings.m_playerEnable);

    if (m_settings.m_playerEnable)
    {
        disconnect(m_datvDemod->getUDPStream(), &DATVUDPStream::fifoData, this, &DATVDemodGUI::on_StreamDataAvailable);
        connect(m_datvDemod->getVideoStream(), &DATVideostream::fifoData, this, &DATVDemodGUI::on_StreamDataAvailable);
    }
    else
    {
        disconnect(m_datvDemod->getVideoStream(), &DATVideostream::fifoData, this, &DATVDemodGUI::on_StreamDataAvailable);
        connect(m_datvDemod->getUDPStream(), &DATVUDPStream::fifoData, this, &DATVDemodGUI::on_StreamDataAvailable);
    }

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    updateIndexLabel();
    blockApplySettings(false);
}

void DATVDemodGUI::displaySystemConfiguration()
{
    ui->cmbModulation->blockSignals(true);
    ui->cmbFEC->blockSignals(true);

    std::vector<DATVDemodSettings::DATVModulation> modulations;
    DATVDemodSettings::getAvailableModulations(m_settings.m_standard, modulations);
    std::vector<DATVDemodSettings::DATVCodeRate> codeRates;
    DATVDemodSettings::getAvailableCodeRates(m_settings.m_standard, m_settings.m_modulation, codeRates);

    ui->cmbModulation->clear();
    int modulationIndex = 0;
    int i;
    std::vector<DATVDemodSettings::DATVModulation>::const_iterator mIt = modulations.begin();

    for (i = 0; mIt != modulations.end(); ++mIt, i++)
    {
        ui->cmbModulation->addItem(DATVDemodSettings::getStrFromModulation(*mIt));

        if (m_settings.m_modulation == *mIt) {
            modulationIndex = i;
        }
    }

    ui->cmbFEC->clear();
    int rateIndex = 0;
    std::vector<DATVDemodSettings::DATVCodeRate>::const_iterator rIt = codeRates.begin();

    for (i = 0; rIt != codeRates.end(); ++rIt, i++)
    {
        ui->cmbFEC->addItem(DATVDemodSettings::getStrFromCodeRate(*rIt));

        if (m_settings.m_fec == *rIt) {
            rateIndex = i;
        }
    }

    ui->cmbModulation->setCurrentIndex(modulationIndex);
    ui->cmbFEC->setCurrentIndex(rateIndex);

    ui->cmbModulation->blockSignals(false);
    ui->cmbFEC->blockSignals(false);
}

void DATVDemodGUI::applySettings(bool force)
{
    if (m_blnDoApplySettings)
    {
        qDebug("DATVDemodGUI::applySettings");

        setTitleColor(m_channelMarker.getColor());

        QString msg = tr("DATVDemodGUI::applySettings: force: %1").arg(force ? "true" : "false");
        m_settings.debug(msg);

        DATVDemod::MsgConfigureDATVDemod* message = DATVDemod::MsgConfigureDATVDemod::create(m_settings, force);
	    m_datvDemod->getInputMessageQueue()->push(message);
    }
}

void DATVDemodGUI::leaveEvent(QEvent* event)
{
    blockApplySettings(true);
    m_channelMarker.setHighlighted(false);
    blockApplySettings(false);
    ChannelGUI::leaveEvent(event);
}

void DATVDemodGUI::enterEvent(EnterEventType* event)
{
    blockApplySettings(true);
    m_channelMarker.setHighlighted(true);
    blockApplySettings(false);
    ChannelGUI::enterEvent(event);
}

void DATVDemodGUI::audioSelect()
{
    qDebug("DATVDemodGUI::audioSelect");
    AudioSelectDialog audioSelect(DSPEngine::instance()->getAudioDeviceManager(), m_settings.m_audioDeviceName);
    audioSelect.exec();

    if (audioSelect.m_selected)
    {
        m_settings.m_audioDeviceName = audioSelect.m_audioDeviceName;
        applySettings();
    }
}

void DATVDemodGUI::ldpcToolSelect()
{
    qDebug("DATVDemodGUI::ldpcToolSelect");
    DatvDvbS2LdpcDialog ldpcDialog;
    ldpcDialog.setFileName(m_settings.m_softLDPCToolPath);
    ldpcDialog.setMaxTrials(m_settings.m_softLDPCMaxTrials);

    if (ldpcDialog.exec() == QDialog::Accepted)
    {
        m_settings.m_softLDPCMaxTrials = ldpcDialog.getMaxTrials();
        m_settings.m_softLDPCToolPath = ldpcDialog.getFileName();
        applySettings();
    }
}

void DATVDemodGUI::tick()
{
    if (m_datvDemod)
    {
        m_objMagSqAverage(m_datvDemod->getMagSq());
        double magSqDB = CalcDb::dbPower(m_objMagSqAverage / (SDR_RX_SCALED*SDR_RX_SCALED));
        ui->channePowerText->setText(tr("%1 dB").arg(magSqDB, 0, 'f', 1));

        if ((m_modcodModulationIndex != m_datvDemod->getModcodModulation()) || (m_modcodCodeRateIndex != m_datvDemod->getModcodCodeRate()))
        {
            m_modcodModulationIndex = m_datvDemod->getModcodModulation();
            m_modcodCodeRateIndex = m_datvDemod->getModcodCodeRate();
            DATVDemodSettings::DATVModulation modulation = DATVDemodSettings::getModulationFromLeanDVBCode(m_modcodModulationIndex);
            DATVDemodSettings::DATVCodeRate rate = DATVDemodSettings::getCodeRateFromLeanDVBCode(m_modcodCodeRateIndex);
            QString modcodModulationStr = DATVDemodSettings::getStrFromModulation(modulation);
            QString modcodCodeRateStr = DATVDemodSettings::getStrFromCodeRate(rate);
            ui->statusText->setText(tr("MCOD %1 %2").arg(modcodModulationStr).arg(modcodCodeRateStr));
        }

        if (m_cstlnSetByModcod != m_datvDemod->isCstlnSetByModcod())
        {
            m_cstlnSetByModcod = m_datvDemod->isCstlnSetByModcod();

            if (m_cstlnSetByModcod) {
                ui->statusText->setStyleSheet("QLabel { background-color : green; }");
            } else {
                ui->statusText->setStyleSheet("QLabel { background:rgb(79,79,79); }");
            }
        }
    }

    if((m_intLastDecodedData-m_intPreviousDecodedData)>=0)
    {
        m_intLastSpeed = 8*(m_intLastDecodedData-m_intPreviousDecodedData);
        ui->lblRate->setText(QString("Speed: %1b/s").arg(formatBytes(m_intLastSpeed)));
    }

    if (m_datvDemod->audioActive())
    {
        if (m_datvDemod->audioDecodeOK()) {
            ui->audioMute->setStyleSheet("QToolButton { background-color : green; }");
        } else {
            ui->audioMute->setStyleSheet("QToolButton { background-color : red; }");
        }
    }
    else
    {
        ui->audioMute->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
    }

    if (m_datvDemod->videoActive())
    {
		if (m_datvDemod->videoDecodeOK()) {
			ui->videoMute->setStyleSheet("QToolButton { background-color : green; }");
		} else {
			ui->videoMute->setStyleSheet("QToolButton { background-color : red; }");
		}
    }
    else
    {
        ui->videoMute->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
    }

    m_intPreviousDecodedData = m_intLastDecodedData;

    //Try to start video rendering
    bool success = m_datvDemod->playVideo();

    if (success) {
        ui->playerIndicator->setStyleSheet("QLabel { background-color: rgb(85, 232, 85); border-radius: 8px; }"); // green
    } else {
        ui->playerIndicator->setStyleSheet("QLabel { background-color: gray; border-radius: 8px; }");
    }

    if (m_datvDemod->udpRunning()) {
        ui->udpIndicator->setStyleSheet("QLabel { background-color: rgb(85, 232, 85); border-radius: 8px; }"); // green
    } else {
        ui->udpIndicator->setStyleSheet("QLabel { background-color: gray; border-radius: 8px; }");
    }
}

void DATVDemodGUI::tickMeter()
{
    ui->merMeter->levelChanged(m_datvDemod->getMERRMS(), m_datvDemod->getMERPeak(), m_datvDemod->getMERNbAvg());
    ui->cnrMeter->levelChanged(m_datvDemod->getCNRRMS(), m_datvDemod->getCNRPeak(), m_datvDemod->getCNRNbAvg());
    ui->merText->setText(QString("%1").arg(m_datvDemod->getMERAvg(), 0, 'f', 1));
    ui->cnrText->setText(QString("%1").arg(m_datvDemod->getCNRAvg(), 0, 'f', 1));
}

void DATVDemodGUI::on_cmbStandard_currentIndexChanged(int index)
{
    m_settings.m_standard = (DATVDemodSettings::dvb_version) index;

    if (m_settings.m_standard == DATVDemodSettings::dvb_version::DVB_S)
    {
        ui->chkAllowDrift->setEnabled(true);
        ui->chkHardMetric->setEnabled(true);
        ui->chkFastlock->setEnabled(true);
        ui->chkViterbi->setEnabled(true);
        ui->maxBitflips->setEnabled(false);
        ui->chkAllowDrift->setStyleSheet("QCheckBox { color: white }");
        ui->chkHardMetric->setStyleSheet("QCheckBox { color: white }");
        ui->chkFastlock->setStyleSheet("QCheckBox { color: white }");
        ui->chkViterbi->setStyleSheet("QCheckBox { color: white }");
        ui->maxBitflips->setStyleSheet("QSpinBox { color: gray }");
        ui->maxBitflipsLabel->setStyleSheet("QLabel { color: gray }");
    }
    else
    {
        ui->chkAllowDrift->setEnabled(false);
        ui->chkHardMetric->setEnabled(false);
        ui->chkFastlock->setEnabled(false);
        ui->chkViterbi->setEnabled(false);
        ui->maxBitflips->setEnabled(true);
        ui->chkAllowDrift->setStyleSheet("QCheckBox { color: gray }");
        ui->chkHardMetric->setStyleSheet("QCheckBox { color: gray }");
        ui->chkFastlock->setStyleSheet("QCheckBox { color: gray }");
        ui->chkViterbi->setStyleSheet("QCheckBox { color: gray }");
        ui->maxBitflips->setStyleSheet("QSpinBox { color: white }");
        ui->maxBitflipsLabel->setStyleSheet("QLabel { color: white }");
    }

    if (m_settings.m_standard == DATVDemodSettings::dvb_version::DVB_S)
    {
        ui->softLDPC->setEnabled(false);
        ui->softLDPC->setStyleSheet("QCheckBox { color: gray }");
    }
    else
    {
        ui->softLDPC->setEnabled(true);
        ui->softLDPC->setStyleSheet("QCheckBox { color: white }");
    }

    if (m_settings.m_standard == DATVDemodSettings::dvb_version::DVB_S)
    {
        ui->statusText->clear();
        ui->statusText->setStyleSheet("QLabel { background:rgb(79,79,79); }");
    }

    m_settings.validateSystemConfiguration();
    displaySystemConfiguration();
    applySettings();
}

void DATVDemodGUI::on_cmbModulation_currentIndexChanged(int arg1)
{
    (void) arg1;
    QString strModulation = ui->cmbModulation->currentText();
    m_settings.m_modulation = DATVDemodSettings::getModulationFromStr(strModulation);
    m_settings.validateSystemConfiguration();
    displaySystemConfiguration();

    //Viterbi only for BPSK and QPSK
    if ((m_settings.m_modulation != DATVDemodSettings::BPSK)
        && (m_settings.m_modulation != DATVDemodSettings::QPSK))
    {
        ui->chkViterbi->setChecked(false);
    }

    applySettings();
}

void DATVDemodGUI::on_cmbFEC_currentIndexChanged(int arg1)
{
    (void) arg1;
    QString strFEC = ui->cmbFEC->currentText();
    m_settings.m_fec = DATVDemodSettings::getCodeRateFromStr(strFEC);
    applySettings();
}

void DATVDemodGUI::on_softLDPC_clicked()
{
    m_settings.m_softLDPC = ui->softLDPC->isChecked();
    applySettings();
}

void DATVDemodGUI::on_maxBitflips_valueChanged(int value)
{
    m_settings.m_maxBitflips = value;
    applySettings();
}

void DATVDemodGUI::on_chkViterbi_clicked()
{
    m_settings.m_viterbi = ui->chkViterbi->isChecked();
    applySettings();
}

void DATVDemodGUI::on_chkHardMetric_clicked()
{
    m_settings.m_hardMetric = ui->chkHardMetric->isChecked();
    applySettings();
}

void DATVDemodGUI::on_resetDefaults_clicked()
{
    resetToDefaults();
}

void DATVDemodGUI::on_spiSymbolRate_valueChanged(int value)
{
    m_settings.m_symbolRate = value;
    ui->datvStdSR->blockSignals(true);
    ui->datvStdSR->setValue(indexFromSymbolRate(value));
    ui->datvStdSR->blockSignals(false);
    applySettings();
}

void DATVDemodGUI::on_datvStdSR_valueChanged(int value)
{
    m_settings.m_symbolRate = symbolRateFromIndex(value);
    ui->spiSymbolRate->blockSignals(true);
    ui->spiSymbolRate->setValue(m_settings.m_symbolRate);
    ui->spiSymbolRate->blockSignals(false);
    applySettings();
}

void DATVDemodGUI::on_spiNotchFilters_valueChanged(int value)
{
    m_settings.m_notchFilters = value;
    applySettings();
}

void DATVDemodGUI::on_chkAllowDrift_clicked()
{
    m_settings.m_allowDrift = ui->chkAllowDrift->isChecked();
    applySettings();
}

void DATVDemodGUI::on_fullScreen_clicked()
{
    ui->screenTV_2->setFullScreen(true);
}

void DATVDemodGUI::on_mouseEvent(QMouseEvent* obj)
{
    (void) obj;
}

QString DATVDemodGUI::formatBytes(qint64 intBytes)
{
    if(intBytes<1024) {
        return QString("%1").arg(intBytes);
    } else if(intBytes<1024*1024) {
        return QString("%1 K").arg((float)(10*intBytes/1024)/10.0f);
    } else if(intBytes<1024*1024*1024) {
        return QString("%1 M").arg((float)(10*intBytes/(1024*1024))/10.0f);
    }

    return QString("%1 G").arg((float)(10*intBytes/(1024*1024*1024))/10.0f);
}


void DATVDemodGUI::on_StreamDataAvailable(int intBytes, int intPercent, qint64 intTotalReceived)
{
    ui->lblStatus->setText(QString("Data: %1B").arg(formatBytes(intTotalReceived)));
    m_intLastDecodedData = intTotalReceived;

    if ((intPercent)<100) {
        ui->prgSynchro->setValue(intPercent);
    } else {
        ui->prgSynchro->setValue(100);
    }

    m_intReadyDecodedData = intBytes;
}

void DATVDemodGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_centerFrequency = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings();
}

void DATVDemodGUI::on_rfBandwidth_changed(qint64 value)
{
    m_channelMarker.setBandwidth(value);
    m_settings.m_rfBandwidth = m_channelMarker.getBandwidth();
    applySettings();
}

void DATVDemodGUI::on_chkFastlock_clicked()
{
    m_settings.m_fastLock = ui->chkFastlock->isChecked();
    applySettings();
}

void DATVDemodGUI::on_audioMute_toggled(bool checked)
{
    m_settings.m_audioMute = checked;
	applySettings();
}

void DATVDemodGUI::on_videoMute_toggled(bool checked)
{
    m_settings.m_videoMute = checked;
	applySettings();
}

void DATVDemodGUI::on_audioVolume_valueChanged(int value)
{
    ui->audioVolumeText->setText(tr("%1").arg(value));
    m_settings.m_audioVolume = value;
    applySettings();
}

void DATVDemodGUI::on_udpTS_clicked(bool checked)
{
    m_settings.m_udpTS = checked;
    applySettings();
}

void DATVDemodGUI::on_StreamMetaDataChanged(DataTSMetaData2 *objMetaData)
{

    if (objMetaData)
    {
        QString strMetaData = "";

        if (objMetaData->OK_TransportStream == true)
        {
            strMetaData = tr("PID: %1 - Width: %2 - Height: %3\r\n%4%5\r\nCodec: %6\r\n")
                .arg(objMetaData->PID)
                .arg(objMetaData->Width)
                .arg(objMetaData->Height)
                .arg(objMetaData->Program)
                .arg(objMetaData->Stream)
                .arg(objMetaData->CodecDescription);
        }

        ui->streamInfo->setText(strMetaData);
        ui->chkData->setChecked(objMetaData->OK_Data);
        ui->chkTS->setChecked(objMetaData->OK_TransportStream);
        ui->chkVS->setChecked(objMetaData->OK_VideoStream);
        ui->chkDecoding->setChecked(objMetaData->OK_Decoding);

        if (objMetaData->Height > 0) {
            ui->screenTV_2->setFixedWidth((int)objMetaData->Width*(270.0f/(float)objMetaData->Height));
        }

        delete objMetaData;
    }
}

void DATVDemodGUI::on_playerEnable_clicked()
{
    m_settings.m_playerEnable = ui->playerEnable->isChecked();

    if (m_settings.m_playerEnable)
    {
        disconnect(m_datvDemod->getUDPStream(), &DATVUDPStream::fifoData, this, &DATVDemodGUI::on_StreamDataAvailable);
        connect(m_datvDemod->getVideoStream(), &DATVideostream::fifoData, this, &DATVDemodGUI::on_StreamDataAvailable);
    }
    else
    {
        disconnect(m_datvDemod->getVideoStream(), &DATVideostream::fifoData, this, &DATVDemodGUI::on_StreamDataAvailable);
        connect(m_datvDemod->getUDPStream(), &DATVUDPStream::fifoData, this, &DATVDemodGUI::on_StreamDataAvailable);
    }

    applySettings();
}

void DATVDemodGUI::displayRRCParameters(bool blnVisible)
{
    ui->spiRollOff->setVisible(blnVisible);
    ui->spiExcursion->setVisible(blnVisible);
    ui->rollOffLabel->setVisible(blnVisible);
    ui->excursionLabel->setVisible(blnVisible);
}

void DATVDemodGUI::on_cmbFilter_currentIndexChanged(int index)
{
    if (index == 0) {
        m_settings.m_filter = DATVDemodSettings::SAMP_LINEAR;
    } else if (index == 1) {
        m_settings.m_filter = DATVDemodSettings::SAMP_NEAREST;
    } else {
        m_settings.m_filter = DATVDemodSettings::SAMP_RRC;
    }

    displayRRCParameters(index == 2);
    applySettings();
}

void DATVDemodGUI::on_spiRollOff_valueChanged(int value)
{
    m_settings.m_rollOff = ((float) value) / 100.0f;
    applySettings();
}

void DATVDemodGUI::on_spiExcursion_valueChanged(int value)
{
    m_settings.m_excursion = value;
    applySettings();
}

void DATVDemodGUI::on_udpTSAddress_editingFinished()
{
    m_settings.m_udpTSAddress = ui->udpTSAddress->text();
    applySettings();
}

void DATVDemodGUI::on_udpTSPort_editingFinished()
{
    bool ok;
    quint16 udpPort = ui->udpTSPort->text().toInt(&ok);

    if((!ok) || (udpPort < 1024)) {
        udpPort = 8882;
    }

    m_settings.m_udpTSPort = udpPort;
    ui->udpTSPort->setText(tr("%1").arg(udpPort));
    applySettings();
}

void DATVDemodGUI::makeUIConnections()
{
    QObject::connect(ui->cmbStandard, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DATVDemodGUI::on_cmbStandard_currentIndexChanged);
    QObject::connect(ui->cmbModulation, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DATVDemodGUI::on_cmbModulation_currentIndexChanged);
    QObject::connect(ui->cmbFEC, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DATVDemodGUI::on_cmbFEC_currentIndexChanged);
    QObject::connect(ui->softLDPC, &QCheckBox::clicked, this, &DATVDemodGUI::on_softLDPC_clicked);
    QObject::connect(ui->maxBitflips, QOverload<int>::of(&QSpinBox::valueChanged), this, &DATVDemodGUI::on_maxBitflips_valueChanged);
    QObject::connect(ui->chkViterbi, &QCheckBox::clicked, this, &DATVDemodGUI::on_chkViterbi_clicked);
    QObject::connect(ui->chkHardMetric, &QCheckBox::clicked, this, &DATVDemodGUI::on_chkHardMetric_clicked);
    QObject::connect(ui->resetDefaults, &QPushButton::clicked, this, &DATVDemodGUI::on_resetDefaults_clicked);
    QObject::connect(ui->spiSymbolRate, QOverload<int>::of(&QSpinBox::valueChanged), this, &DATVDemodGUI::on_spiSymbolRate_valueChanged);
    QObject::connect(ui->datvStdSR, &QDial::valueChanged, this, &DATVDemodGUI::on_datvStdSR_valueChanged);
    QObject::connect(ui->spiNotchFilters, QOverload<int>::of(&QSpinBox::valueChanged), this, &DATVDemodGUI::on_spiNotchFilters_valueChanged);
    QObject::connect(ui->chkAllowDrift, &QCheckBox::clicked, this, &DATVDemodGUI::on_chkAllowDrift_clicked);
    QObject::connect(ui->fullScreen, &QPushButton::clicked, this, &DATVDemodGUI::on_fullScreen_clicked);
    QObject::connect(ui->chkFastlock, &QCheckBox::clicked, this, &DATVDemodGUI::on_chkFastlock_clicked);
    QObject::connect(ui->cmbFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DATVDemodGUI::on_cmbFilter_currentIndexChanged);
    QObject::connect(ui->spiRollOff, QOverload<int>::of(&QSpinBox::valueChanged), this, &DATVDemodGUI::on_spiRollOff_valueChanged);
    QObject::connect(ui->spiExcursion, QOverload<int>::of(&QSpinBox::valueChanged), this, &DATVDemodGUI::on_spiExcursion_valueChanged);
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &DATVDemodGUI::on_deltaFrequency_changed);
    QObject::connect(ui->rfBandwidth, &ValueDialZ::changed, this, &DATVDemodGUI::on_rfBandwidth_changed);
    QObject::connect(ui->audioMute, &QToolButton::toggled, this, &DATVDemodGUI::on_audioMute_toggled);
    QObject::connect(ui->audioVolume, &QSlider::valueChanged, this, &DATVDemodGUI::on_audioVolume_valueChanged);
    QObject::connect(ui->videoMute, &QToolButton::toggled, this, &DATVDemodGUI::on_videoMute_toggled);
    QObject::connect(ui->udpTS, &ButtonSwitch::clicked, this, &DATVDemodGUI::on_udpTS_clicked);
    QObject::connect(ui->udpTSAddress, &QLineEdit::editingFinished, this, &DATVDemodGUI::on_udpTSAddress_editingFinished);
    QObject::connect(ui->udpTSPort, &QLineEdit::editingFinished, this, &DATVDemodGUI::on_udpTSPort_editingFinished);
    QObject::connect(ui->playerEnable, &QCheckBox::clicked, this, &DATVDemodGUI::on_playerEnable_clicked);
}

void DATVDemodGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_centerFrequency);
}

int DATVDemodGUI::indexFromSymbolRate(int sampleRate)
{
    int index  = 0;

    if (sampleRate < m_symbolRates[1]) {
        return index;
    }

    for (auto definedRate : m_symbolRates)
    {
        if (sampleRate <= definedRate) {
            return index;
        }

        index++;
    }

    return index;
}

int DATVDemodGUI::symbolRateFromIndex(int index)
{
    return m_symbolRates.at(index);
}
