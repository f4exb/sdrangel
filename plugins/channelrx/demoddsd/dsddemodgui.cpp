///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2015 John Greb <hexameron@spam.no>                              //
// Copyright (C) 2021-2023 Jon Beniston, M7RCE <jon@beniston.com>                //
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

#include <QFileDialog>
#include <QLocale>

#include "dsddemodgui.h"

#include "device/deviceuiset.h"

#include "ui_dsddemodgui.h"
#include "dsp/scopevisxy.h"
#include "dsp/dspcommands.h"
#include "plugin/pluginapi.h"
#include "util/db.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/crightclickenabler.h"
#include "gui/audioselectdialog.h"
#include "gui/dialogpositioner.h"
#include "dsp/dspengine.h"
#include "maincore.h"

#include "dsddemod.h"
#include "dsdvocodermbelib.h"

#include <algorithm>
#include <cmath>
#include <QDockWidget>
#include <QMainWindow>
#include <QDebug>
#include <QScrollBar>

#include <complex>

#include "dsp/dv/dvangel61.h"

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
	applySettings(QStringList(), true);
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
        applySettings(QStringList(), true);
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
        if (m_basebandSampleRate < 48000) {
            setStatusText(QString("Sample rate must be >= 48000 Hz (Currently %1 Hz)").arg(m_basebandSampleRate));
        } else {
            setStatusText("");
        }
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

    while ((message = getInputMessageQueue()->pop()))
    {
        if (!handleMessage(*message)) {
            qDebug("%s: unhandled message: %s", Q_FUNC_INFO, message->getIdentifier());
        }
        delete message;
    }
}

void DSDDemodGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings(QStringList({"inputFrequencyOffset"}));
}

void DSDDemodGUI::on_rfBW_valueChanged(int value)
{
	m_channelMarker.setBandwidth(value * 100);
	m_settings.m_rfBandwidth = value * 100.0;
    ui->rfBWText->setText(QString("%1k").arg(value / 10.0, 0, 'f', 1));
	applySettings(QStringList({"rfBandwidth"}));
}

void DSDDemodGUI::on_volume_valueChanged(int value)
{
    m_settings.m_volume= value / 10.0;
    ui->volumeText->setText(QString("%1").arg(value / 10.0, 0, 'f', 1));
    applySettings(QStringList({"volume"}));
}

void DSDDemodGUI::on_protocol_currentIndexChanged(int index)
{
    m_settings.m_protocol = ((index >= 0) && (index < DSDDemodSettings::NbProtocols)) ? index : DSDDemodSettings::ProtocolAuto;
    displayP25P2System();
    applySettings(QStringList({"protocol"}));
}

void DSDDemodGUI::on_p25p2System_editingFinished()
{
    // WACN:SYSID:NAC in hexadecimal
    QStringList parts = ui->p25p2System->text().split(':');
    bool ok = parts.size() == 3;
    int values[3] = { 0, 0, 0 };
    static const int masks[3] = { 0xFFFFF, 0xFFF, 0xFFF };

    for (int i = 0; ok && (i < 3); i++)
    {
        bool okPart;
        values[i] = parts[i].trimmed().toInt(&okPart, 16);
        ok = okPart && (values[i] >= 0) && (values[i] <= masks[i]);
    }

    if (ok)
    {
        m_settings.m_p25p2Wacn = values[0];
        m_settings.m_p25p2Sysid = values[1];
        m_settings.m_p25p2Nac = values[2];
        applySettings(QStringList({"p25p2Wacn", "p25p2Sysid", "p25p2Nac"}));
    }

    displayP25P2System();
}

void DSDDemodGUI::on_coherent_toggled(bool checked)
{
    m_settings.m_coherent = checked;
    applySettings(QStringList({"coherent"}));
}

void DSDDemodGUI::displayP25P2System()
{
    // only P25 Phase 2 (selected, or found in Auto) uses it
    const bool p25p2 = (m_settings.m_protocol == DSDDemodSettings::ProtocolAuto) || (m_settings.m_protocol == DSDDemodSettings::ProtocolP25P2);
    ui->p25p2SystemLabel->setEnabled(p25p2);
    ui->p25p2System->setEnabled(p25p2);
    ui->p25p2System->setText(QString("%1:%2:%3")
        .arg(m_settings.m_p25p2Wacn, 5, 16, QChar('0'))
        .arg(m_settings.m_p25p2Sysid, 3, 16, QChar('0'))
        .arg(m_settings.m_p25p2Nac, 3, 16, QChar('0')).toUpper());
}

void DSDDemodGUI::on_traceLength_valueChanged(int value)
{
    m_settings.m_traceLengthMutliplier = value;
    ui->traceLengthText->setText(QString("%1").arg(m_settings.m_traceLengthMutliplier*50));
    updateScopeTraceLength();
    applySettings(QStringList({"traceLengthMutliplier"}));
}

void DSDDemodGUI::updateScopeTraceLength()
{
    const int traceMilliseconds = m_settings.m_traceLengthMutliplier * 50;
    m_scopeVisXY->setPixelsPerFrame(std::max(1, m_scopeSampleRate * traceMilliseconds / 1000));
}

void DSDDemodGUI::on_traceStroke_valueChanged(int value)
{
    m_settings.m_traceStroke = value;
    ui->traceStrokeText->setText(QString("%1").arg(m_settings.m_traceStroke));
    m_scopeVisXY->setStroke(m_settings.m_traceStroke);
    applySettings(QStringList({"traceStroke"}));
}

void DSDDemodGUI::on_traceDecay_valueChanged(int value)
{
    m_settings.m_traceDecay = value;
    ui->traceDecayText->setText(QString("%1").arg(m_settings.m_traceDecay));
    m_scopeVisXY->setDecay(m_settings.m_traceDecay);
    applySettings(QStringList({"traceDecay"}));
}

void DSDDemodGUI::on_slot1On_toggled(bool checked)
{
    m_settings.m_slot1On = checked;
    applySettings(QStringList({"slot1On"}));
}

void DSDDemodGUI::on_slot2On_toggled(bool checked)
{
    m_settings.m_slot2On = checked;
    applySettings(QStringList({"slot2On"}));
}

void DSDDemodGUI::on_tdmaStereoSplit_toggled(bool checked)
{
    m_settings.m_tdmaStereo = checked;
    applySettings(QStringList({"tdmaStereo"}));
}

void DSDDemodGUI::on_dmrColorCode_currentIndexChanged(int index)
{
    if (index < 0) {
        return;
    }

    m_settings.m_dmrColorCode = ui->dmrColorCode->itemData(index).toInt();
    applySettings(QStringList({"dmrColorCode"}));
}

void DSDDemodGUI::on_audioMute_toggled(bool checked)
{
    m_settings.m_audioMute = checked;
    applySettings(QStringList({"audioMute"}));
}

void DSDDemodGUI::on_highPassFilter_toggled(bool checked)
{
    m_settings.m_highPassFilter = checked;
    applySettings(QStringList({"highPassFilter"}));
}

void DSDDemodGUI::on_vocoder_currentIndexChanged(int index)
{
    if (index < 0) {
        return;
    }

    const int feature = ui->vocoder->itemData(index).toInt();
    m_settings.m_connectAMBE = feature >= 0;

    if (feature >= 0) {
        m_settings.m_ambeFeatureIndex = feature;
    }

    applySettings(QStringList({"connectAMBE", "ambeFeatureIndex"}));
}

void DSDDemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings(QStringList());
}

void DSDDemodGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuType::ContextMenuChannelSettings)
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

        applySettings(QStringList({
            "title",
            "rgbColor",
            "useReverseAPI",
            "reverseAPIAddress",
            "reverseAPIPort",
            "reverseAPIDeviceIndex",
            "reverseAPIChannelIndex",
            "streamIndex"
        }));
    }

    resetContextMenuType();
}

void DSDDemodGUI::on_angel61FileDirectory_editingFinished()
{
    m_settings.m_angel61FileDirectory = ui->angel61FileDirectory->text();
    applySettings(QStringList({"angel61FileDirectory"}));
}

void DSDDemodGUI::on_angel61FileDirectoryBrowse_clicked()
{
    QString directory = QFileDialog::getExistingDirectory(this, tr("Directory for received Angel61 files"),
        m_settings.m_angel61FileDirectory);

    if (!directory.isEmpty())
    {
        m_settings.m_angel61FileDirectory = directory;
        ui->angel61FileDirectory->setText(directory);
        applySettings(QStringList({"angel61FileDirectory"}));
    }
}

void DSDDemodGUI::on_angel61FileConfirm_toggled(bool checked)
{
    m_settings.m_angel61FileConfirm = checked;
    applySettings(QStringList({"angel61FileConfirm"}));
}

void DSDDemodGUI::on_angel61FileAccept_clicked()
{
    m_dsdDemod->angel61FileDecision(true);
}

void DSDDemodGUI::on_angel61FileReject_clicked()
{
    m_dsdDemod->angel61FileDecision(false);
}

void DSDDemodGUI::on_angel61FileMaxKB_valueChanged(int value)
{
    m_settings.m_angel61FileMaxKB = value;
    applySettings(QStringList({"angel61FileMaxKB"}));
}

void DSDDemodGUI::on_angel61Groups_editingFinished()
{
    // Shown back as it is kept: the numbers, comma separated
    QStringList groups;

    for (uint32_t group : DVAngel61::parseTalkgroupList(ui->angel61Groups->text().toStdString())) {
        groups.append(QString::number(group));
    }

    m_settings.m_angel61Groups = groups.join(", ");
    ui->angel61Groups->setText(m_settings.m_angel61Groups);
    applySettings(QStringList({"angel61Groups"}));
}

void DSDDemodGUI::on_clearTextMessages_clicked()
{
    ui->textMessages->clear();
}

void DSDDemodGUI::on_viewStatusLog_clicked()
{
    qDebug("DSDDemodGUI::on_viewStatusLog_clicked");

    if (m_dsdStatusTextDialog.isMinimized()) {
        m_dsdStatusTextDialog.showNormal();
    } else {
        m_dsdStatusTextDialog.show();
    }

    m_dsdStatusTextDialog.raise();
    m_dsdStatusTextDialog.activateWindow();
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
	m_slot1On(false),
	m_slot2On(false),
	m_tdmaStereo(false),
	m_locked(false),
    m_scopeSampleRate(48000),
    m_audioSampleRate(-1),
	m_tickCount(0),
	m_dsdStatusTextDialog(0),
    m_lastLoggedEventSequence(0),
    m_lastMessageSequence(0)
{
	setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/demoddsd/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    ui->dmrColorCode->addItem(tr("Any"), -1);

    for (int colorCode = 0; colorCode <= 63; colorCode++) {   // dPMR and NXDN go to 63, DMR to 15
        ui->dmrColorCode->addItem(QString::number(colorCode), colorCode);
    }

    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));

	ui->screenTV->setColor(true);
	ui->screenTV->resizeTVScreen(200,200);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    CRightClickEnabler *audioMuteRightClickEnabler = new CRightClickEnabler(ui->audioMute);
    connect(audioMuteRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(audioSelect(const QPoint &)));

	m_scopeVisXY = new ScopeVisXY(ui->screenTV);
	m_scopeVisXY->setScale(1.0);
	m_scopeVisXY->setPixelsPerFrame(4001);
	m_scopeVisXY->setPlotRGB(qRgb(0, 220, 250));
	m_scopeVisXY->setGridRGB(qRgb(255, 255, 128));

	// The four nominal symbol levels: the outer level is 0.8 of the display, the inner a third of that
	const float levels[4] = { -0.8f, -0.8f / 3.0f, 0.8f / 3.0f, 0.8f };

	for (float x : levels)
	{
		for (float y : levels) {
			m_scopeVisXY->addGraticulePoint(std::complex<float>(x, y));
		}
	}

	m_scopeVisXY->calculateGraticule(200,200);

	m_dsdDemod = (DSDDemod*) rxChannel;
	m_dsdDemod->setScopeXYSink(m_scopeVisXY);
	m_dsdDemod->setMessageQueueToGUI(getInputMessageQueue());
	m_dsdDemod->getInputMessageQueue()->push(DSDDemod::MsgQueryAvailableAMBEFeatures::create());   // the features already open

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
	applySettings(QStringList(), true);
    m_resizer.enableChildMouseTracking();
}

DSDDemodGUI::~DSDDemodGUI()
{
	m_dsdDemod->setScopeXYSink(nullptr);
	delete m_scopeVisXY;
    ui->screenTV->setParent(nullptr); // Prefer memory leak to core dump... ~TVScreen() is buggy
	delete ui;
}

void DSDDemodGUI::updateMyPosition()
{
    float latitude = MainCore::instance()->getSettings().getLatitude();
    float longitude = MainCore::instance()->getSettings().getLongitude();

    if (!m_myPositionSent || (m_myLatitude != latitude) || (m_myLongitude != longitude))
    {
        m_dsdDemod->configureMyPosition(latitude, longitude);   // (the channel gives a new sink the preferences' itself)
        m_myLatitude = latitude;
        m_myLongitude = longitude;
        m_myPositionSent = true;
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

    ui->volume->setValue((int) std::lround(m_settings.m_volume * 10.0));
    ui->angel61FileDirectory->setText(m_settings.m_angel61FileDirectory);
    ui->angel61FileMaxKB->setValue(m_settings.m_angel61FileMaxKB);
    ui->angel61FileConfirm->setChecked(m_settings.m_angel61FileConfirm);
    ui->angel61Groups->setText(m_settings.m_angel61Groups);
    ui->volumeText->setText(QString("%1").arg(ui->volume->value() / 10.0, 0, 'f', 1));

    ui->slot1On->setChecked(m_settings.m_slot1On);
    ui->slot2On->setChecked(m_settings.m_slot2On);
    ui->tdmaStereoSplit->setChecked(m_settings.m_tdmaStereo);
    ui->dmrColorCode->setCurrentIndex(ui->dmrColorCode->findData(m_settings.m_dmrColorCode));
    ui->audioMute->setChecked(m_settings.m_audioMute);
    ui->highPassFilter->setChecked(m_settings.m_highPassFilter);

    ui->protocol->setCurrentIndex(m_settings.m_protocol);
    displayP25P2System();
    ui->contactReportEnabled->setChecked(m_settings.m_contactReportEnabled);
    ui->contactReportCallsign->setText(m_settings.m_contactReportCallsign);
    ui->coherent->setChecked(m_settings.m_coherent);

    ui->traceLength->setValue(m_settings.m_traceLengthMutliplier);
    ui->traceLengthText->setText(QString("%1").arg(m_settings.m_traceLengthMutliplier*50));
    updateScopeTraceLength();

    ui->traceStroke->setValue(m_settings.m_traceStroke);
    ui->traceStrokeText->setText(QString("%1").arg(m_settings.m_traceStroke));
    m_scopeVisXY->setStroke(m_settings.m_traceStroke);

    ui->traceDecay->setValue(m_settings.m_traceDecay);
    ui->traceDecayText->setText(QString("%1").arg(m_settings.m_traceDecay));
    m_scopeVisXY->setDecay(m_settings.m_traceDecay);

    updateAMBEFeaturesList();   // the AMBE feature the settings name, when it is there
    updateIndexLabel();
    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void DSDDemodGUI::updateAMBEFeaturesList()
{
    // One list of decoders: the software one, or "none" when it is not built in, then every AMBE
    // feature the instance has. Display only: the settings say which feature to connect to (the
    // channel connects whenever it is there and disconnects, keeping the settings, when it goes).
    // Nothing here changes the settings: the list arrives after a preset or the channel's settings,
    // and an empty one clearing them lost the connection a preset asked for
    ui->vocoder->blockSignals(true);
    ui->vocoder->clear();
    ui->vocoder->addItem(DSDVocoderMbelib::isAvailable() ? QString("mbevocoder") : tr("none"), -1);   // -1: not a feature

    for (int i = 0; i < m_availableAMBEFeatures.count(); i++)
    {
        ui->vocoder->addItem(tr("AMBE F:%1").arg(m_availableAMBEFeatures[i].m_featureIndex),
            m_availableAMBEFeatures[i].m_featureIndex);
    }

    int select = 0;

    if (m_settings.m_connectAMBE && (m_settings.m_ambeFeatureIndex >= 0))
    {
        select = ui->vocoder->findData(m_settings.m_ambeFeatureIndex);

        // A feature not here now (removed, or a preset loaded before it) is shown as asked for, not
        // replaced: the channel connects it again when it comes back
        if (select < 0)
        {
            ui->vocoder->addItem(tr("AMBE F:%1 (absent)").arg(m_settings.m_ambeFeatureIndex), m_settings.m_ambeFeatureIndex);
            select = ui->vocoder->count() - 1;
        }
    }

    ui->vocoder->setCurrentIndex(select);
    ui->vocoder->setEnabled(ui->vocoder->count() > 1);
    ui->vocoder->blockSignals(false);
}

void DSDDemodGUI::applySettings(const QStringList& settingsKeys, const bool force)
{
	if (m_doApplySettings)
	{
		qDebug() << "DSDDemodGUI::applySettings";

        DSDDemod::MsgConfigureDSDDemod* message = DSDDemod::MsgConfigureDSDDemod::create(settingsKeys, m_settings, force);
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
    applySettings(QStringList({"inputFrequencyOffset"}));
}

void DSDDemodGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void DSDDemodGUI::audioSelect(const QPoint& p)
{
    qDebug("DSDDemodGUI::audioSelect");
    AudioSelectDialog audioSelect(DSPEngine::instance()->getAudioDeviceManager(), m_settings.m_audioDeviceName);
    audioSelect.move(p);
    new DialogPositioner(&audioSelect, false);
    audioSelect.exec();

    if (audioSelect.m_selected)
    {
        m_settings.m_audioDeviceName = audioSelect.m_audioDeviceName;
        applySettings(QStringList({"audioDeviceName"}));
    }
}

void DSDDemodGUI::tick()
{
    updateMyPosition();   // the preferences' position, when it changes
    ui->contactReportStatus->setText(tr("Contact reporting: %1").arg(m_dsdDemod->contactReportStatus()));
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
    DSDDemodSink::Report report = m_dsdDemod->getReport();
	bool locked = m_dsdDemod->isRunning() && report.m_locked;

    if (!report.m_events.isEmpty() && (report.m_events.constLast().m_sequence < m_lastLoggedEventSequence)) {
        m_lastLoggedEventSequence = 0; // The baseband sink was restarted and its sequence began again
    }

    for (const DSDDemodSink::Event& event : report.m_events)
    {
        if (event.m_sequence > m_lastLoggedEventSequence)
        {
            if (ui->activateStatusLog->isChecked()) {
                m_dsdStatusTextDialog.addEvent(event.m_text);
            }

            m_lastLoggedEventSequence = event.m_sequence;
        }
    }

    // Text messages: one line each, the field scrolled to the last
    if (!report.m_messages.isEmpty() && (report.m_messages.constLast().m_sequence < m_lastMessageSequence)) {
        m_lastMessageSequence = 0;
    }

    for (const DSDDemodSink::TextMessage& message : report.m_messages)
    {
        if (message.m_sequence > m_lastMessageSequence)
        {
            ui->textMessages->appendPlainText(QString("%1 %2 > %3: %4").arg(message.m_protocol).arg(message.m_source).arg(message.m_destination).arg(message.m_text));
            ui->textMessages->verticalScrollBar()->setValue(ui->textMessages->verticalScrollBar()->maximum());
            m_lastMessageSequence = message.m_sequence;
        }
    }

    if (report.m_scopeSampleRate != m_scopeSampleRate)
    {
        m_scopeSampleRate = report.m_scopeSampleRate;
        updateScopeTraceLength();
    }

	if ((audioSampleRate != m_audioSampleRate) || (locked != m_locked))
	{
        if (audioSampleRate < 0) {
			ui->audioMute->setStyleSheet("QToolButton { background-color : red; }");
        } else if (locked) {
			ui->audioMute->setStyleSheet("QToolButton { background-color : green; }");
		} else {
			ui->audioMute->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
		}

        m_audioSampleRate = audioSampleRate;
        m_locked = locked;
	}

	// "slow" updates; once more when the channel stops, which shows it idle rather than as it was
	const bool running = m_dsdDemod->isRunning();

	if (!running)
	{
		report.m_slot1Voice = report.m_slot2Voice = report.m_locked = false;
		report.m_statusText.clear();
	}

	if ((running || m_statusShown) && (m_tickCount % 10 == 0))
	{
		m_statusShown = running;

        // An Angel61 file offer awaiting the operator (3.4)
        const bool pending = !report.m_angel61FilePending.isEmpty();
        ui->angel61FileAccept->setEnabled(pending);
        ui->angel61FileReject->setEnabled(pending);
        ui->angel61FileAccept->setToolTip(pending ? tr("Accept %1").arg(report.m_angel61FilePending)
            : tr("Accept the Angel61 file offer awaiting confirmation"));
        ui->angel61FileReject->setToolTip(pending ? tr("Refuse %1").arg(report.m_angel61FilePending)
            : tr("Refuse the Angel61 file offer awaiting confirmation (the sender is told)"));

        const QLocale locale;
        ui->angel61IPTraffic->setText(tr("Heard: %1 B (%2 datagrams)")
            .arg(locale.toString(report.m_angel61IPRxBytes))
            .arg(locale.toString(report.m_angel61IPRxPackets)));

        if (report.m_slot1Voice) {
            ui->slot1On->setStyleSheet("QToolButton { background-color : green; }");
        } else {
            ui->slot1On->setStyleSheet("QToolButton { background-color : rgb(79,79,79); }");
        }

        if (report.m_slot2Voice) {
            ui->slot2On->setStyleSheet("QToolButton { background-color : green; }");
        } else {
            ui->slot2On->setStyleSheet("QToolButton { background-color : rgb(79,79,79); }");
        }

	    if (report.m_locked)
        {
            ui->syncText->setStyleSheet("QLabel { background:rgb(37,53,39); }"); // turn on background
            ui->syncText->setText(report.m_syncType);
            ui->frequencyErrorText->setText(QString::number((int) std::lround(report.m_frequencyErrorHz)));
            ui->snrText->setText(QString::number((int) std::lround(report.m_snrDB)));
            ui->berText->setText(QString::number(report.m_berEstimate * 100.0f, 'f', 1));
        }
        else
        {
            ui->syncText->setStyleSheet("QLabel { background:rgb(53,53,53); }"); // turn off background
            ui->syncText->setText("No Sync");
            ui->frequencyErrorText->setText("----");
            ui->snrText->setText("--");
            ui->berText->setText("---");
        }

	    ui->formatStatusText->setText(report.m_statusText);

	    if (ui->activateStatusLog->isChecked() && !report.m_statusText.isEmpty() && (report.m_statusText != m_lastLoggedStatus))
        {
	        m_dsdStatusTextDialog.addLine(report.m_statusText);
            m_lastLoggedStatus = report.m_statusText;
	    }

	    if (report.m_statusText.isEmpty()) {
	        ui->formatStatusText->setStyleSheet("QLabel { background:rgb(53,53,53); }"); // turn off background
	    } else {
            ui->formatStatusText->setStyleSheet("QLabel { background:rgb(37,53,39); }"); // turn on background
	    }
	}

	// The tunnel is the DSE Modulator's, and goes on whether or not this channel runs
	if (m_tickCount % 10 == 0) {
		updateAngel61TunnelState();
	}

	m_tickCount++;
}

void DSDDemodGUI::updateAngel61TunnelState()
{
	const QLocale locale;
	QStringList lines;

	for (const DSDDemod::Angel61Tunnel& tunnel : m_dsdDemod->getAngel61Tunnels())
	{
		lines.append(tunnel.m_status);
		lines.append(tunnel.m_listening
			? tr("Delivered: %1 B (%2 datagrams), refused: %3")
				.arg(locale.toString(tunnel.m_bytes))
				.arg(locale.toString(tunnel.m_datagrams))
				.arg(locale.toString(tunnel.m_refused))
			: tr("Its Rx selector names another demodulator: this one is not used"));
	}

	ui->angel61TunnelState->setText(lines.isEmpty()
		? tr("No DSE Modulator: what is received will not reach the host")
		: lines.join('\n'));
}

void DSDDemodGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &DSDDemodGUI::on_deltaFrequency_changed);
    QObject::connect(ui->rfBW, &QSlider::valueChanged, this, &DSDDemodGUI::on_rfBW_valueChanged);
    QObject::connect(ui->volume, &QDial::valueChanged, this, &DSDDemodGUI::on_volume_valueChanged);
    QObject::connect(ui->protocol, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DSDDemodGUI::on_protocol_currentIndexChanged);
    QObject::connect(ui->p25p2System, &QLineEdit::editingFinished, this, &DSDDemodGUI::on_p25p2System_editingFinished);
    QObject::connect(ui->coherent, &ButtonSwitch::toggled, this, &DSDDemodGUI::on_coherent_toggled);
    QObject::connect(ui->traceLength, &QDial::valueChanged, this, &DSDDemodGUI::on_traceLength_valueChanged);
    QObject::connect(ui->traceStroke, &QDial::valueChanged, this, &DSDDemodGUI::on_traceStroke_valueChanged);
    QObject::connect(ui->traceDecay, &QDial::valueChanged, this, &DSDDemodGUI::on_traceDecay_valueChanged);
    QObject::connect(ui->slot1On, &QToolButton::toggled, this, &DSDDemodGUI::on_slot1On_toggled);
    QObject::connect(ui->slot2On, &QToolButton::toggled, this, &DSDDemodGUI::on_slot2On_toggled);
    QObject::connect(ui->tdmaStereoSplit, &QToolButton::toggled, this, &DSDDemodGUI::on_tdmaStereoSplit_toggled);
    QObject::connect(ui->dmrColorCode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DSDDemodGUI::on_dmrColorCode_currentIndexChanged);
    QObject::connect(ui->highPassFilter, &ButtonSwitch::toggled, this, &DSDDemodGUI::on_highPassFilter_toggled);
    QObject::connect(ui->audioMute, &QToolButton::toggled, this, &DSDDemodGUI::on_audioMute_toggled);
    QObject::connect(ui->viewStatusLog, &QPushButton::clicked, this, &DSDDemodGUI::on_viewStatusLog_clicked);
    QObject::connect(ui->clearTextMessages, &QPushButton::clicked, this, &DSDDemodGUI::on_clearTextMessages_clicked);
    QObject::connect(ui->angel61FileDirectory, &QLineEdit::editingFinished, this, &DSDDemodGUI::on_angel61FileDirectory_editingFinished);
    QObject::connect(ui->angel61FileDirectoryBrowse, &QPushButton::clicked, this, &DSDDemodGUI::on_angel61FileDirectoryBrowse_clicked);
    QObject::connect(ui->angel61FileMaxKB, QOverload<int>::of(&QSpinBox::valueChanged), this, &DSDDemodGUI::on_angel61FileMaxKB_valueChanged);
    QObject::connect(ui->angel61FileConfirm, &QCheckBox::toggled, this, &DSDDemodGUI::on_angel61FileConfirm_toggled);
    QObject::connect(ui->angel61FileAccept, &QPushButton::clicked, this, &DSDDemodGUI::on_angel61FileAccept_clicked);
    QObject::connect(ui->angel61FileReject, &QPushButton::clicked, this, &DSDDemodGUI::on_angel61FileReject_clicked);
    QObject::connect(ui->angel61Groups, &QLineEdit::editingFinished, this, &DSDDemodGUI::on_angel61Groups_editingFinished);
    QObject::connect(ui->contactReportEnabled, &ButtonSwitch::toggled, this, [this](bool checked) {
        m_settings.m_contactReportEnabled = checked; applySettings({"contactReportEnabled"});
    });
    QObject::connect(ui->contactReportCallsign, &QLineEdit::editingFinished, this, [this] {
        m_settings.m_contactReportCallsign = ui->contactReportCallsign->text().trimmed().toUpper();
        ui->contactReportCallsign->setText(m_settings.m_contactReportCallsign);
        applySettings({"contactReportCallsign"});
    });
    QObject::connect(ui->vocoder, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DSDDemodGUI::on_vocoder_currentIndexChanged);
}

void DSDDemodGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}
