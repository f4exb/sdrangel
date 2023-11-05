#include <QPixmap>

#include "ssbdemodgui.h"

#include "device/deviceuiset.h"

#include "ui_ssbdemodgui.h"
#include "dsp/spectrumvis.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "gui/glspectrum.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "gui/crightclickenabler.h"
#include "gui/audioselectdialog.h"
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "maincore.h"
#include "ssbdemod.h"

SSBDemodGUI* SSBDemodGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
	SSBDemodGUI* gui = new SSBDemodGUI(pluginAPI, deviceUISet, rxChannel);
	return gui;
}

void SSBDemodGUI::destroy()
{
	delete this;
}

void SSBDemodGUI::resetToDefaults()
{
	m_settings.resetToDefaults();
}

QByteArray SSBDemodGUI::serialize() const
{
    return m_settings.serialize();
}

bool SSBDemodGUI::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data))
    {
        ui->BW->setMaximum(480);
        ui->BW->setMinimum(-480);
        ui->lowCut->setMaximum(480);
        ui->lowCut->setMinimum(-480);
        displaySettings();
        applyBandwidths(m_settings.m_filterBank[m_settings.m_filterIndex].m_spanLog2, true); // does applySettings(true)
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
        return false;
    }
}

bool SSBDemodGUI::handleMessage(const Message& message)
{
    if (SSBDemod::MsgConfigureSSBDemod::match(message))
    {
        qDebug("SSBDemodGUI::handleMessage: SSBDemod::MsgConfigureSSBDemod");
        const SSBDemod::MsgConfigureSSBDemod& cfg = (SSBDemod::MsgConfigureSSBDemod&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        ui->spectrumGUI->updateSettings();
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (DSPConfigureAudio::match(message))
    {
        qDebug("SSBDemodGUI::handleMessage: DSPConfigureAudio: %d", m_ssbDemod->getAudioSampleRate());
        applyBandwidths(1 + ui->spanLog2->maximum() - ui->spanLog2->value()); // will update spectrum details with new sample rate
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (DSPSignalNotification::match(message))
    {
        const DSPSignalNotification& notif = (const DSPSignalNotification&) message;
        m_deviceCenterFrequency = notif.getCenterFrequency();
        m_basebandSampleRate = notif.getSampleRate();
        qDebug("SSBDemodGUI::handleMessage: DSPSignalNotification: centerFrequency: %lld sampleRate: %d",
             m_deviceCenterFrequency, m_basebandSampleRate);
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

void SSBDemodGUI::handleInputMessages()
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

void SSBDemodGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void SSBDemodGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void SSBDemodGUI::on_audioBinaural_toggled(bool binaural)
{
	m_audioBinaural = binaural;
	m_settings.m_audioBinaural = binaural;
	applySettings();
}

void SSBDemodGUI::on_audioFlipChannels_toggled(bool flip)
{
	m_audioFlipChannels = flip;
	m_settings.m_audioFlipChannels = flip;
	applySettings();
}

void SSBDemodGUI::on_dsb_toggled(bool dsb)
{
    ui->flipSidebands->setEnabled(!dsb);
    applyBandwidths(1 + ui->spanLog2->maximum() - ui->spanLog2->value());
}

void SSBDemodGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings();
}

void SSBDemodGUI::on_BW_valueChanged(int value)
{
    (void) value;
    qDebug("SSBDemodGUI::on_BW_valueChanged: ui->spanLog2: %d", ui->spanLog2->value());
    applyBandwidths(1 + ui->spanLog2->maximum() - ui->spanLog2->value());
}

void SSBDemodGUI::on_lowCut_valueChanged(int value)
{
    (void) value;
    applyBandwidths(1 + ui->spanLog2->maximum() - ui->spanLog2->value());
}

void SSBDemodGUI::on_volume_valueChanged(int value)
{
	ui->volumeText->setText(QString("%1").arg(value));
	m_settings.m_volume = CalcDb::powerFromdB(value);
	applySettings();
}

void SSBDemodGUI::on_agc_toggled(bool checked)
{
    m_settings.m_agc = checked;
    applySettings();
}

void SSBDemodGUI::on_agcClamping_toggled(bool checked)
{
    m_settings.m_agcClamping = checked;
    applySettings();
}

void SSBDemodGUI::on_dnr_toggled(bool checked)
{
    m_settings.m_dnr = checked;
    m_settings.m_filterBank[m_settings.m_filterIndex].m_dnr = m_settings.m_dnr;
    applySettings();
}

void SSBDemodGUI::on_agcTimeLog2_valueChanged(int value)
{
    QString s = QString::number((1<<value), 'f', 0);
    ui->agcTimeText->setText(s);
    m_settings.m_agcTimeLog2 = value;
    applySettings();
}

void SSBDemodGUI::on_agcPowerThreshold_valueChanged(int value)
{
    displayAGCPowerThreshold(value);
    m_settings.m_agcPowerThreshold = value;
    applySettings();
}

void SSBDemodGUI::on_agcThresholdGate_valueChanged(int value)
{
    int agcThresholdGate = value < 20 ? value : ((value - 20) * 10) + 20;
    QString s = QString::number(agcThresholdGate, 'f', 0);
    ui->agcThresholdGateText->setText(s);
    m_settings.m_agcThresholdGate = agcThresholdGate;
    applySettings();
}

void SSBDemodGUI::on_audioMute_toggled(bool checked)
{
	m_audioMute = checked;
	m_settings.m_audioMute = checked;
	applySettings();
}

void SSBDemodGUI::on_spanLog2_valueChanged(int value)
{
    int s2max = spanLog2Max();

    if ((value < 0) || (value > s2max-1)) {
        return;
    }

    applyBandwidths(s2max - ui->spanLog2->value());
}

void SSBDemodGUI::on_flipSidebands_clicked(bool checked)
{
    (void) checked;
    int bwValue = ui->BW->value();
    int lcValue = ui->lowCut->value();
    ui->BW->setValue(-bwValue);
    ui->lowCut->setValue(-lcValue);
}

void SSBDemodGUI::on_fftWindow_currentIndexChanged(int index)
{
    m_settings.m_filterBank[m_settings.m_filterIndex].m_fftWindow = (FFTWindow::Function) index;
    applySettings();
}

void SSBDemodGUI::on_filterIndex_valueChanged(int value)
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
    m_settings.m_dnr = m_settings.m_filterBank[m_settings.m_filterIndex].m_dnr;
    m_settings.m_dnrScheme = m_settings.m_filterBank[m_settings.m_filterIndex].m_dnrScheme;
    m_settings.m_dnrAboveAvgFactor = m_settings.m_filterBank[m_settings.m_filterIndex].m_dnrAboveAvgFactor;
    m_settings.m_dnrSigmaFactor = m_settings.m_filterBank[m_settings.m_filterIndex].m_dnrSigmaFactor;
    m_settings.m_dnrNbPeaks = m_settings.m_filterBank[m_settings.m_filterIndex].m_dnrNbPeaks;
    m_settings.m_dnrAlpha = m_settings.m_filterBank[m_settings.m_filterIndex].m_dnrAlpha;
    displaySettings();
    applyBandwidths(m_settings.m_filterBank[m_settings.m_filterIndex].m_spanLog2, true); // does applySettings(true)
}

void SSBDemodGUI::onMenuDialogCalled(const QPoint &p)
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
            dialog.setNumberOfStreams(m_ssbDemod->getNumberOfDeviceStreams());
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

void SSBDemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

SSBDemodGUI::SSBDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
	ChannelGUI(parent),
	ui(new Ui::SSBDemodGUI),
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
    m_fftNRDialog(nullptr)
{
	setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/demodssb/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
	connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

	m_ssbDemod = (SSBDemod*) rxChannel;
    m_spectrumVis = m_ssbDemod->getSpectrumVis();
	m_spectrumVis->setGLSpectrum(ui->glSpectrum);
	m_ssbDemod->setMessageQueueToGUI(getInputMessageQueue());

    CRightClickEnabler *audioMuteRightClickEnabler = new CRightClickEnabler(ui->audioMute);
    connect(audioMuteRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(audioSelect(const QPoint &)));

    CRightClickEnabler *dnrRightClickEnabler = new CRightClickEnabler(ui->dnr);
    connect(dnrRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(dnrSetupDialog(const QPoint &)));

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


	m_iconDSBUSB.addPixmap(QPixmap("://dsb.png"), QIcon::Normal, QIcon::On);
    m_iconDSBUSB.addPixmap(QPixmap("://usb.png"), QIcon::Normal, QIcon::Off);
	m_iconDSBLSB.addPixmap(QPixmap("://dsb.png"), QIcon::Normal, QIcon::On);
    m_iconDSBLSB.addPixmap(QPixmap("://lsb.png"), QIcon::Normal, QIcon::Off);

    ui->BW->setMaximum(480);
    ui->BW->setMinimum(-480);
    ui->lowCut->setMaximum(480);
    ui->lowCut->setMinimum(-480);
	displaySettings();
    makeUIConnections();

	applyBandwidths(m_settings.m_filterBank[m_settings.m_filterIndex].m_spanLog2, true); // does applySettings(true)
    DialPopup::addPopupsToChildDials(this);
}

SSBDemodGUI::~SSBDemodGUI()
{
	delete ui;
}

bool SSBDemodGUI::blockApplySettings(bool block)
{
    bool ret = !m_doApplySettings;
    m_doApplySettings = !block;
    return ret;
}

void SSBDemodGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
        SSBDemod::MsgConfigureSSBDemod* message = SSBDemod::MsgConfigureSSBDemod::create( m_settings, force);
        m_ssbDemod->getInputMessageQueue()->push(message);
	}
}

unsigned int SSBDemodGUI::spanLog2Max()
{
    unsigned int spanLog2 = 0;
    for (; m_ssbDemod->getAudioSampleRate() / (1<<spanLog2) >= 1000; spanLog2++);
    return spanLog2 == 0 ? 0 : spanLog2-1;
}

void SSBDemodGUI::applyBandwidths(unsigned int spanLog2, bool force)
{
    unsigned int s2max = spanLog2Max();
    spanLog2 = spanLog2 > s2max ? s2max : spanLog2;
    unsigned int limit = s2max < 1 ? 0 : s2max - 1;
    ui->spanLog2->setMaximum(limit);
    bool dsb = ui->dsb->isChecked();
    //int spanLog2 = ui->spanLog2->value();
    m_spectrumRate = m_ssbDemod->getAudioSampleRate() / (1<<spanLog2);
    int bw = ui->BW->value();
    int lw = ui->lowCut->value();
    int bwMax = m_ssbDemod->getAudioSampleRate() / (100*(1<<spanLog2));
    int tickInterval = m_spectrumRate / 1200;
    tickInterval = tickInterval == 0 ? 1 : tickInterval;

    qDebug() << "SSBDemodGUI::applyBandwidths:"
            << " s2max:" << s2max
            << " dsb: " << dsb
            << " spanLog2: " << spanLog2
            << " m_spectrumRate: " << m_spectrumRate
            << " bw: " << bw
            << " lw: " << lw
            << " bwMax: " << bwMax
            << " tickInterval: " << tickInterval;

    ui->BW->setTickInterval(tickInterval);
    ui->lowCut->setTickInterval(tickInterval);

    bw = bw < -bwMax ? -bwMax : bw > bwMax ? bwMax : bw;

    if (bw < 0) {
        lw = lw < bw+1 ? bw+1 : lw < 0 ? lw : 0;
    } else if (bw > 0) {
        lw = lw > bw-1 ? bw-1 : lw < 0 ? 0 : lw;
    } else {
        lw = 0;
    }

    if (dsb)
    {
        bw = bw < 0 ? -bw : bw;
        lw = 0;
    }

    QString spanStr = QString::number(bwMax/10.0, 'f', 1);
    QString bwStr   = QString::number(bw/10.0, 'f', 1);
    QString lwStr   = QString::number(lw/10.0, 'f', 1);

    SpectrumSettings spectrumSettings = m_spectrumVis->getSettings();

    if (dsb)
    {
        ui->BWText->setText(tr("%1%2k").arg(QChar(0xB1, 0x00)).arg(bwStr));
        ui->spanText->setText(tr("%1%2k").arg(QChar(0xB1, 0x00)).arg(spanStr));
        ui->scaleMinus->setText("0");
        ui->scaleCenter->setText("");
        ui->scalePlus->setText(tr("%1").arg(QChar(0xB1, 0x00)));
        ui->lsbLabel->setText("");
        ui->usbLabel->setText("");
        ui->glSpectrum->setCenterFrequency(0);
        ui->glSpectrum->setSampleRate(2*m_spectrumRate);
        spectrumSettings.m_ssb = false;
        ui->glSpectrum->setLsbDisplay(false);
        ui->glSpectrum->setSsbSpectrum(false);
    }
    else
    {
        ui->BWText->setText(tr("%1k").arg(bwStr));
        ui->spanText->setText(tr("%1k").arg(spanStr));
        ui->scaleMinus->setText("-");
        ui->scaleCenter->setText("0");
        ui->scalePlus->setText("+");
        ui->lsbLabel->setText("LSB");
        ui->usbLabel->setText("USB");
        ui->glSpectrum->setCenterFrequency(0);
        ui->glSpectrum->setSampleRate(2*m_spectrumRate);
        spectrumSettings.m_ssb = true;
        ui->glSpectrum->setLsbDisplay(bw < 0);
        ui->glSpectrum->setSsbSpectrum(true);
    }

    SpectrumVis::MsgConfigureSpectrumVis *msg = SpectrumVis::MsgConfigureSpectrumVis::create(spectrumSettings, false);
    m_spectrumVis->getInputMessageQueue()->push(msg);

    ui->lowCutText->setText(tr("%1k").arg(lwStr));

    ui->BW->blockSignals(true);
    ui->lowCut->blockSignals(true);

    ui->BW->setMaximum(bwMax);
    ui->BW->setMinimum(dsb ? 0 : -bwMax);
    ui->BW->setValue(bw);

    ui->lowCut->setMaximum(dsb ? 0 :  bwMax);
    ui->lowCut->setMinimum(dsb ? 0 : -bwMax);
    ui->lowCut->setValue(lw);

    ui->lowCut->blockSignals(false);
    ui->BW->blockSignals(false);

    ui->channelPowerMeter->setRange(SSBDemodSettings::m_minPowerThresholdDB, 0);

    m_settings.m_dsb = dsb;
    m_settings.m_filterBank[m_settings.m_filterIndex].m_spanLog2 = spanLog2;
    m_settings.m_filterBank[m_settings.m_filterIndex].m_rfBandwidth = bw * 100;
    m_settings.m_filterBank[m_settings.m_filterIndex].m_lowCutoff = lw * 100;

    applySettings(force);

    bool wasBlocked = blockApplySettings(true);
    m_channelMarker.setBandwidth(bw * 200);
    m_channelMarker.setSidebands(dsb ? ChannelMarker::dsb : bw < 0 ? ChannelMarker::lsb : ChannelMarker::usb);
    ui->dsb->setIcon(bw < 0 ? m_iconDSBLSB: m_iconDSBUSB);
    if (!dsb) { m_channelMarker.setLowCutoff(lw * 100); }
    blockApplySettings(wasBlocked);
}

void SSBDemodGUI::displaySettings()
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

    ui->flipSidebands->setEnabled(!m_settings.m_dsb);

    if (m_settings.m_dsb)
    {
        m_channelMarker.setSidebands(ChannelMarker::dsb);
    }
    else
    {
        if (m_settings.m_filterBank[m_settings.m_filterIndex].m_rfBandwidth < 0)
        {
            m_channelMarker.setSidebands(ChannelMarker::lsb);
            ui->dsb->setIcon(m_iconDSBLSB);
        }
        else
        {
            m_channelMarker.setSidebands(ChannelMarker::usb);
            ui->dsb->setIcon(m_iconDSBUSB);
        }
    }

    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());

    blockApplySettings(true);

    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());

    ui->agc->setChecked(m_settings.m_agc);
    ui->agcClamping->setChecked(m_settings.m_agcClamping);
    ui->dnr->setChecked(m_settings.m_dnr);
    ui->audioBinaural->setChecked(m_settings.m_audioBinaural);
    ui->audioFlipChannels->setChecked(m_settings.m_audioFlipChannels);
    ui->audioMute->setChecked(m_settings.m_audioMute);
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    ui->fftWindow->setCurrentIndex((int) m_settings.m_filterBank[m_settings.m_filterIndex].m_fftWindow);

    // Prevent uncontrolled triggering of applyBandwidths
    ui->spanLog2->blockSignals(true);
    ui->dsb->blockSignals(true);
    ui->BW->blockSignals(true);
    ui->filterIndex->blockSignals(true);

    ui->filterIndex->setValue(m_settings.m_filterIndex);
    ui->filterIndexText->setText(tr("%1").arg(m_settings.m_filterIndex));

    ui->dsb->setChecked(m_settings.m_dsb);
    ui->spanLog2->setValue(1 + ui->spanLog2->maximum() - m_settings.m_filterBank[m_settings.m_filterIndex].m_spanLog2);

    ui->BW->setValue(m_settings.m_filterBank[m_settings.m_filterIndex].m_rfBandwidth / 100.0);
    QString s = QString::number(m_settings.m_filterBank[m_settings.m_filterIndex].m_rfBandwidth/1000.0, 'f', 1);

    if (m_settings.m_dsb) {
        ui->BWText->setText(tr("%1%2k").arg(QChar(0xB1, 0x00)).arg(s));
    } else {
        ui->BWText->setText(tr("%1k").arg(s));
    }

    ui->spanLog2->blockSignals(false);
    ui->dsb->blockSignals(false);
    ui->BW->blockSignals(false);
    ui->filterIndex->blockSignals(false);

    // The only one of the four signals triggering applyBandwidths will trigger it once only with all other values
    // set correctly and therefore validate the settings and apply them to dependent widgets
    ui->lowCut->setValue(m_settings.m_filterBank[m_settings.m_filterIndex].m_lowCutoff / 100.0);
    ui->lowCutText->setText(tr("%1k").arg(m_settings.m_filterBank[m_settings.m_filterIndex].m_lowCutoff / 1000.0));

    int volume = CalcDb::dbPower(m_settings.m_volume);
    ui->volume->setValue(volume);
    ui->volumeText->setText(QString("%1").arg(volume));

    ui->agcTimeLog2->setValue(m_settings.m_agcTimeLog2);
    s = QString::number((1<<ui->agcTimeLog2->value()), 'f', 0);
    ui->agcTimeText->setText(s);

    ui->agcPowerThreshold->setValue(m_settings.m_agcPowerThreshold);
    displayAGCPowerThreshold(ui->agcPowerThreshold->value());
    displayAGCThresholdGate(m_settings.m_agcThresholdGate);

    updateIndexLabel();

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void SSBDemodGUI::displayAGCPowerThreshold(int value)
{
    if (value == SSBDemodSettings::m_minPowerThresholdDB)
    {
        ui->agcPowerThresholdText->setText("---");
    }
    else
    {
        QString s = QString::number(value, 'f', 0);
        ui->agcPowerThresholdText->setText(s);
    }
}

void SSBDemodGUI::displayAGCThresholdGate(int value)
{
    QString s = QString::number(value, 'f', 0);
    ui->agcThresholdGateText->setText(s);
    int dialValue = value;

    if (value > 20) {
        dialValue = ((value - 20) / 10) + 20;
    }

    ui->agcThresholdGate->setValue(dialValue);
}

void SSBDemodGUI::leaveEvent(QEvent* event)
{
	m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void SSBDemodGUI::enterEvent(EnterEventType* event)
{
	m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void SSBDemodGUI::audioSelect(const QPoint& p)
{
    AudioSelectDialog audioSelect(DSPEngine::instance()->getAudioDeviceManager(), m_settings.m_audioDeviceName);
    audioSelect.move(p);
    audioSelect.exec();

    if (audioSelect.m_selected)
    {
        m_settings.m_audioDeviceName = audioSelect.m_audioDeviceName;
        applySettings();
    }
}

void SSBDemodGUI::dnrSetupDialog(const QPoint& p)
{
    m_fftNRDialog = new FFTNRDialog();
    m_fftNRDialog->move(p);
    QObject::connect(m_fftNRDialog, &FFTNRDialog::valueChanged, this, &SSBDemodGUI::dnrSetup);
    m_fftNRDialog->setScheme((FFTNoiseReduction::Scheme) m_settings.m_dnrScheme);
    m_fftNRDialog->setAboveAvgFactor(m_settings.m_dnrAboveAvgFactor);
    m_fftNRDialog->setSigmaFactor(m_settings.m_dnrSigmaFactor);
    m_fftNRDialog->setNbPeaks(m_settings.m_dnrNbPeaks);
    m_fftNRDialog->setAlpha(m_settings.m_dnrAlpha, 2048, m_audioSampleRate);
    m_fftNRDialog->exec();
    QObject::disconnect(m_fftNRDialog, &FFTNRDialog::valueChanged, this, &SSBDemodGUI::dnrSetup);
    m_fftNRDialog->deleteLater();
    m_fftNRDialog = nullptr;
}

void SSBDemodGUI::dnrSetup(int32_t iValueChanged)
{
    if (!m_fftNRDialog) {
        return;
    }

    FFTNRDialog::ValueChanged valueChanged = (FFTNRDialog::ValueChanged) iValueChanged;

    switch (valueChanged)
    {
    case FFTNRDialog::ValueChanged::ChangedScheme:
        m_settings.m_dnrScheme = m_fftNRDialog->getScheme();
        m_settings.m_filterBank[m_settings.m_filterIndex].m_dnrScheme = m_settings.m_dnrScheme;
        applySettings();
        break;
    case FFTNRDialog::ValueChanged::ChangedAboveAvgFactor:
        m_settings.m_dnrAboveAvgFactor = m_fftNRDialog->getAboveAvgFactor();
        m_settings.m_filterBank[m_settings.m_filterIndex].m_dnrAboveAvgFactor = m_settings.m_dnrAboveAvgFactor;
        applySettings();
        break;
    case FFTNRDialog::ValueChanged::ChangedSigmaFactor:
        m_settings.m_dnrSigmaFactor = m_fftNRDialog->getSigmaFactor();
        m_settings.m_filterBank[m_settings.m_filterIndex].m_dnrSigmaFactor = m_settings.m_dnrSigmaFactor;
        applySettings();
        break;
    case FFTNRDialog::ValueChanged::ChangedNbPeaks:
        m_settings.m_dnrNbPeaks = m_fftNRDialog->getNbPeaks();
        m_settings.m_filterBank[m_settings.m_filterIndex].m_dnrNbPeaks = m_settings.m_dnrNbPeaks;
        applySettings();
        break;
    case FFTNRDialog::ValueChanged::ChangedAlpha:
        m_settings.m_dnrAlpha = m_fftNRDialog->getAlpha();
        m_settings.m_filterBank[m_settings.m_filterIndex].m_dnrAlpha = m_settings.m_dnrAlpha;
        applySettings();
        break;
    default:
        break;
    }
}

void SSBDemodGUI::tick()
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    m_ssbDemod->getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
    double powDbAvg = CalcDb::dbPower(magsqAvg);
    double powDbPeak = CalcDb::dbPower(magsqPeak);

    ui->channelPowerMeter->levelChanged(
            (SSBDemodSettings::m_mminPowerThresholdDBf + powDbAvg) / SSBDemodSettings::m_mminPowerThresholdDBf,
            (SSBDemodSettings::m_mminPowerThresholdDBf + powDbPeak) / SSBDemodSettings::m_mminPowerThresholdDBf,
            nbMagsqSamples);

    if (m_tickCount % 4 == 0) {
        ui->channelPower->setText(tr("%1 dB").arg(powDbAvg, 0, 'f', 1));
    }

    int audioSampleRate = m_ssbDemod->getAudioSampleRate();
    bool squelchOpen = m_ssbDemod->getAudioActive();

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

    m_tickCount++;
}

void SSBDemodGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &SSBDemodGUI::on_deltaFrequency_changed);
    QObject::connect(ui->audioBinaural, &QToolButton::toggled, this, &SSBDemodGUI::on_audioBinaural_toggled);
    QObject::connect(ui->audioFlipChannels, &QToolButton::toggled, this, &SSBDemodGUI::on_audioFlipChannels_toggled);
    QObject::connect(ui->dsb, &QToolButton::toggled, this, &SSBDemodGUI::on_dsb_toggled);
    QObject::connect(ui->BW, &TickedSlider::valueChanged, this, &SSBDemodGUI::on_BW_valueChanged);
    QObject::connect(ui->lowCut, &TickedSlider::valueChanged, this, &SSBDemodGUI::on_lowCut_valueChanged);
    QObject::connect(ui->volume, &QDial::valueChanged, this, &SSBDemodGUI::on_volume_valueChanged);
    QObject::connect(ui->agc, &ButtonSwitch::toggled, this, &SSBDemodGUI::on_agc_toggled);
    QObject::connect(ui->agcClamping, &ButtonSwitch::toggled, this, &SSBDemodGUI::on_agcClamping_toggled);
    QObject::connect(ui->dnr, &ButtonSwitch::toggled, this, &SSBDemodGUI::on_dnr_toggled);
    QObject::connect(ui->agcTimeLog2, &QDial::valueChanged, this, &SSBDemodGUI::on_agcTimeLog2_valueChanged);
    QObject::connect(ui->agcPowerThreshold, &QDial::valueChanged, this, &SSBDemodGUI::on_agcPowerThreshold_valueChanged);
    QObject::connect(ui->agcThresholdGate, &QDial::valueChanged, this, &SSBDemodGUI::on_agcThresholdGate_valueChanged);
    QObject::connect(ui->audioMute, &QToolButton::toggled, this, &SSBDemodGUI::on_audioMute_toggled);
    QObject::connect(ui->spanLog2, &QSlider::valueChanged, this, &SSBDemodGUI::on_spanLog2_valueChanged);
    QObject::connect(ui->flipSidebands, &QPushButton::clicked, this, &SSBDemodGUI::on_flipSidebands_clicked);
    QObject::connect(ui->fftWindow, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SSBDemodGUI::on_fftWindow_currentIndexChanged);
    QObject::connect(ui->filterIndex, &QDial::valueChanged, this, &SSBDemodGUI::on_filterIndex_valueChanged);
}

void SSBDemodGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}
