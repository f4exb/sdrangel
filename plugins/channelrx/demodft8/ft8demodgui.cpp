///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Edouard Griffiths, F4EXB                                   //
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
#include <QPixmap>
#include <QScrollBar>

#include "plugin/pluginapi.h"
#include "device/deviceuiset.h"
#include "dsp/spectrumvis.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "util/simpleserializer.h"
#include "gui/glspectrum.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "gui/crightclickenabler.h"
#include "gui/audioselectdialog.h"
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "util/db.h"
#include "maincore.h"

#include "ui_ft8demodgui.h"
#include "ft8demodgui.h"
#include "ft8demod.h"
#include "ft8demodsettingsdialog.h"

FT8MessagesTableModel::FT8MessagesTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{}

int FT8MessagesTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_ft8Messages.size();
}

int FT8MessagesTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_columnCount;
}

QVariant FT8MessagesTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    if (index.row() >= m_ft8Messages.size() || index.row() < 0) {
        return QVariant();
    }

    if (role == Qt::DisplayRole)
    {
        const auto &ft8Message = m_ft8Messages.at(index.row());

        switch (index.column()) {
            case FT8DemodSettings::MESSAGE_COL_UTC:
                return ft8Message.m_utc;
            case FT8DemodSettings::MESSAGE_COL_TYPE:
                return ft8Message.m_type;
            case FT8DemodSettings::MESSAGE_COL_PASS:
                return ft8Message.m_pass;
            case FT8DemodSettings::MESSAGE_COL_OKBITS:
                return ft8Message.m_okBits;
            case FT8DemodSettings::MESSAGE_COL_DT:
                return tr("%1").arg(ft8Message.m_dt, 4, 'f', 1);
            case FT8DemodSettings::MESSAGE_COL_DF:
                return ft8Message.m_df;
            case FT8DemodSettings::MESSAGE_COL_SNR:
                return ft8Message.m_snr;
            case FT8DemodSettings::MESSAGE_COL_CALL1:
                return ft8Message.m_call1;
            case FT8DemodSettings::MESSAGE_COL_CALL2:
                return ft8Message.m_call2;
            case FT8DemodSettings::MESSAGE_COL_LOC:
                return ft8Message.m_loc;
            case FT8DemodSettings::MESSAGE_COL_INFO:
                return ft8Message.m_info;
            default:
                break;
        }
    }

    if (role == Qt::TextAlignmentRole)
    {
        switch (index.column()) {
            case FT8DemodSettings::MESSAGE_COL_TYPE:
            case FT8DemodSettings::MESSAGE_COL_DT:
            case FT8DemodSettings::MESSAGE_COL_DF:
            case FT8DemodSettings::MESSAGE_COL_SNR:
                return Qt::AlignRight;
            default:
                break;
        }
    }

    return QVariant();
}

QVariant FT8MessagesTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if ((role == Qt::DisplayRole) && (orientation == Qt::Horizontal))
    {
        switch (section) {
            case FT8DemodSettings::MESSAGE_COL_UTC:
                return tr("UTC");
            case FT8DemodSettings::MESSAGE_COL_TYPE:
                return tr("Typ");
            case FT8DemodSettings::MESSAGE_COL_PASS:
                return tr("P");
            case FT8DemodSettings::MESSAGE_COL_OKBITS:
                return tr("OKb");
            case FT8DemodSettings::MESSAGE_COL_DT:
                return tr("dt");
            case FT8DemodSettings::MESSAGE_COL_DF:
                return tr("df");
            case FT8DemodSettings::MESSAGE_COL_SNR:
                return tr("SNR");
            case FT8DemodSettings::MESSAGE_COL_CALL1:
                return tr("Call1");
            case FT8DemodSettings::MESSAGE_COL_CALL2:
                return tr("Call2");
            case FT8DemodSettings::MESSAGE_COL_LOC:
                return tr("Loc");
            case FT8DemodSettings::MESSAGE_COL_INFO:
                return tr("Info");
            default:
                break;
        }
    }

    if (role == Qt::ToolTipRole)
    {
        switch (section) {
            case FT8DemodSettings::MESSAGE_COL_UTC:
                return tr("Sequence UTC time HHMMSS");
            case FT8DemodSettings::MESSAGE_COL_TYPE:
                return tr("Message type (see documentation)");
            case FT8DemodSettings::MESSAGE_COL_PASS:
                return tr("Successful decoder pass index");
            case FT8DemodSettings::MESSAGE_COL_OKBITS:
                return tr("Number of correct bits before correction");
            case FT8DemodSettings::MESSAGE_COL_DT:
                return tr("Message start time delay in sequence (s)");
            case FT8DemodSettings::MESSAGE_COL_DF:
                return tr("Carrier frequency shift (Hz)");
            case FT8DemodSettings::MESSAGE_COL_SNR:
                return tr("Signal to noise ratio (dB) in 2.5 kHz bandwidth");
            case FT8DemodSettings::MESSAGE_COL_CALL1:
                return tr("Fist call area");
            case FT8DemodSettings::MESSAGE_COL_CALL2:
                return tr("Second call area");
            case FT8DemodSettings::MESSAGE_COL_LOC:
                return tr("Locator area");
            case FT8DemodSettings::MESSAGE_COL_INFO:
                return tr("Decoder information");
            default:
                break;
        }
    }

    return QVariant();
}

const QVector<FT8MesssageData> &FT8MessagesTableModel::getMessages() const
{
    return m_ft8Messages;
}

void FT8MessagesTableModel::messagesReceived(const QList<FT8Message>& messages)
{
    int position = m_ft8Messages.size();
    int rows = messages.size();
    beginInsertRows(QModelIndex(), position, position + rows - 1);

    for (const auto& message : messages)
    {
        m_ft8Messages.push_back(FT8MesssageData{
            message.ts.toString("HHmmss"),
            message.type,
            message.pass,
            message.nbCorrectBits,
            message.dt,
            (int) message.df,
            message.snr,
            message.call1,
            message.call2,
            message.loc,
            message.decoderInfo
        });
    }

    endInsertRows();
}

void FT8MessagesTableModel::setDefaultMessage()
{
    if (m_ft8Messages.size() != 0) {
        return;
    }

    beginInsertRows(QModelIndex(), 0, 0);
    m_ft8Messages.push_back(FT8MesssageData{
        "000000",
        "0.0",
        0,
        174,
        -8.0,
        8000,
        -24,
        "CQ PA900RAALTE",
        "PA900RAALTE",
        "JN000",
        "OSD-0-73"
    });
    endInsertRows();
}

void FT8MessagesTableModel::clearMessages()
{
    if (m_ft8Messages.size() == 0) {
        return;
    }

    beginRemoveRows(QModelIndex(), 0, m_ft8Messages.size()-1);
    m_ft8Messages.clear();
    endRemoveRows();
}


FT8DemodGUI* FT8DemodGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
	FT8DemodGUI* gui = new FT8DemodGUI(pluginAPI, deviceUISet, rxChannel);
	return gui;
}

void FT8DemodGUI::destroy()
{
	delete this;
}

void FT8DemodGUI::resetToDefaults()
{
	m_settings.resetToDefaults();
}

QByteArray FT8DemodGUI::serialize() const
{
    return m_settings.serialize();
}

bool FT8DemodGUI::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data))
    {
        ui->BW->setMaximum(480);
        ui->BW->setMinimum(-480);
        ui->lowCut->setMaximum(480);
        ui->lowCut->setMinimum(-480);
        displaySettings();
        applyBandwidths(m_settings.m_filterBank[m_settings.m_filterIndex].m_spanLog2, true); // does applySettings(true)
        populateBandPresets();
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        ui->BW->setMaximum(480);
        ui->BW->setMinimum(-480);
        ui->lowCut->setMaximum(480);
        ui->lowCut->setMinimum(-480);
        displaySettings();
        applyBandwidths(m_settings.m_filterBank[m_settings.m_filterIndex].m_spanLog2, true); // does applySettings(true)
        populateBandPresets();
        return false;
    }
}

bool FT8DemodGUI::handleMessage(const Message& message)
{
    if (FT8Demod::MsgConfigureFT8Demod::match(message))
    {
        qDebug("FT8DemodGUI::handleMessage: FT8Demod::MsgConfigureFT8Demod");
        const FT8Demod::MsgConfigureFT8Demod& cfg = (FT8Demod::MsgConfigureFT8Demod&) message;
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
        ui->deltaFrequency->setValueRange(false, 7, -m_basebandSampleRate/2, m_basebandSampleRate/2);
        ui->deltaFrequencyLabel->setToolTip(tr("Range %1 %L2 Hz").arg(QChar(0xB1)).arg(m_basebandSampleRate/2));
        updateAbsoluteCenterFrequency();
        return true;
    }
    else if (MsgReportFT8Messages::match(message))
    {
        MsgReportFT8Messages& notif = (MsgReportFT8Messages&) message;
        messagesReceived(notif.getFT8Messages());
        return true;
    }
    else
    {
        return false;
    }
}

void FT8DemodGUI::handleInputMessages()
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

void FT8DemodGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void FT8DemodGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void FT8DemodGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings();
}

void FT8DemodGUI::on_BW_valueChanged(int value)
{
    (void) value;
    qDebug("FT8DemodGUI::on_BW_valueChanged: ui->spanLog2: %d", ui->spanLog2->value());
    applyBandwidths(1 + ui->spanLog2->maximum() - ui->spanLog2->value());
}

void FT8DemodGUI::on_lowCut_valueChanged(int value)
{
    (void) value;
    applyBandwidths(1 + ui->spanLog2->maximum() - ui->spanLog2->value());
}

void FT8DemodGUI::on_volume_valueChanged(int value)
{
	ui->volumeText->setText(QString("%1").arg(value));
	m_settings.m_volume = CalcDb::powerFromdB(value);
	applySettings();
}

void FT8DemodGUI::on_agc_toggled(bool checked)
{
    m_settings.m_agc = checked;
    applySettings();
}

void FT8DemodGUI::on_spanLog2_valueChanged(int value)
{
    int s2max = spanLog2Max();

    if ((value < 0) || (value > s2max-1)) {
        return;
    }

    applyBandwidths(s2max - ui->spanLog2->value());
}

void FT8DemodGUI::on_fftWindow_currentIndexChanged(int index)
{
    m_settings.m_filterBank[m_settings.m_filterIndex].m_fftWindow = (FFTWindow::Function) index;
    applySettings();
}

void FT8DemodGUI::on_filterIndex_valueChanged(int value)
{
    if ((value < 0) || (value >= 10)) {
        return;
    }

    ui->filterIndexText->setText(tr("%1").arg(value));
    m_settings.m_filterIndex = value;
    ui->BW->setMaximum(480);
    ui->BW->setMinimum(-480);
    ui->lowCut->setMaximum(480);
    ui->lowCut->setMinimum(-480);
    displaySettings();
    applyBandwidths(m_settings.m_filterBank[m_settings.m_filterIndex].m_spanLog2, true); // does applySettings(true)
}

void FT8DemodGUI::on_moveToBottom_clicked()
{
    ui->messagesView->scrollToBottom();
}

void FT8DemodGUI::on_filterMessages_toggled(bool checked)
{
    m_filterMessages = checked;
    filterMessages();
}

void FT8DemodGUI::on_applyBandPreset_clicked()
{
    int bandPresetIndex = ui->bandPreset->currentIndex();
    int channelShift = m_settings.m_bandPresets[bandPresetIndex].m_channelOffset; // kHz
    int baseFrequency = m_settings.m_bandPresets[bandPresetIndex].m_baseFrequency; // kHz
    quint64 deviceFrequency = (baseFrequency - channelShift)*1000; // Hz
    m_ft8Demod->setDeviceCenterFrequency(deviceFrequency, m_settings.m_streamIndex);

    if (channelShift * 1000 != m_settings.m_inputFrequencyOffset)
    {
        m_settings.m_inputFrequencyOffset = channelShift * 1000; // Hz
        displaySettings();
        applySettings();
    }
}

void FT8DemodGUI::on_clearMessages_clicked()
{
    ui->nbDecodesInTable->setText("0");
    m_messagesModel.clearMessages();
}

void FT8DemodGUI::on_recordWav_toggled(bool checked)
{
    m_settings.m_recordWav = checked;
    applySettings();
}

void FT8DemodGUI::on_logMessages_toggled(bool checked)
{
    m_settings.m_logMessages = checked;
    applySettings();
}

void FT8DemodGUI::on_settings_clicked()
{
    FT8DemodSettings settings = m_settings;
    QStringList settingsKeys;
    FT8DemodSettingsDialog dialog(settings, settingsKeys);

    if (dialog.exec() == QDialog::Accepted)
    {
        bool changed = false;

        if (settingsKeys.contains("nbDecoderThreads"))
        {
            m_settings.m_nbDecoderThreads = settings.m_nbDecoderThreads;
            changed = true;
        }

        if (settingsKeys.contains("decoderTimeBudget"))
        {
            m_settings.m_decoderTimeBudget = settings.m_decoderTimeBudget;
            changed = true;
        }

        if (settingsKeys.contains("useOSD"))
        {
            m_settings.m_useOSD = settings.m_useOSD;
            changed = true;
        }

        if (settingsKeys.contains("osdDepth"))
        {
            m_settings.m_osdDepth = settings.m_osdDepth;
            changed = true;
        }

        if (settingsKeys.contains("osdLDPCThreshold"))
        {
            m_settings.m_osdLDPCThreshold = settings.m_osdLDPCThreshold;
            changed = true;
        }

        if (settingsKeys.contains("verifyOSD"))
        {
            m_settings.m_verifyOSD = settings.m_verifyOSD;
            changed = true;
        }

        if (settingsKeys.contains("bandPresets"))
        {
            m_settings.m_bandPresets = settings.m_bandPresets;
            populateBandPresets();
        }

        if (changed) {
            applySettings();
        }
    }
}

void FT8DemodGUI::onMenuDialogCalled(const QPoint &p)
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
            dialog.setNumberOfStreams(m_ft8Demod->getNumberOfDeviceStreams());
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

void FT8DemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

FT8DemodGUI::FT8DemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
	ChannelGUI(parent),
	ui(new Ui::FT8DemodGUI),
	m_pluginAPI(pluginAPI),
	m_deviceUISet(deviceUISet),
	m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_basebandSampleRate(1),
	m_doApplySettings(true),
    m_spectrumRate(6000),
	m_audioBinaural(false),
	m_audioFlipChannels(false),
    m_audioMute(false),
	m_squelchOpen(false),
    m_audioSampleRate(-1),
    m_filterMessages(false)
{
	setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/demodft8/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
	connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

	m_ft8Demod = (FT8Demod*) rxChannel;
    m_spectrumVis = m_ft8Demod->getSpectrumVis();
	m_spectrumVis->setGLSpectrum(ui->glSpectrum);
	m_ft8Demod->setMessageQueueToGUI(getInputMessageQueue());

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);
	ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);

	ui->spectrumGUI->setBuddies(m_spectrumVis, ui->glSpectrum);

    ui->glSpectrum->setCenterFrequency(m_spectrumRate/2);
    ui->glSpectrum->setSampleRate(m_spectrumRate);

    SpectrumSettings spectrumSettings = m_spectrumVis->getSettings();
    spectrumSettings.m_displayWaterfall = true;
    spectrumSettings.m_ssb = true;
    SpectrumVis::MsgConfigureSpectrumVis *msg = SpectrumVis::MsgConfigureSpectrumVis::create(spectrumSettings, false);
    m_spectrumVis->getInputMessageQueue()->push(msg);

	connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

	m_channelMarker.blockSignals(true);
	m_channelMarker.setColor(Qt::green);
    m_channelMarker.setBandwidth(6000);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("SSB Demodulator");
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    setTitleColor(m_channelMarker.getColor());

    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setSpectrumGUI(ui->spectrumGUI);
    m_settings.setRollupState(&m_rollupState);

	m_deviceUISet->addChannelMarker(&m_channelMarker);
	connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()), this, SLOT(channelMarkerHighlightedByCursor()));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    m_ft8Demod->setLevelMeter(ui->volumeMeter);

    ui->BW->setMaximum(60);
    ui->BW->setMinimum(10);
    ui->lowCut->setMaximum(50);
    ui->lowCut->setMinimum(0);
	displaySettings();
    makeUIConnections();

	applyBandwidths(m_settings.m_filterBank[m_settings.m_filterIndex].m_spanLog2, true); // does applySettings(true)
    DialPopup::addPopupsToChildDials(this);

    populateBandPresets();
    setupMessagesView();
}

FT8DemodGUI::~FT8DemodGUI()
{
	delete ui;
}

bool FT8DemodGUI::blockApplySettings(bool block)
{
    bool ret = !m_doApplySettings;
    m_doApplySettings = !block;
    return ret;
}

void FT8DemodGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
        FT8Demod::MsgConfigureFT8Demod* message = FT8Demod::MsgConfigureFT8Demod::create( m_settings, force);
        m_ft8Demod->getInputMessageQueue()->push(message);
	}
}

unsigned int FT8DemodGUI::spanLog2Max()
{
    unsigned int spanLog2 = 0;
    for (; FT8DemodSettings::m_ft8SampleRate / (1<<spanLog2) >= 1000; spanLog2++);
    return spanLog2 == 0 ? 0 : spanLog2-1;
}

void FT8DemodGUI::applyBandwidths(unsigned int spanLog2, bool force)
{
    unsigned int s2max = spanLog2Max();
    spanLog2 = spanLog2 > s2max ? s2max : spanLog2;
    unsigned int limit = s2max < 1 ? 0 : s2max - 1;
    ui->spanLog2->setMaximum(limit);
    //int spanLog2 = ui->spanLog2->value();
    m_spectrumRate = FT8DemodSettings::m_ft8SampleRate / (1<<spanLog2);
    int bw = ui->BW->value();
    int lw = ui->lowCut->value();
    int bwMax = FT8DemodSettings::m_ft8SampleRate / (100*(1<<spanLog2));
    int tickInterval = m_spectrumRate / 2400;
    tickInterval = tickInterval == 0 ? 1 : tickInterval;

    qDebug() << "FT8DemodGUI::applyBandwidths:"
            << " s2max:" << s2max
            << " spanLog2: " << spanLog2
            << " m_spectrumRate: " << m_spectrumRate
            << " bw: " << bw
            << " lw: " << lw
            << " bwMax: " << bwMax
            << " tickInterval: " << tickInterval;

    ui->BW->setTickInterval(tickInterval);
    ui->lowCut->setTickInterval(tickInterval);

    bw = bw < 0 ? 0 : bw > bwMax ? bwMax : bw;
    lw = lw > bw-1 ? bw-1 : lw < 0 ? 0 : lw;

    if (bw == 0) {
        lw = 0;
    }

    QString spanStr = QString::number(bwMax/10.0, 'f', 1);
    QString bwStr   = QString::number(bw/10.0, 'f', 1);
    QString lwStr   = QString::number(lw/10.0, 'f', 1);

    ui->BWText->setText(tr("%1k").arg(bwStr));
    ui->spanText->setText(tr("%1k").arg(spanStr));
    ui->glSpectrum->setCenterFrequency(0);
    ui->glSpectrum->setSampleRate(2*m_spectrumRate);
    ui->glSpectrum->setLsbDisplay(bw < 0);
    ui->glSpectrum->setSsbSpectrum(true);

    ui->lowCutText->setText(tr("%1k").arg(lwStr));

    ui->BW->blockSignals(true);
    ui->lowCut->blockSignals(true);

    ui->BW->setMaximum(bwMax);
    ui->BW->setMinimum(0);
    ui->BW->setValue(bw);

    ui->lowCut->setMaximum(bwMax);
    ui->lowCut->setMinimum(0);
    ui->lowCut->setValue(lw);

    ui->lowCut->blockSignals(false);
    ui->BW->blockSignals(false);

    ui->channelPowerMeter->setRange(FT8DemodSettings::m_minPowerThresholdDB, 0);

    m_settings.m_filterBank[m_settings.m_filterIndex].m_spanLog2 = spanLog2;
    m_settings.m_filterBank[m_settings.m_filterIndex].m_rfBandwidth = bw * 100;
    m_settings.m_filterBank[m_settings.m_filterIndex].m_lowCutoff = lw * 100;

    applySettings(force);

    bool wasBlocked = blockApplySettings(true);
    m_channelMarker.setBandwidth(bw * 200);
    m_channelMarker.setSidebands(bw < 0 ? ChannelMarker::lsb : ChannelMarker::usb);
    m_channelMarker.setLowCutoff(lw * 100);
    blockApplySettings(wasBlocked);
}

void FT8DemodGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setBandwidth(m_settings.m_filterBank[m_settings.m_filterIndex].m_rfBandwidth * 2);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.setLowCutoff(m_settings.m_filterBank[m_settings.m_filterIndex].m_lowCutoff);

    if (m_deviceUISet->m_deviceMIMOEngine)
    {
        m_channelMarker.clearStreamIndexes();
        m_channelMarker.addStreamIndex(m_settings.m_streamIndex);
    }

    if (m_settings.m_filterBank[m_settings.m_filterIndex].m_rfBandwidth < 0) {
        m_channelMarker.setSidebands(ChannelMarker::lsb);
    } else {
        m_channelMarker.setSidebands(ChannelMarker::usb);
    }

    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());

    blockApplySettings(true);

    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());

    ui->agc->setChecked(m_settings.m_agc);
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    ui->fftWindow->setCurrentIndex((int) m_settings.m_filterBank[m_settings.m_filterIndex].m_fftWindow);

    // Prevent uncontrolled triggering of applyBandwidths
    ui->spanLog2->blockSignals(true);
    ui->BW->blockSignals(true);
    ui->filterIndex->blockSignals(true);

    ui->filterIndex->setValue(m_settings.m_filterIndex);
    ui->filterIndexText->setText(tr("%1").arg(m_settings.m_filterIndex));

    ui->spanLog2->setValue(1 + ui->spanLog2->maximum() - m_settings.m_filterBank[m_settings.m_filterIndex].m_spanLog2);

    ui->BW->setValue(m_settings.m_filterBank[m_settings.m_filterIndex].m_rfBandwidth / 100.0);
    QString s = QString::number(m_settings.m_filterBank[m_settings.m_filterIndex].m_rfBandwidth/1000.0, 'f', 1);

    ui->BWText->setText(tr("%1k").arg(s));

    ui->spanLog2->blockSignals(false);
    ui->BW->blockSignals(false);
    ui->filterIndex->blockSignals(false);

    // The only one of the four signals triggering applyBandwidths will trigger it once only with all other values
    // set correctly and therefore validate the settings and apply them to dependent widgets
    ui->lowCut->setValue(m_settings.m_filterBank[m_settings.m_filterIndex].m_lowCutoff / 100.0);
    ui->lowCutText->setText(tr("%1k").arg(m_settings.m_filterBank[m_settings.m_filterIndex].m_lowCutoff / 1000.0));

    int volume = CalcDb::dbPower(m_settings.m_volume);
    ui->volume->setValue(volume);
    ui->volumeText->setText(QString("%1").arg(volume));

    ui->recordWav->setChecked(m_settings.m_recordWav);
    ui->logMessages->setChecked(m_settings.m_logMessages);

    updateIndexLabel();

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void FT8DemodGUI::leaveEvent(QEvent* event)
{
	m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void FT8DemodGUI::enterEvent(EnterEventType* event)
{
	m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void FT8DemodGUI::tick()
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    m_ft8Demod->getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
    double powDbAvg = CalcDb::dbPower(magsqAvg);
    double powDbPeak = CalcDb::dbPower(magsqPeak);

    ui->channelPowerMeter->levelChanged(
            (FT8DemodSettings::m_mminPowerThresholdDBf + powDbAvg) / FT8DemodSettings::m_mminPowerThresholdDBf,
            (FT8DemodSettings::m_mminPowerThresholdDBf + powDbPeak) / FT8DemodSettings::m_mminPowerThresholdDBf,
            nbMagsqSamples);

    if (m_tickCount % 4 == 0) {
        ui->channelPower->setText(tr("%1 dB").arg(powDbAvg, 0, 'f', 1));
    }

    m_tickCount++;
}

void FT8DemodGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &FT8DemodGUI::on_deltaFrequency_changed);
    QObject::connect(ui->BW, &TickedSlider::valueChanged, this, &FT8DemodGUI::on_BW_valueChanged);
    QObject::connect(ui->lowCut, &TickedSlider::valueChanged, this, &FT8DemodGUI::on_lowCut_valueChanged);
    QObject::connect(ui->volume, &QDial::valueChanged, this, &FT8DemodGUI::on_volume_valueChanged);
    QObject::connect(ui->agc, &ButtonSwitch::toggled, this, &FT8DemodGUI::on_agc_toggled);
    QObject::connect(ui->spanLog2, &QSlider::valueChanged, this, &FT8DemodGUI::on_spanLog2_valueChanged);
    QObject::connect(ui->fftWindow, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FT8DemodGUI::on_fftWindow_currentIndexChanged);
    QObject::connect(ui->filterIndex, &QDial::valueChanged, this, &FT8DemodGUI::on_filterIndex_valueChanged);
    QObject::connect(ui->moveToBottom, &QPushButton::clicked, this, &FT8DemodGUI::on_moveToBottom_clicked);
    QObject::connect(ui->filterMessages, &ButtonSwitch::toggled, this, &FT8DemodGUI::on_filterMessages_toggled);
    QObject::connect(ui->applyBandPreset, &QPushButton::clicked, this, &FT8DemodGUI::on_applyBandPreset_clicked);
    QObject::connect(ui->clearMessages, &QPushButton::clicked, this, &FT8DemodGUI::on_clearMessages_clicked);
    QObject::connect(ui->recordWav, &ButtonSwitch::toggled, this, &FT8DemodGUI::on_recordWav_toggled);
    QObject::connect(ui->logMessages, &ButtonSwitch::toggled, this, &FT8DemodGUI::on_logMessages_toggled);
    QObject::connect(ui->settings, &QPushButton::clicked, this, &FT8DemodGUI::on_settings_clicked);
}

void FT8DemodGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}

void FT8DemodGUI::messagesReceived(const QList<FT8Message>& messages)
{
    ui->nbDecodesText->setText(tr("%1").arg(messages.size()));

    // Is scroll bar at bottom
    QScrollBar *sb = ui->messagesView->verticalScrollBar();
    bool scrollToBottom = sb->value() == sb->maximum();

    m_messagesModel.messagesReceived(messages);
    ui->nbDecodesInTable->setText(tr("%1").arg(m_messagesModel.countAllMessages()));

    if (scrollToBottom) {
        ui->messagesView->scrollToBottom();
    }
}

void FT8DemodGUI::populateBandPresets()
{
    ui->bandPreset->blockSignals(true);
    ui->bandPreset->clear();

    for (const auto& bandPreset : m_settings.m_bandPresets) {
        ui->bandPreset->addItem(bandPreset.m_name);
    }

    ui->bandPreset->blockSignals(false);
}

void FT8DemodGUI::messageViewClicked(const QModelIndex &index)
{
    if (index.isValid())
    {
        m_selectedColumn = index.column();
        m_selectedData = index.data();
        filterMessages();
    }
}

void FT8DemodGUI::filterMessages()
{
    if (!m_filterMessages)
    {
        m_messagesFilterProxy.resetFilter();
        return;
    }

    if ((m_selectedColumn == FT8DemodSettings::MESSAGE_COL_CALL1) || (m_selectedColumn == FT8DemodSettings::MESSAGE_COL_CALL2)) {
        m_messagesFilterProxy.setFilterCall(m_selectedData.toString());
    } else if (m_selectedColumn == FT8DemodSettings::MESSAGE_COL_LOC) {
        m_messagesFilterProxy.setFilterLoc(m_selectedData.toString());
    } else if (m_selectedColumn == FT8DemodSettings::MESSAGE_COL_UTC) {
        m_messagesFilterProxy.setFilterUTC(m_selectedData.toString());
    } else if (m_selectedColumn == FT8DemodSettings::MESSAGE_COL_DF) {
        m_messagesFilterProxy.setFilterDf(m_selectedData.toInt());
    } else if (m_selectedColumn == FT8DemodSettings::MESSAGE_COL_INFO) {
        m_messagesFilterProxy.setFilterInfo(m_selectedData.toString());
    }
}

void FT8DemodGUI::setupMessagesView()
{
    m_messagesFilterProxy.setSourceModel(&m_messagesModel);
    ui->messagesView->setModel(&m_messagesFilterProxy);
    // resize columns
    m_messagesModel.setDefaultMessage();
    ui->messagesView->resizeColumnsToContents();
    m_messagesModel.clearMessages();

    connect(ui->messagesView, &QTableView::clicked, this, &FT8DemodGUI::messageViewClicked);
}
