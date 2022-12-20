///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#include <QLocale>

#include "device/deviceuiset.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "gui/dialogpositioner.h"
#include "dsp/hbfilterchainconverter.h"
#include "dsp/dspcommands.h"

#include "localsinkgui.h"
#include "localsink.h"
#include "ui_localsinkgui.h"

LocalSinkGUI* LocalSinkGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *channelRx)
{
    LocalSinkGUI* gui = new LocalSinkGUI(pluginAPI, deviceUISet, channelRx);
    return gui;
}

void LocalSinkGUI::destroy()
{
    delete this;
}

void LocalSinkGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray LocalSinkGUI::serialize() const
{
    return m_settings.serialize();
}

bool LocalSinkGUI::deserialize(const QByteArray& data)
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

bool LocalSinkGUI::handleMessage(const Message& message)
{
    if (DSPSignalNotification::match(message))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) message;
        //m_channelMarker.setBandwidth(notif.getSampleRate());
        m_deviceCenterFrequency = notif.getCenterFrequency();
        m_basebandSampleRate = notif.getSampleRate();
        updateAbsoluteCenterFrequency();
        displayRateAndShift();
        displayFFTBand();
        return true;
    }
    else if (LocalSink::MsgConfigureLocalSink::match(message))
    {
        const LocalSink::MsgConfigureLocalSink& cfg = (LocalSink::MsgConfigureLocalSink&) message;

        if (cfg.getForce()) {
            m_settings = cfg.getSettings();
        } else {
            m_settings.applySettings(cfg.getSettingsKeys(), cfg.getSettings());
        }

        blockApplySettings(true);
        ui->spectrumGUI->updateSettings();
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (LocalSink::MsgReportDevices::match(message))
    {
        LocalSink::MsgReportDevices& report = (LocalSink::MsgReportDevices&) message;
        updateDeviceSetList(report.getDeviceSetIndexes());
        return true;
    }
    else
    {
        return false;
    }
}

LocalSinkGUI::LocalSinkGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *channelrx, QWidget* parent) :
        ChannelGUI(parent),
        ui(new Ui::LocalSinkGUI),
        m_pluginAPI(pluginAPI),
        m_deviceUISet(deviceUISet),
        m_currentBandIndex(-1),
        m_showFilterHighCut(false),
        m_deviceCenterFrequency(0),
        m_basebandSampleRate(0),
        m_tickCount(0)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/localsink/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_localSink = (LocalSink*) channelrx;
    m_spectrumVis = m_localSink->getSpectrumVis();
	m_spectrumVis->setGLSpectrum(ui->glSpectrum);
    m_localSink->setMessageQueueToGUI(getInputMessageQueue());

    ui->glSpectrum->setCenterFrequency(m_deviceCenterFrequency);
    ui->glSpectrum->setSampleRate(m_basebandSampleRate);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(m_settings.m_rgbColor);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("Local Sink");
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setSpectrumGUI(ui->spectrumGUI);
    m_settings.setRollupState(&m_rollupState);

    m_deviceUISet->addChannelMarker(&m_channelMarker);

    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));
    ui->spectrumGUI->setBuddies(m_spectrumVis, ui->glSpectrum);

    updateDeviceSetList(m_localSink->getDeviceSetList());
    displaySettings();
    makeUIConnections();
    applySettings(true);
}

LocalSinkGUI::~LocalSinkGUI()
{
    delete ui;
}

void LocalSinkGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void LocalSinkGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        setTitleColor(m_channelMarker.getColor());

        LocalSink::MsgConfigureLocalSink* message = LocalSink::MsgConfigureLocalSink::create(m_settings, m_settingsKeys, force);
        m_localSink->getInputMessageQueue()->push(message);
    }

    m_settingsKeys.clear();
}

void LocalSinkGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.setBandwidth(m_basebandSampleRate / (1<<m_settings.m_log2Decim));
    m_channelMarker.setMovable(false); // do not let user move the center arbitrarily
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());

    blockApplySettings(true);
    int index = getLocalDeviceIndexInCombo(m_settings.m_localDeviceIndex);

    if (index >= 0) {
        ui->localDevice->setCurrentIndex(index);
    }

    ui->localDevicePlay->setChecked(m_settings.m_play);
    ui->decimationFactor->setCurrentIndex(m_settings.m_log2Decim);
    ui->dsp->setChecked(m_settings.m_dsp);
    ui->gain->setValue(m_settings.m_gaindB);
    ui->gainText->setText(tr("%1").arg(m_settings.m_gaindB));
    ui->fft->setChecked(m_settings.m_fftOn);
    ui->fftSize->setCurrentIndex(m_settings.m_log2FFT-6);
    ui->fftWindow->setCurrentIndex((int) m_settings.m_fftWindow);
    ui->fftFilterReverse->setChecked(m_settings.m_reverseFilter);
    ui->filterF2orW->setChecked(m_showFilterHighCut);
    applyDecimation();
    updateIndexLabel();
    displayFFTBand(false);

    getRollupContents()->restoreState(m_rollupState);
    blockApplySettings(false);
}

void LocalSinkGUI::displayRateAndShift()
{
    int shift = m_shiftFrequencyFactor * m_basebandSampleRate;
    double channelSampleRate = ((double) m_basebandSampleRate) / (1<<m_settings.m_log2Decim);
    QLocale loc;
    ui->offsetFrequencyText->setText(tr("%1 Hz").arg(loc.toString(shift)));
    ui->channelRateText->setText(tr("%1k").arg(QString::number(channelSampleRate / 1000.0, 'g', 5)));
    ui->glSpectrum->setSampleRate(channelSampleRate);

    if (ui->relativeSpectrum->isChecked()) {
        ui->glSpectrum->setCenterFrequency(0);
    } else {
        ui->glSpectrum->setCenterFrequency(m_deviceCenterFrequency + shift);
    }

    m_channelMarker.setCenterFrequency(shift);
    m_channelMarker.setBandwidth(channelSampleRate);
}

void LocalSinkGUI::displayFFTBand(bool blockApplySettings)
{
    if (blockApplySettings) {
        this->blockApplySettings(true);
    }

    ui->bandIndex->setMaximum(m_settings.m_fftBands.size() != 0 ? m_settings.m_fftBands.size() - 1 : 0);
    ui->bandIndex->setEnabled(m_settings.m_fftBands.size() != 0);
    ui->f1->setEnabled(m_settings.m_fftBands.size() != 0);
    ui->bandWidth->setEnabled(m_settings.m_fftBands.size() != 0);

    if ((m_settings.m_fftBands.size() != 0) && (m_currentBandIndex < 0)) {
        m_currentBandIndex = 0;
    }

    if (m_currentBandIndex >= 0)
    {
        ui->bandIndex->setValue(m_currentBandIndex);
        m_currentBandIndex = ui->bandIndex->value();
        ui->bandIndexText->setText(tr("%1").arg(m_currentBandIndex));
        ui->f1->setValue(m_settings.m_fftBands[m_currentBandIndex].first*1000);
        ui->bandWidth->setValue(m_settings.m_fftBands[m_currentBandIndex].second*1000);
        double channelSampleRate = ((double) m_basebandSampleRate) / (1<<m_settings.m_log2Decim);
        double f1 = (m_settings.m_fftBands[m_currentBandIndex].first)*channelSampleRate;
        double w  = (m_settings.m_fftBands[m_currentBandIndex].second)*channelSampleRate;
        ui->f1Text->setText(displayScaled(f1, 5));

        if (m_showFilterHighCut)
        {
            ui->bandwidthText->setToolTip("Filter high cut frequency");
            double f2 = f1 + w;
            ui->bandwidthText->setText(displayScaled(f2, 5));
        }
        else
        {
            ui->bandwidthText->setToolTip("Filter width");
            ui->bandwidthText->setText(displayScaled(w, 5));
        }
    }

    if (blockApplySettings) {
        this->blockApplySettings(false);
    }
}

int LocalSinkGUI::getLocalDeviceIndexInCombo(int localDeviceIndex)
{
    int index = 0;

    for (; index < ui->localDevice->count(); index++)
    {
        if (localDeviceIndex == ui->localDevice->itemData(index).toInt()) {
            return index;
        }
    }

    return -1;
}
void LocalSinkGUI::leaveEvent(QEvent* event)
{
    m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void LocalSinkGUI::enterEvent(EnterEventType* event)
{
    m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void LocalSinkGUI::handleSourceMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()) != 0)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void LocalSinkGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
}

void LocalSinkGUI::onMenuDialogCalled(const QPoint &p)
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
            dialog.setNumberOfStreams(m_localSink->getNumberOfDeviceStreams());
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

        m_settingsKeys.append("title");
        m_settingsKeys.append("rgbColor");
        m_settingsKeys.append("useReverseAPI");
        m_settingsKeys.append("reverseAPIAddress");
        m_settingsKeys.append("reverseAPIPort");
        m_settingsKeys.append("reverseAPIFeatureSetIndex");
        m_settingsKeys.append("reverseAPIFeatureIndex");

        applySettings();
    }

    resetContextMenuType();
}

void LocalSinkGUI::updateDeviceSetList(const QList<int>& deviceSetIndexes)
{
    QList<int>::const_iterator it = deviceSetIndexes.begin();

    ui->localDevice->blockSignals(true);

    ui->localDevice->clear();

    for (; it != deviceSetIndexes.end(); ++it) {
        ui->localDevice->addItem(QString("R%1").arg(*it), *it);
    }

    ui->localDevice->blockSignals(false);
}

void LocalSinkGUI::on_decimationFactor_currentIndexChanged(int index)
{
    m_settings.m_log2Decim = index;
    applyDecimation();
}

void LocalSinkGUI::on_relativeSpectrum_toggled(bool checked)
{
    if (checked)
    {
        ui->glSpectrum->setCenterFrequency(0);
    }
    else
    {
        int shift = m_shiftFrequencyFactor * m_basebandSampleRate;
        ui->glSpectrum->setCenterFrequency(m_deviceCenterFrequency + shift);
    }
}

void LocalSinkGUI::on_position_valueChanged(int value)
{
    m_settings.m_filterChainHash = value;
    applyPosition();
    m_settingsKeys.append("filterChainHash");
    applySettings();
}

void LocalSinkGUI::on_localDevice_currentIndexChanged(int index)
{
    if (index >= 0)
    {
        m_settings.m_localDeviceIndex = ui->localDevice->currentData().toInt();
        m_settingsKeys.append("localDeviceIndex");
        applySettings();
    }
}

void LocalSinkGUI::on_localDevicePlay_toggled(bool checked)
{
    m_settings.m_play = checked;
    m_settingsKeys.append("play");
    applySettings();
}

void LocalSinkGUI::on_dsp_toggled(bool checked)
{
    m_settings.m_dsp = checked;
    m_settingsKeys.append("dsp");
    applySettings();
}

void LocalSinkGUI::on_gain_valueChanged(int value)
{
    m_settings.m_gaindB = value;
    ui->gainText->setText(tr("%1").arg(value));
    m_settingsKeys.append("gaindB");
    applySettings();
}

void LocalSinkGUI::on_fft_toggled(bool checked)
{
    m_settings.m_fftOn = checked;
    m_settingsKeys.append("fftOn");
    applySettings();
}

void LocalSinkGUI::on_fftSize_currentIndexChanged(int index)
{
    m_settings.m_log2FFT = index + 6;
    m_settingsKeys.append("log2FFT");
    applySettings();
}

void LocalSinkGUI::on_fftWindow_currentIndexChanged(int index)
{
    m_settings.m_fftWindow = (FFTWindow::Function) index;
    m_settingsKeys.append("fftWindow");
    applySettings();
}

void LocalSinkGUI::on_fftFilterReverse_toggled(bool checked)
{
    m_settings.m_reverseFilter = checked;
    m_settingsKeys.append("reverseFilter");
    applySettings();
}

void LocalSinkGUI::on_fftBandAdd_clicked()
{
    if (m_settings.m_fftBands.size() == m_settings.m_maxFFTBands) {
        return;
    }

    m_settings.m_fftBands.push_back(std::pair<float,float>{-0.1f, 0.2f});
    m_currentBandIndex = m_settings.m_fftBands.size()-1;
    displayFFTBand();
    m_settingsKeys.append("fftBands");
    applySettings();
}

void LocalSinkGUI::on_fftBandDel_clicked()
{
    if (m_settings.m_fftBands.size() == 0) {
        return;
    }

    m_settings.m_fftBands.erase(m_settings.m_fftBands.begin() + m_currentBandIndex);
    m_currentBandIndex--;
    displayFFTBand();
    m_settingsKeys.append("fftBands");
    applySettings();
}

void LocalSinkGUI::on_bandIndex_valueChanged(int value)
{
    ui->bandIndexText->setText(tr("%1").arg(value));
    m_currentBandIndex = value;
    displayFFTBand();
}

void LocalSinkGUI::on_f1_valueChanged(int value)
{
    float f1 = value / 1000.0f;
    m_settings.m_fftBands[m_currentBandIndex].first = f1;
    float maxWidth = 0.5f - f1;

    if (m_settings.m_fftBands[m_currentBandIndex].second > maxWidth) {
        m_settings.m_fftBands[m_currentBandIndex].second = maxWidth;
    }

    displayFFTBand();
    m_settingsKeys.append("fftBands");
    applySettings();
}

void LocalSinkGUI::on_bandWidth_valueChanged(int value)
{
    float w = value / 1000.0f;
    const float& f1 = m_settings.m_fftBands[m_currentBandIndex].first;
    float maxWidth = 0.5f - f1;

    if (w > maxWidth) {
        m_settings.m_fftBands[m_currentBandIndex].second = maxWidth;
    } else {
        m_settings.m_fftBands[m_currentBandIndex].second = w;
    }

    displayFFTBand();
    m_settingsKeys.append("fftBands");
    applySettings();
}

void LocalSinkGUI::on_filterF2orW_toggled(bool checked)
{
    m_showFilterHighCut = checked;
    displayFFTBand();
}

void LocalSinkGUI::applyDecimation()
{
    uint32_t maxHash = 1;

    for (uint32_t i = 0; i < m_settings.m_log2Decim; i++) {
        maxHash *= 3;
    }

    ui->position->setMaximum(maxHash-1);
    ui->position->setValue(m_settings.m_filterChainHash);
    m_settings.m_filterChainHash = ui->position->value();
    applyPosition();
    m_settingsKeys.append("filterChainHash");
    applySettings();
}

void LocalSinkGUI::applyPosition()
{
    ui->filterChainIndex->setText(tr("%1").arg(m_settings.m_filterChainHash));
    QString s;
    m_shiftFrequencyFactor = HBFilterChainConverter::convertToString(m_settings.m_log2Decim, m_settings.m_filterChainHash, s);
    ui->filterChainText->setText(s);

    updateAbsoluteCenterFrequency();
    displayRateAndShift();
    displayFFTBand();
}

void LocalSinkGUI::tick()
{
    if (++m_tickCount == 20) { // once per second
        m_tickCount = 0;
    }
}

void LocalSinkGUI::makeUIConnections()
{
    QObject::connect(ui->decimationFactor, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LocalSinkGUI::on_decimationFactor_currentIndexChanged);
    QObject::connect(ui->relativeSpectrum, &ButtonSwitch::toggled, this, &LocalSinkGUI::on_relativeSpectrum_toggled);
    QObject::connect(ui->position, &QSlider::valueChanged, this, &LocalSinkGUI::on_position_valueChanged);
    QObject::connect(ui->localDevice, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LocalSinkGUI::on_localDevice_currentIndexChanged);
    QObject::connect(ui->localDevicePlay, &ButtonSwitch::toggled, this, &LocalSinkGUI::on_localDevicePlay_toggled);
    QObject::connect(ui->dsp, &ButtonSwitch::toggled, this, &LocalSinkGUI::on_dsp_toggled);
    QObject::connect(ui->gain, &QDial::valueChanged, this, &LocalSinkGUI::on_gain_valueChanged);
    QObject::connect(ui->fft, &ButtonSwitch::toggled, this, &LocalSinkGUI::on_fft_toggled);
    QObject::connect(ui->fftSize, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LocalSinkGUI::on_fftSize_currentIndexChanged);
    QObject::connect(ui->fftWindow, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LocalSinkGUI::on_fftWindow_currentIndexChanged);
    QObject::connect(ui->fftFilterReverse, &ButtonSwitch::toggled, this, &LocalSinkGUI::on_fftFilterReverse_toggled);
    QObject::connect(ui->fftBandAdd, &QPushButton::clicked, this, &LocalSinkGUI::on_fftBandAdd_clicked);
    QObject::connect(ui->fftBandDel, &QPushButton::clicked, this, &LocalSinkGUI::on_fftBandDel_clicked);
    QObject::connect(ui->bandIndex, &QSlider::valueChanged, this, &LocalSinkGUI::on_bandIndex_valueChanged);
    QObject::connect(ui->f1, &QDial::valueChanged, this, &LocalSinkGUI::on_f1_valueChanged);
    QObject::connect(ui->bandWidth, &QDial::valueChanged, this, &LocalSinkGUI::on_bandWidth_valueChanged);
    QObject::connect(ui->filterF2orW, &ButtonSwitch::toggled, this, &LocalSinkGUI::on_filterF2orW_toggled);
}

void LocalSinkGUI::updateAbsoluteCenterFrequency()
{
    int shift = m_shiftFrequencyFactor * m_basebandSampleRate;
    setStatusFrequency(m_deviceCenterFrequency + shift);
}

QString LocalSinkGUI::displayScaled(int64_t value, int precision)
{
    int64_t posValue = (value < 0) ? -value : value;

    if (posValue < 1000) {
        return tr("%1").arg(QString::number(value, 'g', precision));
    } else if (posValue < 1000000) {
        return tr("%1k").arg(QString::number(value / 1000.0, 'g', precision));
    } else if (posValue < 1000000000) {
        return tr("%1M").arg(QString::number(value / 1000000.0, 'g', precision));
    } else if (posValue < 1000000000000) {
        return tr("%1%2").arg(QString::number(value / 1000000000.0, 'g', precision)).arg("G");
    } else {
        return tr("%1").arg(QString::number(value, 'e', precision));
    }
}
