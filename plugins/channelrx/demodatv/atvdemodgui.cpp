///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 F4HKW                                                      //
// for F4EXB / SDRAngel                                                          //
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

#include "atvdemodgui.h"

#include "device/deviceuiset.h"
#include "dsp/scopevis.h"
#include "dsp/glscopesettings.h"
#include "gui/basicchannelsettingsdialog.h"
#include "ui_atvdemodgui.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "dsp/dspengine.h"
#include "maincore.h"

#include "atvdemod.h"

ATVDemodGUI* ATVDemodGUI::create(PluginAPI* objPluginAPI,
        DeviceUISet *deviceUISet,
        BasebandSampleSink *rxChannel)
{
    ATVDemodGUI* gui = new ATVDemodGUI(objPluginAPI, deviceUISet, rxChannel);
    return gui;
}

void ATVDemodGUI::destroy()
{
    delete this;
}

void ATVDemodGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray ATVDemodGUI::serialize() const
{
    return m_settings.serialize();
}

bool ATVDemodGUI::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data))
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

void ATVDemodGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle(m_settings.m_title);
    setChannelMarkerBandwidth();
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());
    updateIndexLabel();

    m_doApplySettings = false;

    //********** ATV values **********
    ui->synchLevel->setValue((int) (m_settings.m_levelSynchroTop * 1000.0f));
    ui->synchLevelText->setText(QString("%1 mV").arg((int) (m_settings.m_levelSynchroTop * 1000.0f)));
    ui->blackLevel->setValue((int) (m_settings.m_levelBlack * 1000.0f));
    ui->blackLevelText->setText(QString("%1 mV").arg((int) (m_settings.m_levelBlack * 1000.0f)));
    ui->modulation->setCurrentIndex((int) m_settings.m_atvModulation);
    ui->fps->setCurrentIndex(ATVDemodSettings::getFpsIndex(m_settings.m_fps));
    ui->nbLines->setCurrentIndex(ATVDemodSettings::getNumberOfLinesIndex(m_settings.m_nbLines));
    ui->hSync->setChecked(m_settings.m_hSync);
    ui->vSync->setChecked(m_settings.m_vSync);
    ui->halfImage->setChecked(m_settings.m_halfFrames);
    ui->invertVideo->setChecked(m_settings.m_invertVideo);
    ui->standard->setCurrentIndex((int) m_settings.m_atvStd);
    lineTimeUpdate();
    topTimeUpdate();

    //********** RF values **********
    ui->deltaFrequency->setValue(m_settings.m_inputFrequencyOffset);
    ui->rfFiltering->setChecked(m_settings.m_fftFiltering);
    ui->bfo->setValue(m_settings.m_bfoFrequency);
    ui->bfoText->setText(QString("%1").arg(m_settings.m_bfoFrequency * 1.0, 0, 'f', 0));
    ui->fmDeviation->setValue((int) (m_settings.m_fmDeviation * 1000.0f));
    ui->fmDeviationText->setText(QString("%1").arg(m_settings.m_fmDeviation * 100.0, 0, 'f', 1));
    ui->amScaleFactor->setValue(m_settings.m_amScalingFactor);
    ui->amScaleFactorText->setText(QString("%1").arg(m_settings.m_amScalingFactor));
    ui->amScaleOffset->setValue(m_settings.m_amOffsetFactor);
    ui->amScaleOffsetText->setText(QString("%1").arg(m_settings.m_amOffsetFactor));

    applySampleRate();
    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();

    m_doApplySettings = true;
}

void ATVDemodGUI::displayRFBandwidths()
{
    int sliderPosition = m_settings.m_fftBandwidth / m_rfSliderDivisor;
    sliderPosition = sliderPosition < 1 ? 1 : sliderPosition > 100 ? 100 : sliderPosition;
    ui->rfBW->setValue(sliderPosition);
    ui->rfBWText->setText(QString("%1k").arg((sliderPosition * m_rfSliderDivisor) / 1000.0, 0, 'f', 0));
    sliderPosition = m_settings.m_fftOppBandwidth / m_rfSliderDivisor;
    sliderPosition = sliderPosition < 0 ? 0 : sliderPosition > 100 ? 100 : sliderPosition;
    ui->rfOppBW->setValue(sliderPosition);
    ui->rfOppBWText->setText(QString("%1k").arg((sliderPosition * m_rfSliderDivisor) / 1000.0, 0, 'f', 0));
}

void ATVDemodGUI::applySampleRate()
{
    qDebug("ATVDemodGUI::applySampleRate");
    unsigned int nbPointsPerLine;
    ATVDemodSettings::getBaseValues(m_basebandSampleRate, m_settings.m_fps*m_settings.m_nbLines, nbPointsPerLine);
    float samplesPerLineFrac = (float) m_basebandSampleRate / (m_settings.m_nbLines * m_settings.m_fps) - nbPointsPerLine;
    ui->tvSampleRateText->setText(tr("%1k").arg(m_basebandSampleRate/1000.0f, 0, 'f', 2));
    ui->nbPointsPerLineText->setText(tr("%1p+%2").arg(nbPointsPerLine).arg(samplesPerLineFrac, 0, 'f', 2));
    m_scopeVis->setLiveRate(m_basebandSampleRate);
    setRFFiltersSlidersRange(m_basebandSampleRate);
    displayRFBandwidths();
    lineTimeUpdate();
    topTimeUpdate();
    setChannelMarkerBandwidth();
}

bool ATVDemodGUI::handleMessage(const Message& message)
{
    if (DSPSignalNotification::match(message))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) message;
        m_basebandSampleRate = notif.getSampleRate();
        m_deviceCenterFrequency = notif.getCenterFrequency();
        ui->deltaFrequency->setValueRange(false, 8, -m_basebandSampleRate/2, m_basebandSampleRate/2);
        ui->deltaFrequencyLabel->setToolTip(tr("Range %1 %L2 Hz").arg(QChar(0xB1)).arg(m_basebandSampleRate/2));
        updateAbsoluteCenterFrequency();
        applySampleRate();

        return true;
    }
    else
    {
        return false;
    }
}

void ATVDemodGUI::channelMarkerChangedByCursor()
{
    qDebug("ATVDemodGUI::channelMarkerChangedByCursor");
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void ATVDemodGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void ATVDemodGUI::handleSourceMessages()
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

void ATVDemodGUI::onMenuDialogCalled(const QPoint &p)
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
            dialog.setNumberOfStreams(m_atvDemod->getNumberOfDeviceStreams());
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

void ATVDemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

ATVDemodGUI::ATVDemodGUI(PluginAPI* objPluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* objParent) :
        ChannelGUI(objParent),
        ui(new Ui::ATVDemodGUI),
        m_pluginAPI(objPluginAPI),
        m_deviceUISet(deviceUISet),
        m_channelMarker(this),
        m_deviceCenterFrequency(0),
        m_doApplySettings(false),
        m_intTickCount(0),
        m_basebandSampleRate(48000)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/demodatv/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_atvDemod = (ATVDemod*) rxChannel;
    m_atvDemod->setMessageQueueToGUI(getInputMessageQueue());
    m_scopeVis = m_atvDemod->getScopeSink();
    m_scopeVis->setGLScope(ui->glScope);
    m_atvDemod->setTVScreen(ui->screenTV);

    ui->glScope->connectTimer(MainCore::instance()->getMasterTimer());
    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick())); // 50 ms

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 8, -99999999, 99999999);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(Qt::white);
    m_channelMarker.setBandwidth(6000000);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setRollupState(&m_rollupState);

    setTitleColor(m_channelMarker.getColor());

    m_deviceUISet->addChannelMarker(&m_channelMarker);

    ui->scopeGUI->setBuddies(m_scopeVis->getInputMessageQueue(), m_scopeVis, ui->glScope);

    resetToDefaults(); // does applySettings()

    ui->scopeGUI->setPreTrigger(1);
    GLScopeSettings::TraceData traceData;
    traceData.m_amp = 2.0;      // amplification factor
    traceData.m_ofs = 0.5;      // direct offset
    ui->scopeGUI->changeTrace(0, traceData);
    ui->scopeGUI->focusOnTrace(0); // re-focus to take changes into account in the GUI
    GLScopeSettings::TriggerData triggerData;
    triggerData.m_triggerLevel = 0.1;
    triggerData.m_triggerLevelCoarse = 10;
    triggerData.m_triggerPositiveEdge = false;
    ui->scopeGUI->changeTrigger(0, triggerData);
    ui->scopeGUI->focusOnTrigger(0); // re-focus to take changes into account in the GUI

    connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()), this, SLOT(channelMarkerHighlightedByCursor()));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));

    QChar delta = QChar(0x94, 0x03);
    ui->fmDeviationLabel->setText(delta);

    makeUIConnections();
}

ATVDemodGUI::~ATVDemodGUI()
{
    delete ui;
}

void ATVDemodGUI::applySettings(bool force)
{
    qDebug() << "ATVDemodGUI::applySettings: " << force << " m_doApplySettings: " << m_doApplySettings;

    if (m_doApplySettings)
    {
		ATVDemod::MsgConfigureATVDemod *msg = ATVDemod::MsgConfigureATVDemod::create(m_settings, force);
		m_atvDemod->getInputMessageQueue()->push(msg);
    }
}

void ATVDemodGUI::setChannelMarkerBandwidth()
{
    // avoid infinite recursion
    m_channelMarker.blockSignals(true);
    ui->rfFiltering->blockSignals(true);
    ui->rfBW->blockSignals(true);
    ui->rfOppBW->blockSignals(true);
    ui->modulation->blockSignals(true);

    if (ui->rfFiltering->isChecked()) // FFT filter
    {
        m_channelMarker.setBandwidth(ui->rfBW->value()*m_rfSliderDivisor);
        m_channelMarker.setOppositeBandwidth(ui->rfOppBW->value()*m_rfSliderDivisor);

        if (ui->modulation->currentIndex() == (int) ATVDemodSettings::ATV_LSB) {
            m_channelMarker.setSidebands(ChannelMarker::vlsb);
        } else if (ui->modulation->currentIndex() == (int) ATVDemodSettings::ATV_USB) {
            m_channelMarker.setSidebands(ChannelMarker::vusb);
        } else {
            m_channelMarker.setSidebands(ChannelMarker::vusb);
        }
    }
    else
    {
        m_channelMarker.setBandwidth(m_basebandSampleRate);
        m_channelMarker.setSidebands(ChannelMarker::dsb);
    }

    m_channelMarker.blockSignals(false);
    m_channelMarker.emitChangedByAPI();

    ui->rfFiltering->blockSignals(false);
    ui->rfBW->blockSignals(false);
    ui->rfOppBW->blockSignals(false);
    ui->modulation->blockSignals(false);
}

void ATVDemodGUI::setRFFiltersSlidersRange(int sampleRate)
{
    // RF filters sliders range
    int scaleFactor = (int) std::log10(sampleRate/2);
    m_rfSliderDivisor = std::pow(10.0, scaleFactor-1);

    if (sampleRate/m_rfSliderDivisor < 50) {
        m_rfSliderDivisor /= 10;
    }

    if (ui->rfFiltering->isChecked())
    {
        ui->rfBW->setMaximum((sampleRate) / (2*m_rfSliderDivisor));
        ui->rfOppBW->setMaximum((sampleRate) / (2*m_rfSliderDivisor));
    }
    else
    {
        ui->rfBW->setMaximum((sampleRate) / m_rfSliderDivisor);
        ui->rfOppBW->setMaximum((sampleRate) / m_rfSliderDivisor);
    }

    ui->rfBWText->setText(QString("%1k").arg((ui->rfBW->value() * m_rfSliderDivisor) / 1000.0, 0, 'f', 0));
    ui->rfOppBWText->setText(QString("%1k").arg((ui->rfOppBW->value() * m_rfSliderDivisor) / 1000.0, 0, 'f', 0));
}

void ATVDemodGUI::leaveEvent(QEvent* event)
{
    m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void ATVDemodGUI::enterEvent(EnterEventType* event)
{
    m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void ATVDemodGUI::tick()
{
    if (m_intTickCount < 4) // ~200 ms
    {
        m_intTickCount++;
    }
    else
    {
        if (m_atvDemod)
        {
            m_objMagSqAverage(m_atvDemod->getMagSq());
            double magSqDB = CalcDb::dbPower(m_objMagSqAverage / (SDR_RX_SCALED*SDR_RX_SCALED));
            ui->channePowerText->setText(tr("%1 dB").arg(magSqDB, 0, 'f', 1));

            if (m_atvDemod->getBFOLocked()) {
                ui->bfoLockedLabel->setStyleSheet("QLabel { background-color : green; }");
            } else {
                ui->bfoLockedLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");
            }
        }

        m_intTickCount = 0;
    }

    return;
}

void ATVDemodGUI::on_synchLevel_valueChanged(int value)
{
    ui->synchLevelText->setText(QString("%1 mV").arg(value));
    m_settings.m_levelSynchroTop = value / 1000.0f;
    applySettings();
}

void ATVDemodGUI::on_blackLevel_valueChanged(int value)
{
    ui->blackLevelText->setText(QString("%1 mV").arg(value));
    m_settings.m_levelBlack = value / 1000.0f;
    applySettings();
}

void ATVDemodGUI::on_hSync_clicked()
{
    m_settings.m_hSync = ui->hSync->isChecked();
    applySettings();
}

void ATVDemodGUI::on_vSync_clicked()
{
    m_settings.m_vSync = ui->vSync->isChecked();
    applySettings();
}

void ATVDemodGUI::on_invertVideo_clicked()
{
    m_settings.m_invertVideo = ui->invertVideo->isChecked();
    applySettings();
}

void ATVDemodGUI::on_halfImage_clicked()
{
    m_settings.m_halfFrames = ui->halfImage->isChecked();
    applySettings();
}

void ATVDemodGUI::on_nbLines_currentIndexChanged(int index)
{
    m_settings.m_nbLines = ATVDemodSettings::getNumberOfLines(index);
    applySampleRate();
    applySettings();
}

void ATVDemodGUI::on_fps_currentIndexChanged(int index)
{
    m_settings.m_fps = ATVDemodSettings::getFps(index);
    applySampleRate();
    applySettings();
}

void ATVDemodGUI::on_standard_currentIndexChanged(int index)
{
    m_settings.m_atvStd = (ATVDemodSettings::ATVStd) index;
    applySettings();
}

void ATVDemodGUI::on_reset_clicked(bool checked)
{
    (void) checked;
    resetToDefaults();
}

void ATVDemodGUI::on_modulation_currentIndexChanged(int index)
{
    m_settings.m_atvModulation = (ATVDemodSettings::ATVModulation) index;
    setRFFiltersSlidersRange(m_basebandSampleRate);
    setChannelMarkerBandwidth();
    applySettings();
}

void ATVDemodGUI::on_rfBW_valueChanged(int value)
{
    m_settings.m_fftBandwidth = value * m_rfSliderDivisor;
    ui->rfBWText->setText(QString("%1k").arg((value * m_rfSliderDivisor) / 1000.0, 0, 'f', 0));
    setChannelMarkerBandwidth();
    applySettings();
}

void ATVDemodGUI::on_rfOppBW_valueChanged(int value)
{
    m_settings.m_fftOppBandwidth = value * m_rfSliderDivisor;
    ui->rfOppBWText->setText(QString("%1k").arg((value * m_rfSliderDivisor) / 1000.0, 0, 'f', 0));
    setChannelMarkerBandwidth();
    applySettings();
}

void ATVDemodGUI::on_rfFiltering_toggled(bool checked)
{
    m_settings.m_fftFiltering = checked;
    setRFFiltersSlidersRange(m_basebandSampleRate);
    setChannelMarkerBandwidth();
    applySettings();
}

void ATVDemodGUI::on_deltaFrequency_changed(qint64 value)
{
    m_settings.m_inputFrequencyOffset = value;
    m_channelMarker.setCenterFrequency(value);
    updateAbsoluteCenterFrequency();
    applySettings();
}

void ATVDemodGUI::on_bfo_valueChanged(int value)
{
    m_settings.m_bfoFrequency = value;
    ui->bfoText->setText(QString("%1").arg(value * 1.0, 0, 'f', 0));
    applySettings();
}

void ATVDemodGUI::on_fmDeviation_valueChanged(int value)
{
    m_settings.m_fmDeviation = value / 1000.0f;
    ui->fmDeviationText->setText(QString("%1").arg(value / 10.0, 0, 'f', 1));
    applySettings();
}

void ATVDemodGUI::on_amScaleFactor_valueChanged(int value)
{
    m_settings.m_amScalingFactor = value;
    ui->amScaleFactor->setValue(m_settings.m_amScalingFactor);
    ui->amScaleFactorText->setText(QString("%1").arg(m_settings.m_amScalingFactor));
    applySettings();
}

void ATVDemodGUI::on_amScaleOffset_valueChanged(int value)
{
    m_settings.m_amOffsetFactor = value;
    ui->amScaleOffset->setValue(m_settings.m_amOffsetFactor);
    ui->amScaleOffsetText->setText(QString("%1").arg(m_settings.m_amOffsetFactor));
    applySettings();
}

void ATVDemodGUI::on_screenTabWidget_currentChanged(int index)
{
    m_atvDemod->setVideoTabIndex(index);
}

void ATVDemodGUI::lineTimeUpdate()
{
    float nominalLineTime = ATVDemodSettings::getNominalLineTime(m_settings.m_nbLines, m_settings.m_fps);

    if (nominalLineTime < 0.0)
        ui->lineTimeText->setText("invalid");
    else if(nominalLineTime < 0.000001)
        ui->lineTimeText->setText(tr("%1 ns").arg(nominalLineTime * 1000000000.0, 0, 'f', 2));
    else if(nominalLineTime < 0.001)
        ui->lineTimeText->setText(tr("%1 µs").arg(nominalLineTime * 1000000.0, 0, 'f', 2));
    else if(nominalLineTime < 1.0)
        ui->lineTimeText->setText(tr("%1 ms").arg(nominalLineTime * 1000.0, 0, 'f', 2));
    else
        ui->lineTimeText->setText(tr("%1 s").arg(nominalLineTime * 1.0, 0, 'f', 2));
}

void ATVDemodGUI::topTimeUpdate()
{
    float nominalTopTime = ATVDemodSettings::getNominalLineTime(m_settings.m_nbLines, m_settings.m_fps) * (4.7f / 64.0f);

    if (nominalTopTime < 0.0)
        ui->topTimeText->setText("invalid");
    else if (nominalTopTime < 0.000001)
        ui->topTimeText->setText(tr("%1 ns").arg(nominalTopTime * 1000000000.0, 0, 'f', 2));
    else if(nominalTopTime < 0.001)
        ui->topTimeText->setText(tr("%1 µs").arg(nominalTopTime * 1000000.0, 0, 'f', 2));
    else if(nominalTopTime < 1.0)
        ui->topTimeText->setText(tr("%1 ms").arg(nominalTopTime * 1000.0, 0, 'f', 2));
    else
        ui->topTimeText->setText(tr("%1 s").arg(nominalTopTime * 1.0, 0, 'f', 2));
}

void ATVDemodGUI::makeUIConnections()
{
    QObject::connect(ui->synchLevel, &QSlider::valueChanged, this, &ATVDemodGUI::on_synchLevel_valueChanged);
    QObject::connect(ui->blackLevel, &QSlider::valueChanged, this, &ATVDemodGUI::on_blackLevel_valueChanged);
    QObject::connect(ui->hSync, &QCheckBox::clicked, this, &ATVDemodGUI::on_hSync_clicked);
    QObject::connect(ui->vSync, &QCheckBox::clicked, this, &ATVDemodGUI::on_vSync_clicked);
    QObject::connect(ui->invertVideo, &QCheckBox::clicked, this, &ATVDemodGUI::on_invertVideo_clicked);
    QObject::connect(ui->halfImage, &QCheckBox::clicked, this, &ATVDemodGUI::on_halfImage_clicked);
    QObject::connect(ui->modulation, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ATVDemodGUI::on_modulation_currentIndexChanged);
    QObject::connect(ui->nbLines, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ATVDemodGUI::on_nbLines_currentIndexChanged);
    QObject::connect(ui->fps, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ATVDemodGUI::on_fps_currentIndexChanged);
    QObject::connect(ui->standard, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ATVDemodGUI::on_standard_currentIndexChanged);
    QObject::connect(ui->reset, &QPushButton::clicked, this, &ATVDemodGUI::on_reset_clicked);
    QObject::connect(ui->rfBW, &QSlider::valueChanged, this, &ATVDemodGUI::on_rfBW_valueChanged);
    QObject::connect(ui->rfOppBW, &QSlider::valueChanged, this, &ATVDemodGUI::on_rfOppBW_valueChanged);
    QObject::connect(ui->rfFiltering, &ButtonSwitch::toggled, this, &ATVDemodGUI::on_rfFiltering_toggled);
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &ATVDemodGUI::on_deltaFrequency_changed);
    QObject::connect(ui->bfo, &QDial::valueChanged, this, &ATVDemodGUI::on_bfo_valueChanged);
    QObject::connect(ui->fmDeviation, &QDial::valueChanged, this, &ATVDemodGUI::on_fmDeviation_valueChanged);
    QObject::connect(ui->amScaleFactor, &QDial::valueChanged, this, &ATVDemodGUI::on_amScaleFactor_valueChanged);
    QObject::connect(ui->amScaleOffset, &QDial::valueChanged, this, &ATVDemodGUI::on_amScaleOffset_valueChanged);
    QObject::connect(ui->screenTabWidget, &QTabWidget::currentChanged, this, &ATVDemodGUI::on_screenTabWidget_currentChanged);
}

void ATVDemodGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}
