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
#include "ui_atvdemodgui.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "dsp/dspengine.h"
#include "mainwindow.h"

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

void ATVDemodGUI::setName(const QString& strName)
{
    setObjectName(strName);
}

QString ATVDemodGUI::getName() const
{
    return objectName();
}

qint64 ATVDemodGUI::getCenterFrequency() const
{
    return m_channelMarker.getCenterFrequency();
}

void ATVDemodGUI::setCenterFrequency(qint64 intCenterFrequency)
{
    m_channelMarker.setCenterFrequency(intCenterFrequency);
    m_settings.m_inputFrequencyOffset = intCenterFrequency;
    applySettings();
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
    displayStreamIndex();

    blockApplySettings(true);

    //********** ATV values **********
    ui->synchLevel->setValue((int) (m_settings.m_levelSynchroTop * 1000.0f));
    ui->synchLevelText->setText(QString("%1 mV").arg((int) (m_settings.m_levelSynchroTop * 1000.0f)));
    ui->blackLevel->setValue((int) (m_settings.m_levelBlack * 1000.0f));
    ui->blackLevelText->setText(QString("%1 mV").arg((int) (m_settings.m_levelBlack * 1000.0f)));
    ui->lineTime->setValue(m_settings.m_lineTimeFactor);
    ui->topTime->setValue(m_settings.m_topTimeFactor);
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
    ui->decimatorEnable->setChecked(m_settings.m_forceDecimator);
    ui->rfFiltering->setChecked(m_settings.m_fftFiltering);
    ui->bfo->setValue(m_settings.m_bfoFrequency);
    ui->bfoText->setText(QString("%1").arg(m_settings.m_bfoFrequency * 1.0, 0, 'f', 0));
    ui->fmDeviation->setValue((int) (m_settings.m_fmDeviation * 1000.0f));
    ui->fmDeviationText->setText(QString("%1").arg(m_settings.m_fmDeviation * 100.0, 0, 'f', 1));
    blockApplySettings(false);

    applyTVSampleRate();
}

void ATVDemodGUI::displayStreamIndex()
{
    if (m_deviceUISet->m_deviceMIMOEngine) {
        setStreamIndicator(tr("%1").arg(m_settings.m_streamIndex));
    } else {
        setStreamIndicator("S"); // single channel indicator
    }
}

void ATVDemodGUI::displayRFBandwidths()
{
    int sliderPosition = m_settings.m_fftBandwidth / m_rfSliderDivisor;
    sliderPosition < 1 ? 1 : sliderPosition > 100 ? 100 : sliderPosition;
    ui->rfBW->setValue(sliderPosition);
    ui->rfBWText->setText(QString("%1k").arg((sliderPosition * m_rfSliderDivisor) / 1000.0, 0, 'f', 0));
    sliderPosition = m_settings.m_fftOppBandwidth / m_rfSliderDivisor;
    sliderPosition < 0 ? 0 : sliderPosition > 100 ? 100 : sliderPosition;
    ui->rfOppBW->setValue(sliderPosition);
    ui->rfOppBWText->setText(QString("%1k").arg((sliderPosition * m_rfSliderDivisor) / 1000.0, 0, 'f', 0));
}

void ATVDemodGUI::applyTVSampleRate()
{
    blockApplySettings(true);
    unsigned int nbPointsPerLine;
    ATVDemodSettings::getBaseValues(m_basebandSampleRate, m_settings.m_fps*m_settings.m_nbLines, m_tvSampleRate, nbPointsPerLine);
    ui->tvSampleRateText->setText(tr("%1k").arg(m_tvSampleRate/1000.0f, 0, 'f', 2));
    ui->nbPointsPerLineText->setText(tr("%1p").arg(nbPointsPerLine));
    m_scopeVis->setLiveRate(m_tvSampleRate);
    setRFFiltersSlidersRange(m_tvSampleRate);
    displayRFBandwidths();
    lineTimeUpdate();
    topTimeUpdate();
    blockApplySettings(false);
}

bool ATVDemodGUI::handleMessage(const Message& message)
{
    if (DSPSignalNotification::match(message))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) message;
        m_basebandSampleRate = notif.getSampleRate();
        applyTVSampleRate();

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

void ATVDemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;
}

ATVDemodGUI::ATVDemodGUI(PluginAPI* objPluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* objParent) :
        RollupWidget(objParent),
        ui(new Ui::ATVDemodGUI),
        m_pluginAPI(objPluginAPI),
        m_deviceUISet(deviceUISet),
        m_channelMarker(this),
        m_blnDoApplySettings(true),
        m_intTickCount(0),
        m_basebandSampleRate(48000),
        m_tvSampleRate(48000)
{
    ui->setupUi(this);
    ui->screenTV->setColor(false);
    setAttribute(Qt::WA_DeleteOnClose, true);
    connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));

    m_scopeVis = new ScopeVis(ui->glScope);
    m_atvDemod = (ATVDemod*) rxChannel; //new ATVDemod(m_deviceUISet->m_deviceSourceAPI);
    m_atvDemod->setMessageQueueToGUI(getInputMessageQueue());
    m_atvDemod->setScopeSink(m_scopeVis);
    m_atvDemod->setTVScreen(ui->screenTV);

    ui->glScope->connectTimer(MainWindow::getInstance()->getMasterTimer());
    connect(&MainWindow::getInstance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick())); // 50 ms

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(Qt::white);
    m_channelMarker.setBandwidth(6000000);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    setTitleColor(m_channelMarker.getColor());

    m_deviceUISet->registerRxChannelInstance(ATVDemod::m_channelIdURI, this);
    m_deviceUISet->addChannelMarker(&m_channelMarker);
    m_deviceUISet->addRollupWidget(this);

    ui->scopeGUI->setBuddies(m_scopeVis->getInputMessageQueue(), m_scopeVis, ui->glScope);

    resetToDefaults(); // does applySettings()

    ui->scopeGUI->setPreTrigger(1);
    ScopeVis::TraceData traceData;
    traceData.m_amp = 2.0;      // amplification factor
    traceData.m_ampIndex = 1;   // this is second step
    traceData.m_ofs = 0.5;      // direct offset
    traceData.m_ofsCoarse = 50; // this is 50 coarse steps
    ui->scopeGUI->changeTrace(0, traceData);
    ui->scopeGUI->focusOnTrace(0); // re-focus to take changes into account in the GUI
    ScopeVis::TriggerData triggerData;
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
}

ATVDemodGUI::~ATVDemodGUI()
{
    m_deviceUISet->removeRxChannelInstance(this);
    delete m_atvDemod; // TODO: check this: when the GUI closes it has to delete the demodulator
    delete m_scopeVis;
    delete ui;
}

void ATVDemodGUI::blockApplySettings(bool blnBlock)
{
    m_blnDoApplySettings = !blnBlock;
}

void ATVDemodGUI::applySettings(bool force)
{
    if (m_blnDoApplySettings)
    {
		ATVDemod::MsgConfigureATVDemod *msg = ATVDemod::MsgConfigureATVDemod::create(m_settings, force);
		m_atvDemod->getInputMessageQueue()->push(msg);
    }
}

void ATVDemodGUI::setChannelMarkerBandwidth()
{
    m_blnDoApplySettings = false; // avoid infinite recursion
    m_channelMarker.blockSignals(true);

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
        if ((m_basebandSampleRate == m_tvSampleRate) && (!m_settings.m_forceDecimator)) {
            m_channelMarker.setBandwidth(m_basebandSampleRate);
        } else {
            m_channelMarker.setBandwidth(ui->rfBW->value()*m_rfSliderDivisor);
        }

        m_channelMarker.setSidebands(ChannelMarker::dsb);
    }

    m_channelMarker.blockSignals(false);
    m_channelMarker.emitChangedByAPI();
    m_blnDoApplySettings = true;
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

void ATVDemodGUI::leaveEvent(QEvent*)
{
    m_channelMarker.setHighlighted(false);
}

void ATVDemodGUI::enterEvent(QEvent*)
{
    m_channelMarker.setHighlighted(true);
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

void ATVDemodGUI::on_lineTime_valueChanged(int value)
{
	ui->lineTime->setToolTip(QString("Line length adjustment (%1)").arg(value));
    m_settings.m_lineTimeFactor = value;
    lineTimeUpdate();
    applySettings();
}

void ATVDemodGUI::on_topTime_valueChanged(int value)
{
	ui->topTime->setToolTip(QString("Horizontal sync pulse length adjustment (%1)").arg(value));
    m_settings.m_topTimeFactor = value;
    topTimeUpdate();
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
    applyTVSampleRate();
    applySettings();
}

void ATVDemodGUI::on_fps_currentIndexChanged(int index)
{
    m_settings.m_fps = ATVDemodSettings::getFps(index);
    applyTVSampleRate();
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
    setRFFiltersSlidersRange(m_tvSampleRate);
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
    setRFFiltersSlidersRange(m_tvSampleRate);
    setChannelMarkerBandwidth();
    applySettings();
}

void ATVDemodGUI::on_decimatorEnable_toggled(bool checked)
{
    m_settings.m_forceDecimator = checked;
    setChannelMarkerBandwidth();
    applySettings();
}

void ATVDemodGUI::on_deltaFrequency_changed(qint64 value)
{
    m_settings.m_inputFrequencyOffset = value;
    m_channelMarker.setCenterFrequency(value);
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

void ATVDemodGUI::on_screenTabWidget_currentChanged(int index)
{
    m_atvDemod->setVideoTabIndex(index);
}

void ATVDemodGUI::lineTimeUpdate()
{
    float nominalLineTime = ATVDemodSettings::getNominalLineTime(m_settings.m_nbLines, m_settings.m_fps);
    int lineTimeScaleFactor = (int) std::log10(nominalLineTime);

    if (m_tvSampleRate == 0) {
        m_fltLineTimeMultiplier = std::pow(10.0, lineTimeScaleFactor-3);
    } else {
        m_fltLineTimeMultiplier = 1.0f / m_tvSampleRate;
    }

    float lineTime = nominalLineTime + m_fltLineTimeMultiplier * ui->lineTime->value();

    if (lineTime < 0.0)
        ui->lineTimeText->setText("invalid");
    else if(lineTime < 0.000001)
        ui->lineTimeText->setText(tr("%1 ns").arg(lineTime * 1000000000.0, 0, 'f', 2));
    else if(lineTime < 0.001)
        ui->lineTimeText->setText(tr("%1 µs").arg(lineTime * 1000000.0, 0, 'f', 2));
    else if(lineTime < 1.0)
        ui->lineTimeText->setText(tr("%1 ms").arg(lineTime * 1000.0, 0, 'f', 2));
    else
        ui->lineTimeText->setText(tr("%1 s").arg(lineTime * 1.0, 0, 'f', 2));
}

void ATVDemodGUI::topTimeUpdate()
{
    float nominalTopTime = ATVDemodSettings::getNominalLineTime(m_settings.m_nbLines, m_settings.m_fps) * (4.7f / 64.0f);
    int topTimeScaleFactor = (int) std::log10(nominalTopTime);

    if (m_tvSampleRate == 0) {
        m_fltTopTimeMultiplier = std::pow(10.0, topTimeScaleFactor-3);
    } else {
        m_fltTopTimeMultiplier = 1.0f / m_tvSampleRate;
    }

    float topTime = nominalTopTime + m_fltTopTimeMultiplier * ui->topTime->value();

    if (topTime < 0.0)
        ui->topTimeText->setText("invalid");
    else if (topTime < 0.000001)
        ui->topTimeText->setText(tr("%1 ns").arg(topTime * 1000000000.0, 0, 'f', 2));
    else if(topTime < 0.001)
        ui->topTimeText->setText(tr("%1 µs").arg(topTime * 1000000.0, 0, 'f', 2));
    else if(topTime < 1.0)
        ui->topTimeText->setText(tr("%1 ms").arg(topTime * 1000.0, 0, 'f', 2));
    else
        ui->topTimeText->setText(tr("%1 s").arg(topTime * 1.0, 0, 'f', 2));
}
