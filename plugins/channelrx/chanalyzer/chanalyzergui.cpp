///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#include "device/deviceuiset.h"
#include "dsp/spectrumvis.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "gui/glspectrum.h"
#include "gui/glscope.h"
#include "gui/basicchannelsettingsdialog.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "maincore.h"

#include "ui_chanalyzergui.h"
#include "chanalyzer.h"
#include "chanalyzergui.h"

ChannelAnalyzerGUI* ChannelAnalyzerGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
    ChannelAnalyzerGUI* gui = new ChannelAnalyzerGUI(pluginAPI, deviceUISet, rxChannel);
	return gui;
}

void ChannelAnalyzerGUI::destroy()
{
	delete this;
}

void ChannelAnalyzerGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
}

void ChannelAnalyzerGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setBandwidth(m_settings.m_bandwidth * 2);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.setLowCutoff(m_settings.m_lowCutoff);

    if (m_settings.m_ssb)
    {
        if (m_settings.m_bandwidth < 0) {
            m_channelMarker.setSidebands(ChannelMarker::lsb);
        } else {
            m_channelMarker.setSidebands(ChannelMarker::usb);
        }
    }
    else
    {
        m_channelMarker.setSidebands(ChannelMarker::dsb);
    }

    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());

    blockApplySettings(true);

    ui->useRationalDownsampler->setChecked(m_settings.m_rationalDownSample);
    setSinkSampleRate();
    if (m_settings.m_ssb) {
        ui->BWLabel->setText("LP");
    } else {
        ui->BWLabel->setText("BP");
    }
    ui->ssb->setChecked(m_settings.m_ssb);
    ui->BW->setValue(m_settings.m_bandwidth/100);
    ui->lowCut->setValue(m_settings.m_lowCutoff/100);
    ui->deltaFrequency->setValue(m_settings.m_inputFrequencyOffset);
    ui->log2Decim->setCurrentIndex(m_settings.m_log2Decim);
    displayPLLSettings();
    ui->signalSelect->setCurrentIndex((int) m_settings.m_inputType);
    ui->rrcFilter->setChecked(m_settings.m_rrc);
    QString rolloffStr = QString::number(m_settings.m_rrcRolloff/100.0, 'f', 2);
    ui->rrcRolloffText->setText(rolloffStr);

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void ChannelAnalyzerGUI::displayPLLSettings()
{
    if (m_settings.m_costasLoop)
        ui->pllType->setCurrentIndex(2);
    else if (m_settings.m_fll)
        ui->pllType->setCurrentIndex(1);
    else
        ui->pllType->setCurrentIndex(0);
    setPLLVisibility();

    int i = 0;
    for(; ((m_settings.m_pllPskOrder>>i) & 1) == 0; i++);
    if (m_settings.m_costasLoop)
        ui->pllPskOrder->setCurrentIndex(i==0 ? 0 : i-1);
    else
        ui->pllPskOrder->setCurrentIndex(i);

    ui->pll->setChecked(m_settings.m_pll);
    ui->pllBandwidth->setValue((int)(m_settings.m_pllBandwidth*1000.0));
    QString bandwidthStr = QString::number(m_settings.m_pllBandwidth, 'f', 3);
    ui->pllBandwidthText->setText(bandwidthStr);
    ui->pllDampingFactor->setValue((int)(m_settings.m_pllDampingFactor*10.0));
    QString factorStr = QString::number(m_settings.m_pllDampingFactor, 'f', 1);
    ui->pllDampingFactorText->setText(factorStr);
    ui->pllLoopGain->setValue((int)(m_settings.m_pllLoopGain));
    QString gainStr = QString::number(m_settings.m_pllLoopGain, 'f', 0);
    ui->pllLoopGainText->setText(gainStr);
}

void ChannelAnalyzerGUI::setPLLVisibility()
{
    ui->pllToolbar->setVisible(m_settings.m_pll);

    // BW
    ui->pllPskOrder->setVisible(!m_settings.m_fll);
    ui->pllLine1->setVisible(!m_settings.m_fll);
    ui->pllBandwidthLabel->setVisible(!m_settings.m_fll);
    ui->pllBandwidth->setVisible(!m_settings.m_fll);
    ui->pllBandwidthText->setVisible(!m_settings.m_fll);
    ui->pllLine2->setVisible(!m_settings.m_fll);

    // Damping factor and gain
    bool stdPll = !m_settings.m_fll && !m_settings.m_costasLoop;
    ui->pllDamplingFactor->setVisible(stdPll);
    ui->pllDampingFactor->setVisible(stdPll);
    ui->pllDampingFactorText->setVisible(stdPll);
    ui->pllLine3->setVisible(stdPll);
    ui->pllLoopGainLabel->setVisible(stdPll);
    ui->pllLoopGain->setVisible(stdPll);
    ui->pllLoopGainText->setVisible(stdPll);
    ui->pllLine4->setVisible(stdPll);

    // Order
    ui->pllPskOrder->blockSignals(true);
    ui->pllPskOrder->clear();
    if (stdPll)
    {
        ui->pllPskOrder->addItem("CW");
        ui->pllPskOrder->addItem("BPSK");
        ui->pllPskOrder->addItem("QPSK");
        ui->pllPskOrder->addItem("8PSK");
        ui->pllPskOrder->addItem("16PSK");
    }
    else if (m_settings.m_costasLoop)
    {
        ui->pllPskOrder->addItem("BPSK");
        ui->pllPskOrder->addItem("QPSK");
        ui->pllPskOrder->addItem("8PSK");
        if (m_settings.m_pllPskOrder < 2)
            m_settings.m_pllPskOrder = 2;
        else if (m_settings.m_pllPskOrder > 8)
            m_settings.m_pllPskOrder = 8;
    }
    int i = 0;
    for(; ((m_settings.m_pllPskOrder>>i) & 1) == 0; i++);
    if (m_settings.m_costasLoop)
        ui->pllPskOrder->setCurrentIndex(i==0 ? 0 : i-1);
    else
        ui->pllPskOrder->setCurrentIndex(i);
    ui->pllPskOrder->blockSignals(false);
    getRollupContents()->arrangeRollups();
}

void ChannelAnalyzerGUI::setSpectrumDisplay()
{
    int sinkSampleRate = getSinkSampleRate();
    qDebug("ChannelAnalyzerGUI::setSpectrumDisplay: m_sinkSampleRate: %d", sinkSampleRate);
    if (m_settings.m_ssb)
    {
        ui->glSpectrum->setCenterFrequency(sinkSampleRate/4);
        ui->glSpectrum->setSampleRate(sinkSampleRate/2);
        ui->glSpectrum->setSsbSpectrum(true);
        ui->glSpectrum->setLsbDisplay(ui->BW->value() < 0);
    }
    else
    {
        ui->glSpectrum->setCenterFrequency(0);
        ui->glSpectrum->setSampleRate(sinkSampleRate);
        ui->glSpectrum->setSsbSpectrum(false);
        ui->glSpectrum->setLsbDisplay(false);
    }
}

QByteArray ChannelAnalyzerGUI::serialize() const
{
    return m_settings.serialize();
}

bool ChannelAnalyzerGUI::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        displaySettings();
        applySettings(true); // will have true
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        displaySettings();
        applySettings(true); // will have true
        return false;
    }
}

bool ChannelAnalyzerGUI::handleMessage(const Message& message)
{
    if (DSPSignalNotification::match(message))
    {
        DSPSignalNotification& cmd = (DSPSignalNotification&) message;
        m_basebandSampleRate = cmd.getSampleRate();
        m_deviceCenterFrequency = cmd.getCenterFrequency();
        ui->deltaFrequency->setValueRange(false, 8, -m_basebandSampleRate/2, m_basebandSampleRate/2);
        ui->deltaFrequencyLabel->setToolTip(tr("Range %1 %L2 Hz").arg(QChar(0xB1)).arg(m_basebandSampleRate/2));
        updateAbsoluteCenterFrequency();
        qDebug("ChannelAnalyzerGUI::handleMessage: DSPSignalNotification: m_basebandSampleRate: %d", m_basebandSampleRate);
        setSinkSampleRate();

        return true;
    }
    else if (ChannelAnalyzer::MsgConfigureChannelAnalyzer::match(message))
    {
        qDebug("ChannelAnalyzerGUI::handleMessage: ChannelAnalyzer::MsgConfigureChannelAnalyzer");
        const ChannelAnalyzer::MsgConfigureChannelAnalyzer& cfg = (ChannelAnalyzer::MsgConfigureChannelAnalyzer&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        ui->spectrumGUI->updateSettings();
        ui->scopeGUI->updateSettings();
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        blockApplySettings(false);

        return true;
    }

    return false;
}

void ChannelAnalyzerGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()) != 0)
    {
        qDebug("ChannelAnalyzerGUI::handleInputMessages: message: %s", message->getIdentifier());

        if (handleMessage(*message))
        {
            delete message;
        }
    }
}

void ChannelAnalyzerGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
	applySettings();
}

void ChannelAnalyzerGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void ChannelAnalyzerGUI::tick()
{
	m_channelPowerAvg(m_channelAnalyzer->getMagSqAvg());
	double powDb = CalcDb::dbPower((double) m_channelPowerAvg);
	ui->channelPower->setText(tr("%1 dB").arg(powDb, 0, 'f', 1));

	if (m_channelAnalyzer->isPllLocked()) {
	    ui->pll->setStyleSheet("QToolButton { background-color : green; }");
	} else {
	    ui->pll->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
	}

	if (ui->pll->isChecked())
	{
        double sampleRate = (double) m_channelAnalyzer->getChannelSampleRate();
		int freq = (m_channelAnalyzer->getPllFrequency() * sampleRate) / (2.0*M_PI);
		ui->pll->setToolTip(tr("PLL lock. Freq = %1 Hz").arg(freq));
		ui->pllLockFrequency->setText(tr("%1 Hz").arg(freq));
	}
}

void ChannelAnalyzerGUI::on_rationalDownSamplerRate_changed(quint64 value)
{
    m_settings.m_rationalDownSamplerRate = value;
    setSinkSampleRate();
    applySettings();
}

void ChannelAnalyzerGUI::on_pll_toggled(bool checked)
{
	if (!checked) {
		ui->pll->setToolTip(tr("PLL lock"));
	}

	m_settings.m_pll = checked;
    setPLLVisibility();
    applySettings();
}

void ChannelAnalyzerGUI::on_pllType_currentIndexChanged(int index)
{
    m_settings.m_fll = (index == 1);
    m_settings.m_costasLoop = (index == 2);
    setPLLVisibility();
    applySettings();
}

void ChannelAnalyzerGUI::on_pllPskOrder_currentIndexChanged(int index)
{
    if (m_settings.m_costasLoop)
        m_settings.m_pllPskOrder = (1<<(index+1));
    else
        m_settings.m_pllPskOrder = (1<<index);
    applySettings();
}

void ChannelAnalyzerGUI::on_pllBandwidth_valueChanged(int value)
{
    m_settings.m_pllBandwidth = value/1000.0;
    QString bandwidthStr = QString::number(m_settings.m_pllBandwidth, 'f', 3);
    ui->pllBandwidthText->setText(bandwidthStr);
    applySettings();
}

void ChannelAnalyzerGUI::on_pllDampingFactor_valueChanged(int value)
{
    m_settings.m_pllDampingFactor = value/10.0;
    QString factorStr = QString::number(m_settings.m_pllDampingFactor, 'f', 1);
    ui->pllDampingFactorText->setText(factorStr);
    applySettings();
}

void ChannelAnalyzerGUI::on_pllLoopGain_valueChanged(int value)
{
    m_settings.m_pllLoopGain = value;
    QString gainStr = QString::number(m_settings.m_pllLoopGain, 'f', 0);
    ui->pllLoopGainText->setText(gainStr);
    applySettings();
}

void ChannelAnalyzerGUI::on_useRationalDownsampler_toggled(bool checked)
{
    m_settings.m_rationalDownSample = checked;
    setSinkSampleRate();
    applySettings();
}

void ChannelAnalyzerGUI::on_signalSelect_currentIndexChanged(int index)
{
    m_settings.m_inputType = (ChannelAnalyzerSettings::InputType) index;

    if (m_settings.m_inputType == ChannelAnalyzerSettings::InputAutoCorr) {
        m_scopeVis->setTraceChunkSize(ChannelAnalyzerSink::m_corrFFTLen);
    } else {
        m_scopeVis->setTraceChunkSize(GLScopeSettings::m_traceChunkDefaultSize);
    }

    ui->scopeGUI->traceLengthChange();
    applySettings();
}

void ChannelAnalyzerGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings();
}

void ChannelAnalyzerGUI::on_rrcFilter_toggled(bool checked)
{
    m_settings.m_rrc = checked;
    applySettings();
}

void ChannelAnalyzerGUI::on_rrcRolloff_valueChanged(int value)
{
    m_settings.m_rrcRolloff = value;
    QString rolloffStr = QString::number(value/100.0, 'f', 2);
    ui->rrcRolloffText->setText(rolloffStr);
    applySettings();
}

void ChannelAnalyzerGUI::on_BW_valueChanged(int value)
{
    (void) value;
    setFiltersUIBoundaries();
    m_settings.m_bandwidth = ui->BW->value() * 100;
    m_settings.m_lowCutoff = ui->lowCut->value() * 100;
	applySettings();
}

void ChannelAnalyzerGUI::on_lowCut_valueChanged(int value)
{
    (void) value;
	setFiltersUIBoundaries();
	m_settings.m_bandwidth = ui->BW->value() * 100;
	m_settings.m_lowCutoff = ui->lowCut->value() * 100;
	applySettings();
}

void ChannelAnalyzerGUI::on_log2Decim_currentIndexChanged(int index)
{
    if ((index < 0) || (index > 6)) {
        return;
    }

    m_settings.m_log2Decim = index;
    setSinkSampleRate();
    applySettings();
}

void ChannelAnalyzerGUI::on_ssb_toggled(bool checked)
{
	m_settings.m_ssb = checked;
	if (checked) {
	    ui->BWLabel->setText("LP");
	} else {
	    ui->BWLabel->setText("BP");
	}
    setFiltersUIBoundaries();
    applySettings();
}

void ChannelAnalyzerGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

void ChannelAnalyzerGUI::onMenuDialogCalled(const QPoint& p)
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
            dialog.setNumberOfStreams(m_channelAnalyzer->getNumberOfDeviceStreams());
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

ChannelAnalyzerGUI::ChannelAnalyzerGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
	ChannelGUI(parent),
	ui(new Ui::ChannelAnalyzerGUI),
	m_pluginAPI(pluginAPI),
	m_deviceUISet(deviceUISet),
	m_channelMarker(this),
	m_doApplySettings(true),
	m_basebandSampleRate(48000)
{
	setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/chanalyzer/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
	connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

	m_channelAnalyzer = (ChannelAnalyzer*) rxChannel;
    m_basebandSampleRate = m_channelAnalyzer->getChannelSampleRate();
    qDebug("ChannelAnalyzerGUI::ChannelAnalyzerGUI: m_basebandSampleRate: %d", m_basebandSampleRate);
    m_spectrumVis = m_channelAnalyzer->getSpectrumVis();
	m_spectrumVis->setGLSpectrum(ui->glSpectrum);
    m_scopeVis = m_channelAnalyzer->getScopeVis();
    m_scopeVis->setGLScope(ui->glScope);
    m_basebandSampleRate = m_channelAnalyzer->getChannelSampleRate();
    m_scopeVis->setSpectrumVis(m_spectrumVis);
    m_channelAnalyzer->setScopeVis(m_scopeVis);
	m_channelAnalyzer->setMessageQueueToGUI(getInputMessageQueue());

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 8, -99999999, 99999999);

	ui->rationalDownSamplerRate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));

	ui->glSpectrum->setCenterFrequency(m_basebandSampleRate/2);
	ui->glSpectrum->setSampleRate(m_basebandSampleRate);
	ui->glSpectrum->setDisplayWaterfall(true);
	ui->glSpectrum->setDisplayMaxHold(true);
	ui->glSpectrum->setSsbSpectrum(false);
    ui->glSpectrum->setLsbDisplay(false);

	ui->glScope->connectTimer(MainCore::instance()->getMasterTimer());
	connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

	m_channelMarker.blockSignals(true);
	m_channelMarker.setColor(Qt::gray);
	m_channelMarker.setBandwidth(m_basebandSampleRate);
	m_channelMarker.setSidebands(ChannelMarker::usb);
	m_channelMarker.setCenterFrequency(0);
	m_channelMarker.setTitle("Channel Analyzer");
    m_channelMarker.blockSignals(false);
	m_channelMarker.setVisible(true); // activate signal on the last setting only
	setTitleColor(m_channelMarker.getColor());

	m_deviceUISet->addChannelMarker(&m_channelMarker);

	ui->spectrumGUI->setBuddies(m_spectrumVis, ui->glSpectrum);
	ui->scopeGUI->setBuddies(m_scopeVis->getInputMessageQueue(), m_scopeVis, ui->glScope);

	m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setRollupState(&m_rollupState);
    m_settings.setSpectrumGUI(ui->spectrumGUI);
    m_settings.setScopeGUI(ui->scopeGUI);

	connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()), this, SLOT(channelMarkerHighlightedByCursor()));
	connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

	displaySettings();
    makeUIConnections();
	applySettings(true);
}

ChannelAnalyzerGUI::~ChannelAnalyzerGUI()
{
    qDebug("ChannelAnalyzerGUI::~ChannelAnalyzerGUI");
	ui->glScope->disconnectTimer();
	delete ui;
    qDebug("ChannelAnalyzerGUI::~ChannelAnalyzerGUI: done");
}

int ChannelAnalyzerGUI::getSinkSampleRate()
{
    return m_settings.m_rationalDownSample ?
        m_settings.m_rationalDownSamplerRate
        : m_basebandSampleRate / (1<<m_settings.m_log2Decim);
}

void ChannelAnalyzerGUI::setSinkSampleRate()
{
    unsigned int nominalSinkSampleRate = m_basebandSampleRate / (1<<m_settings.m_log2Decim);

    ui->rationalDownSamplerRate->setValueRange(7, 0.5*nominalSinkSampleRate, nominalSinkSampleRate);
    ui->rationalDownSamplerRate->setValue(m_settings.m_rationalDownSamplerRate);
    m_settings.m_rationalDownSamplerRate = ui->rationalDownSamplerRate->getValueNew();

    unsigned int sinkSampleRate = getSinkSampleRate();

	qDebug("ChannelAnalyzerGUI::setSinkSampleRate: nominalSinkSampleRate: %u sinkSampleRate: %u",
        nominalSinkSampleRate, sinkSampleRate);

	setFiltersUIBoundaries();

	QString s = QString::number(sinkSampleRate/1000.0, 'f', 1);
	ui->sinkSampleRateText->setText(tr("%1 kS/s").arg(s));

	m_scopeVis->setLiveRate(sinkSampleRate == 0 ? 48000 : sinkSampleRate);
    ui->scopeGUI->setSampleRate(sinkSampleRate == 0 ? 48000 : sinkSampleRate);
}

void ChannelAnalyzerGUI::setFiltersUIBoundaries()
{
    int sinkSampleRate = getSinkSampleRate();
    bool dsb = !ui->ssb->isChecked();
    int bw = ui->BW->value();
    int lw = ui->lowCut->value();
    int bwMax = sinkSampleRate / 200;

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

    QString bwStr = QString::number(bw/10.0, 'f', 1);
    QString lwStr = QString::number(lw/10.0, 'f', 1);

    if (dsb) {
        ui->BWText->setText(tr("%1%2k").arg(QChar(0xB1, 0x00)).arg(bwStr));
    } else {
        ui->BWText->setText(tr("%1k").arg(bwStr));
    }

    ui->lowCutText->setText(tr("%1k").arg(lwStr));

    ui->BW->blockSignals(true);
    ui->lowCut->blockSignals(true);

    ui->BW->setMaximum(bwMax);
    ui->BW->setMinimum(dsb ? 0 : -bwMax);
    ui->BW->setValue(bw);

    ui->lowCut->setMaximum(dsb ? 0 :  bw);
    ui->lowCut->setMinimum(dsb ? 0 : -bw);
    ui->lowCut->setValue(lw);

    ui->lowCut->blockSignals(false);
    ui->BW->blockSignals(false);

    setSpectrumDisplay();

    m_channelMarker.setBandwidth(bw * 200);
    m_channelMarker.setSidebands(dsb ? ChannelMarker::dsb : bw < 0 ? ChannelMarker::lsb : ChannelMarker::usb);

    if (!dsb) {
        m_channelMarker.setLowCutoff(lw * 100);
    }
}

void ChannelAnalyzerGUI::blockApplySettings(bool block)
{
    ui->glScope->blockSignals(block);
    ui->glSpectrum->blockSignals(block);
    m_doApplySettings = !block;
}

void ChannelAnalyzerGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
        ChannelAnalyzer::MsgConfigureChannelAnalyzer* message =
                ChannelAnalyzer::MsgConfigureChannelAnalyzer::create( m_settings, force);
        m_channelAnalyzer->getInputMessageQueue()->push(message);
	}
}

void ChannelAnalyzerGUI::leaveEvent(QEvent* event)
{
	m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void ChannelAnalyzerGUI::enterEvent(EnterEventType* event)
{
	m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void ChannelAnalyzerGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &ChannelAnalyzerGUI::on_deltaFrequency_changed);
    QObject::connect(ui->rationalDownSamplerRate, &ValueDial::changed, this, &ChannelAnalyzerGUI::on_rationalDownSamplerRate_changed);
    QObject::connect(ui->pll, &QToolButton::toggled, this, &ChannelAnalyzerGUI::on_pll_toggled);
    QObject::connect(ui->pllType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ChannelAnalyzerGUI::on_pllType_currentIndexChanged);
    QObject::connect(ui->pllPskOrder, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ChannelAnalyzerGUI::on_pllPskOrder_currentIndexChanged);
    QObject::connect(ui->pllBandwidth, &QDial::valueChanged, this, &ChannelAnalyzerGUI::on_pllBandwidth_valueChanged);
    QObject::connect(ui->pllDampingFactor, &QDial::valueChanged, this, &ChannelAnalyzerGUI::on_pllDampingFactor_valueChanged);
    QObject::connect(ui->pllLoopGain, &QDial::valueChanged, this, &ChannelAnalyzerGUI::on_pllLoopGain_valueChanged);
    QObject::connect(ui->log2Decim, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ChannelAnalyzerGUI::on_log2Decim_currentIndexChanged);
    QObject::connect(ui->useRationalDownsampler, &ButtonSwitch::toggled, this, &ChannelAnalyzerGUI::on_useRationalDownsampler_toggled);
    QObject::connect(ui->signalSelect, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ChannelAnalyzerGUI::on_signalSelect_currentIndexChanged);
    QObject::connect(ui->rrcFilter, &ButtonSwitch::toggled, this, &ChannelAnalyzerGUI::on_rrcFilter_toggled);
    QObject::connect(ui->rrcRolloff, &QDial::valueChanged, this, &ChannelAnalyzerGUI::on_rrcRolloff_valueChanged);
    QObject::connect(ui->BW, &QSlider::valueChanged, this, &ChannelAnalyzerGUI::on_BW_valueChanged);
    QObject::connect(ui->lowCut, &QSlider::valueChanged, this, &ChannelAnalyzerGUI::on_lowCut_valueChanged);
    QObject::connect(ui->ssb, &QCheckBox::toggled, this, &ChannelAnalyzerGUI::on_ssb_toggled);
}

void ChannelAnalyzerGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}
