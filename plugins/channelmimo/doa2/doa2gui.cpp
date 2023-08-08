///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB                                   //
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
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "dsp/hbfilterchainconverter.h"
#include "dsp/scopevis.h"
#include "dsp/spectrumvis.h"
#include "maincore.h"

#include "doa2gui.h"
#include "doa2.h"
#include "ui_doa2gui.h"

DOA2GUI* DOA2GUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, MIMOChannel *channelMIMO)
{
    DOA2GUI* gui = new DOA2GUI(pluginAPI, deviceUISet, channelMIMO);
    return gui;
}

void DOA2GUI::destroy()
{
    delete this;
}

void DOA2GUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray DOA2GUI::serialize() const
{
    return m_settings.serialize();
}

bool DOA2GUI::deserialize(const QByteArray& data)
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

MessageQueue* DOA2GUI::getInputMessageQueue()
{
    return &m_inputMessageQueue;
}

bool DOA2GUI::handleMessage(const Message& message)
{
    if (DOA2::MsgBasebandNotification::match(message))
    {
        DOA2::MsgBasebandNotification& notif = (DOA2::MsgBasebandNotification&) message;
        m_sampleRate = notif.getSampleRate();
        m_centerFrequency = notif.getCenterFrequency();
        displayRateAndShift();
        updateAbsoluteCenterFrequency();
        setFFTAveragingTooltip();
        return true;
    }
    else if (DOA2::MsgConfigureDOA2::match(message))
    {
        const DOA2::MsgConfigureDOA2& notif = (const DOA2::MsgConfigureDOA2&) message;
        m_settings = notif.getSettings();
        ui->scopeGUI->updateSettings();
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        return true;
    }
    else
    {
        return false;
    }
}

DOA2GUI::DOA2GUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, MIMOChannel *channelMIMO, QWidget* parent) :
        ChannelGUI(parent),
        ui(new Ui::DOA2GUI),
        m_pluginAPI(pluginAPI),
        m_deviceUISet(deviceUISet),
        m_sampleRate(48000),
        m_centerFrequency(435000000),
        m_tickCount(0)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelmimo/doa2/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_doa2 = (DOA2*) channelMIMO;
    m_scopeVis = m_doa2->getScopeVis();
    m_scopeVis->setGLScope(ui->glScope);
    m_doa2->setMessageQueueToGUI(getInputMessageQueue());
    m_sampleRate = m_doa2->getDeviceSampleRate();

    ui->glScope->setTraceModulo(DOA2::m_fftSize);

	ui->glScope->connectTimer(MainCore::instance()->getMasterTimer());
	connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    m_channelMarker.blockSignals(true);
    m_channelMarker.addStreamIndex(1);
    m_channelMarker.setColor(m_settings.m_rgbColor);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("DOA 2 source");
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setRollupState(&m_rollupState);
    m_settings.setScopeGUI(ui->scopeGUI);

    m_deviceUISet->addChannelMarker(&m_channelMarker);

	ui->scopeGUI->setBuddies(m_scopeVis->getInputMessageQueue(), m_scopeVis, ui->glScope);

    m_scopeVis->setTraceChunkSize(DOA2::m_fftSize); // Set scope trace length unit to FFT size
    ui->scopeGUI->traceLengthChange();
    ui->compass->setBlindAngleBorder(true);

    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));

    displaySettings();
    makeUIConnections();
    displayRateAndShift();
    applySettings(true);

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    ui->halfWLLabel->setText(QString("%1/2").arg(QChar(0xBB, 0x03)));
    ui->azUnits->setText(QString("%1").arg(QChar(0260)));
    DialPopup::addPopupsToChildDials(this);
}

DOA2GUI::~DOA2GUI()
{
    delete ui;
}

void DOA2GUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void DOA2GUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        setTitleColor(m_channelMarker.getColor());

        DOA2::MsgConfigureDOA2* message = DOA2::MsgConfigureDOA2::create(m_settings, force);
        m_doa2->getInputMessageQueue()->push(message);
    }
}

void DOA2GUI::displaySettings()
{
    ui->correlationType->setCurrentIndex((int) m_settings.m_correlationType);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.setBandwidth(m_sampleRate);
    m_channelMarker.setMovable(false); // do not let user move the center arbitrarily
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());

    blockApplySettings(true);
    ui->decimationFactor->setCurrentIndex(m_settings.m_log2Decim);
    applyDecimation();
    ui->phaseCorrection->setValue(m_settings.m_phase);
    ui->phaseCorrectionText->setText(tr("%1").arg(m_settings.m_phase));
    ui->compass->setAzAnt(m_settings.m_antennaAz);
    ui->antAz->setValue(m_settings.m_antennaAz);
    ui->baselineDistance->setValue(m_settings.m_basebandDistance);
    ui->squelch->setValue(m_settings.m_squelchdB);
    ui->squelchText->setText(tr("%1").arg(m_settings.m_squelchdB, 3));
    ui->fftAveraging->setCurrentIndex(m_settings.m_fftAveragingIndex);
    setFFTAveragingTooltip();
    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void DOA2GUI::displayRateAndShift()
{
    int shift = m_shiftFrequencyFactor * m_sampleRate;
    double channelSampleRate = ((double) m_sampleRate) / (1<<m_settings.m_log2Decim);
    QLocale loc;
    ui->offsetFrequencyText->setText(tr("%1 Hz").arg(loc.toString(shift)));
    ui->channelRateText->setText(tr("%1k").arg(QString::number(channelSampleRate / 1000.0, 'g', 5)));
    m_channelMarker.setCenterFrequency(shift);
    m_channelMarker.setBandwidth(channelSampleRate);
    m_scopeVis->setLiveRate(channelSampleRate);
}

void DOA2GUI::setFFTAveragingTooltip()
{
    float channelSampleRate = ((float) m_sampleRate) / (1<<m_settings.m_log2Decim);
    float averagingTime = (DOA2::m_fftSize * DOA2Settings::getAveragingValue(m_settings.m_fftAveragingIndex)) /
        channelSampleRate;
    QString s;
    setNumberStr(averagingTime, 2, s);
    ui->fftAveraging->setToolTip(QString("Number of averaging FFTs (avg time: %1s)").arg(s));
}

void DOA2GUI::setNumberStr(float v, int decimalPlaces, QString& s)
{
    if (v < 1e-6) {
        s = tr("%1n").arg(v*1e9, 0, 'f', decimalPlaces);
    } else if (v < 1e-3) {
        s = tr("%1µ").arg(v*1e6, 0, 'f', decimalPlaces);
    } else if (v < 1.0) {
        s = tr("%1m").arg(v*1e3, 0, 'f', decimalPlaces);
    } else if (v < 1e3) {
        s = tr("%1").arg(v, 0, 'f', decimalPlaces);
    } else if (v < 1e6) {
        s = tr("%1k").arg(v*1e-3, 0, 'f', decimalPlaces);
    } else if (v < 1e9) {
        s = tr("%1M").arg(v*1e-6, 0, 'f', decimalPlaces);
    } else {
        s = tr("%1G").arg(v*1e-9, 0, 'f', decimalPlaces);
    }
}

void DOA2GUI::leaveEvent(QEvent*)
{
    m_channelMarker.setHighlighted(false);
}

void DOA2GUI::enterEvent(EnterEventType*)
{
    m_channelMarker.setHighlighted(true);
}

void DOA2GUI::handleSourceMessages()
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

void DOA2GUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

void DOA2GUI::onMenuDialogCalled(const QPoint &p)
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

        applySettings();
    }

    resetContextMenuType();
}

void DOA2GUI::on_decimationFactor_currentIndexChanged(int index)
{
    m_settings.m_log2Decim = index;
    updateScopeFScale();
    applyDecimation();
}

void DOA2GUI::on_position_valueChanged(int value)
{
    m_settings.m_filterChainHash = value;
    applyPosition();
}

void DOA2GUI::on_phaseCorrection_valueChanged(int value)
{
    m_settings.m_phase = value;
    ui->phaseCorrectionText->setText(tr("%1").arg(value));
    applySettings();
}

void DOA2GUI::on_correlationType_currentIndexChanged(int index)
{
    m_settings.m_correlationType = (DOA2Settings::CorrelationType) index;
    updateScopeFScale();
    applySettings();
}

void DOA2GUI::on_antAz_valueChanged(int value)
{
    m_settings.m_antennaAz = value;
    ui->compass->setAzAnt(value);
    updateDOA();
    applySettings();
}

void DOA2GUI::on_baselineDistance_valueChanged(int value)
{
    m_settings.m_basebandDistance = value < 1 ? 1 : value;
    updateDOA();
    applySettings();
}

void DOA2GUI::on_squelch_valueChanged(int value)
{
    m_settings.m_squelchdB = value;
    ui->squelchText->setText(tr("%1").arg(m_settings.m_squelchdB, 3));
    applySettings();
}

void DOA2GUI::on_fftAveraging_currentIndexChanged(int index)
{
    qDebug("DOA2GUI::on_averaging_currentIndexChanged: %d", index);
    m_settings.m_fftAveragingIndex = index;
    applySettings();
    setFFTAveragingTooltip();
}

void DOA2GUI::on_centerPosition_clicked()
{
    uint32_t filterChainHash = 1;
    uint32_t mul = 1;

    for (uint32_t i = 1; i < m_settings.m_log2Decim; i++)
    {
        mul *= 3;
        filterChainHash += mul;
    }

    m_settings.m_filterChainHash = filterChainHash;
    ui->position->setValue(m_settings.m_filterChainHash);
    applyPosition();
}

void DOA2GUI::applyDecimation()
{
    uint32_t maxHash = 1;

    for (uint32_t i = 0; i < m_settings.m_log2Decim; i++) {
        maxHash *= 3;
    }

    ui->position->setMaximum(maxHash-1);
    ui->position->setValue(m_settings.m_filterChainHash);
    m_settings.m_filterChainHash = ui->position->value();
    applyPosition();
}

void DOA2GUI::applyPosition()
{
    ui->filterChainIndex->setText(tr("%1").arg(m_settings.m_filterChainHash));
    QString s;
    m_shiftFrequencyFactor = HBFilterChainConverter::convertToString(m_settings.m_log2Decim, m_settings.m_filterChainHash, s);
    ui->filterChainText->setText(s);

    displayRateAndShift();
    updateAbsoluteCenterFrequency();
    applySettings();
}

void DOA2GUI::tick()
{
    if (++m_tickCount == 20) // once per second
    {
        updateDOA();
        m_tickCount = 0;
    }
}

void DOA2GUI::makeUIConnections()
{
    QObject::connect(ui->decimationFactor, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DOA2GUI::on_decimationFactor_currentIndexChanged);
    QObject::connect(ui->position, &QSlider::valueChanged, this, &DOA2GUI::on_position_valueChanged);
    QObject::connect(ui->phaseCorrection, &QSlider::valueChanged, this, &DOA2GUI::on_phaseCorrection_valueChanged);
    QObject::connect(ui->correlationType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DOA2GUI::on_correlationType_currentIndexChanged);
    QObject::connect(ui->antAz, QOverload<int>::of(&QSpinBox::valueChanged), this, &DOA2GUI::on_antAz_valueChanged);
    QObject::connect(ui->baselineDistance, QOverload<int>::of(&QSpinBox::valueChanged), this, &DOA2GUI::on_baselineDistance_valueChanged);
    QObject::connect(ui->squelch, &QDial::valueChanged, this, &DOA2GUI::on_squelch_valueChanged);
    QObject::connect(ui->fftAveraging, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DOA2GUI::on_fftAveraging_currentIndexChanged);
    QObject::connect(ui->centerPosition, &QPushButton::clicked, this, &DOA2GUI::on_centerPosition_clicked);
}

void DOA2GUI::updateAbsoluteCenterFrequency()
{
    qint64 cf = m_centerFrequency + m_shiftFrequencyFactor * m_sampleRate;
    setStatusFrequency(cf);
    m_hwl = 1.5e+8 / cf;
    ui->halfWLText->setText(tr("%1").arg(m_hwl*1000, 5, 'f', 0));
    updateScopeFScale();
}

void DOA2GUI::updateScopeFScale()
{
    if (m_settings.m_correlationType == DOA2Settings::CorrelationType::CorrelationFFT) {
        ui->glScope->setXScaleFreq(true);
    } else {
        ui->glScope->setXScaleFreq(false);
    }

    ui->glScope->setXScaleCenterFrequency(m_centerFrequency);
    ui->glScope->setXScaleFrequencySpan(m_sampleRate / (1<<m_settings.m_log2Decim));
}

void DOA2GUI::updateDOA()
{
    float cosTheta = (m_doa2->getPhi()/M_PI) * ((m_hwl * 1000.0) / m_settings.m_basebandDistance);
    float blindAngle = (m_settings.m_basebandDistance > m_hwl * 1000.0) ?
        std::acos((m_hwl * 1000.0) / m_settings.m_basebandDistance) * (180/M_PI) :
        0;
    ui->compass->setBlindAngle(blindAngle);
    float doaAngle = std::acos(cosTheta < -1.0 ? -1.0 : cosTheta > 1.0 ? 1.0 : cosTheta) * (180/M_PI);
    float posAngle = ui->antAz->value() - doaAngle; // DOA angles are trigonometric but displayed angles are clockwise
    float negAngle = ui->antAz->value() + doaAngle;
    ui->compass->setAzPos(posAngle);
    ui->compass->setAzNeg(negAngle);
    ui->posText->setText(tr("%1").arg(ui->compass->getAzPos(), 3, 'f', 0, QLatin1Char('0')));
    ui->negText->setText(tr("%1").arg(ui->compass->getAzNeg(), 3, 'f', 0, QLatin1Char('0')));
}
