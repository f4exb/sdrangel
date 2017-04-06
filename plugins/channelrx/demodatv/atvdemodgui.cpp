///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 F4HKW                                                      //
// for F4EXB / SDRAngel                                                          //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
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

#include "device/devicesourceapi.h"
#include "dsp/downchannelizer.h"

#include "dsp/threadedbasebandsamplesink.h"
#include "dsp/scopevisng.h"
#include "ui_atvdemodgui.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "gui/basicchannelsettingswidget.h"
#include "dsp/dspengine.h"
#include "mainwindow.h"

#include "atvdemod.h"

const QString ATVDemodGUI::m_strChannelID = "sdrangel.channel.demodatv";

ATVDemodGUI* ATVDemodGUI::create(PluginAPI* objPluginAPI,
        DeviceSourceAPI *objDeviceAPI)
{
    ATVDemodGUI* gui = new ATVDemodGUI(objPluginAPI, objDeviceAPI);
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
    return m_objChannelMarker.getCenterFrequency();
}

void ATVDemodGUI::setCenterFrequency(qint64 intCenterFrequency)
{
    m_objChannelMarker.setCenterFrequency(intCenterFrequency);
    applySettings();
}

void ATVDemodGUI::resetToDefaults()
{
    blockApplySettings(true);

    //********** ATV Default values **********
    ui->synchLevel->setValue(100);
    ui->blackLevel->setValue(310);
    ui->lineTime->setValue(0);
    ui->topTime->setValue(3);
    ui->modulation->setCurrentIndex(0);
    ui->fps->setCurrentIndex(0);
    ui->hSync->setChecked(true);
    ui->vSync->setChecked(true);
    ui->halfImage->setChecked(false);
    ui->invertVideo->setChecked(false);

    //********** RF Default values **********
    ui->decimatorEnable->setChecked(false);
    ui->rfFiltering->setChecked(false);
    ui->rfBW->setValue(10);
    ui->rfOppBW->setValue(10);
    ui->bfo->setValue(0);
    ui->fmDeviation->setValue(100);

    blockApplySettings(false);
    lineTimeUpdate();
    topTimeUpdate();
    applySettings();
}

QByteArray ATVDemodGUI::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_objChannelMarker.getCenterFrequency());
    s.writeU32(2, m_objChannelMarker.getColor().rgb());
    s.writeS32(3, ui->synchLevel->value());
    s.writeS32(4, ui->blackLevel->value());
    s.writeS32(5, ui->lineTime->value());
    s.writeS32(6, ui->topTime->value());
    s.writeS32(7, ui->modulation->currentIndex());
    s.writeS32(8, ui->fps->currentIndex());
    s.writeBool(9, ui->hSync->isChecked());
    s.writeBool(10, ui->vSync->isChecked());
    s.writeBool(11, ui->halfImage->isChecked());
    s.writeS32(12, ui->rfBW->value());
    s.writeS32(13, ui->rfOppBW->value());
    s.writeS32(14, ui->bfo->value());
    s.writeBool(15, ui->invertVideo->isChecked());
    s.writeS32(16, ui->nbLines->currentIndex());
    s.writeS32(17, ui->fmDeviation->value());
    s.writeS32(18, ui->standard->currentIndex());

    return s.final();
}

bool ATVDemodGUI::deserialize(const QByteArray& arrData)
{
    SimpleDeserializer d(arrData);

    if (!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if (d.getVersion() == 1)
    {
        QByteArray bytetmp;
        uint32_t u32tmp;
        int tmp;
        bool booltmp;

        blockApplySettings(true);
        m_objChannelMarker.blockSignals(true);

        d.readS32(1, &tmp, 0);
        m_objChannelMarker.setCenterFrequency(tmp);

//        if (d.readU32(2, &u32tmp)) {
//            m_objChannelMarker.setColor(u32tmp);
//        } else {
//            m_objChannelMarker.setColor(Qt::white);
//        }

        d.readS32(3, &tmp, 100);
        ui->synchLevel->setValue(tmp);
        d.readS32(4, &tmp, 310);
        ui->blackLevel->setValue(tmp);
        d.readS32(5, &tmp, 0);
        ui->lineTime->setValue(tmp);
        d.readS32(6, &tmp, 3);
        ui->topTime->setValue(tmp);
        d.readS32(7, &tmp, 0);
        ui->modulation->setCurrentIndex(tmp);
        d.readS32(8, &tmp, 0);
        ui->fps->setCurrentIndex(tmp);
        d.readBool(9, &booltmp, true);
        ui->hSync->setChecked(booltmp);
        d.readBool(10, &booltmp, true);
        ui->vSync->setChecked(booltmp);
        d.readBool(11, &booltmp, false);
        ui->halfImage->setChecked(booltmp);
        d.readS32(12, &tmp, 10);
        ui->rfBW->setValue(tmp);
        d.readS32(13, &tmp, 10);
        ui->rfOppBW->setValue(tmp);
        d.readS32(14, &tmp, 10);
        ui->bfo->setValue(tmp);
        d.readBool(15, &booltmp, true);
        ui->invertVideo->setChecked(booltmp);
        d.readS32(16, &tmp, 0);
        ui->nbLines->setCurrentIndex(tmp);
        d.readS32(17, &tmp, 100);
        ui->fmDeviation->setValue(tmp);
        d.readS32(18, &tmp, 0);
        ui->standard->setCurrentIndex(tmp);

        blockApplySettings(false);
        m_objChannelMarker.blockSignals(false);

        lineTimeUpdate();
        topTimeUpdate();
        applySettings();
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

bool ATVDemodGUI::handleMessage(const Message& objMessage)
{
    if (ATVDemod::MsgReportEffectiveSampleRate::match(objMessage))
    {
        int sampleRate = ((ATVDemod::MsgReportEffectiveSampleRate&)objMessage).getSampleRate();
        int nbPointsPerLine =  ((ATVDemod::MsgReportEffectiveSampleRate&)objMessage).getNbPointsPerLine();
        ui->channelSampleRateText->setText(tr("%1k").arg(sampleRate/1000.0f, 0, 'f', 0));
        ui->nbPointsPerLineText->setText(tr("%1p").arg(nbPointsPerLine));
        m_objScopeVis->setSampleRate(sampleRate);
        setRFFiltersSlidersRange(sampleRate);
        lineTimeUpdate();
        topTimeUpdate();

        return true;
    }
    else
    {
        return false;
    }
}

void ATVDemodGUI::viewChanged()
{
    qDebug("ATVDemodGUI::viewChanged");
    applySettings();
    applyRFSettings();
}

void ATVDemodGUI::channelSampleRateChanged()
{
    qDebug("ATVDemodGUI::channelSampleRateChanged");
    applySettings();
    applyRFSettings();
}

void ATVDemodGUI::handleSourceMessages()
{
    Message* message;

    while ((message = m_objATVDemod->getOutputMessageQueue()->pop()) != 0)
    {
        if (handleMessage(*message))
        {
            delete message;
        }
    }
}

void ATVDemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
}

void ATVDemodGUI::onMenuDoubleClicked()
{
    if (!m_blnBasicSettingsShown)
    {
        m_blnBasicSettingsShown = true;
        BasicChannelSettingsWidget* bcsw = new BasicChannelSettingsWidget(
                &m_objChannelMarker, this);
        bcsw->show();
    }
}

ATVDemodGUI::ATVDemodGUI(PluginAPI* objPluginAPI, DeviceSourceAPI *objDeviceAPI,
        QWidget* objParent) :
        RollupWidget(objParent),
        ui(new Ui::ATVDemodGUI),
        m_objPluginAPI(objPluginAPI),
        m_objDeviceAPI(objDeviceAPI),
        m_objChannelMarker(this),
        m_blnBasicSettingsShown(false),
        m_blnDoApplySettings(true),
        m_intTickCount(0)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose, true);
    connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(menuDoubleClickEvent()), this, SLOT(onMenuDoubleClicked()));

    m_objScopeVis = new ScopeVisNG(ui->glScope);
    m_objATVDemod = new ATVDemod(m_objScopeVis);
    m_objATVDemod->setATVScreen(ui->screenTV);

    m_objChannelizer = new DownChannelizer(m_objATVDemod);
    m_objThreadedChannelizer = new ThreadedBasebandSampleSink(m_objChannelizer, this);
    m_objDeviceAPI->addThreadedSink(m_objThreadedChannelizer);

    ui->glScope->connectTimer(m_objPluginAPI->getMainWindow()->getMasterTimer());
    connect(&m_objPluginAPI->getMainWindow()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick())); // 50 ms

    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::ReverseGold));
    ui->deltaFrequency->setValueRange(7, 0U, 9999999U);

    connect(m_objChannelizer, SIGNAL(inputSampleRateChanged()), this, SLOT(channelSampleRateChanged()));

    //m_objPluginAPI->addThreadedSink(m_objThreadedChannelizer);
    m_objChannelMarker.setColor(Qt::white);
    m_objChannelMarker.setMovable(false);
    m_objChannelMarker.setBandwidth(6000000);
    m_objChannelMarker.setCenterFrequency(0);
    m_objChannelMarker.setVisible(true);
    setTitleColor(m_objChannelMarker.getColor());

    connect(&m_objChannelMarker, SIGNAL(changed()), this, SLOT(viewChanged()));

    m_objDeviceAPI->registerChannelInstance(m_strChannelID, this);
    m_objDeviceAPI->addChannelMarker(&m_objChannelMarker);
    m_objDeviceAPI->addRollupWidget(this);

    //ui->screenTV->connectTimer(m_objPluginAPI->getMainWindow()->getMasterTimer());

    m_objMagSqAverage.resize(4, 1.0);

    ui->scopeGUI->setBuddies(m_objScopeVis->getInputMessageQueue(), m_objScopeVis, ui->glScope);

    resetToDefaults(); // does applySettings()

    ui->scopeGUI->setPreTrigger(1);
    ScopeVisNG::TraceData traceData;
    traceData.m_amp = 2.0;      // amplification factor
    traceData.m_ampIndex = 1;   // this is second step
    traceData.m_ofs = 0.5;      // direct offset
    traceData.m_ofsCoarse = 50; // this is 50 coarse steps
    ui->scopeGUI->changeTrace(0, traceData);
    ui->scopeGUI->focusOnTrace(0); // re-focus to take changes into account in the GUI
    ScopeVisNG::TriggerData triggerData;
    triggerData.m_triggerLevel = 0.1;
    triggerData.m_triggerLevelCoarse = 10;
    triggerData.m_triggerPositiveEdge = false;
    ui->scopeGUI->changeTrigger(0, triggerData);
    ui->scopeGUI->focusOnTrigger(0); // re-focus to take changes into account in the GUI

    connect(m_objATVDemod->getOutputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));

    QChar delta = QChar(0x94, 0x03);
    ui->fmDeviationLabel->setText(delta);
}

ATVDemodGUI::~ATVDemodGUI()
{
    m_objDeviceAPI->removeChannelInstance(this);
    m_objDeviceAPI->removeThreadedSink(m_objThreadedChannelizer);
    delete m_objThreadedChannelizer;
    delete m_objChannelizer;
    delete m_objATVDemod;
    delete m_objScopeVis;
    delete ui;
}

void ATVDemodGUI::blockApplySettings(bool blnBlock)
{
    m_blnDoApplySettings = !blnBlock;
}

void ATVDemodGUI::applySettings()
{
    if (m_blnDoApplySettings)
    {
		ui->deltaFrequency->setValue(abs(m_objChannelMarker.getCenterFrequency()));
        ui->deltaFrequencyMinus->setChecked(m_objChannelMarker.getCenterFrequency() < 0);

        m_objChannelizer->configure(m_objChannelizer->getInputMessageQueue(),
                m_objChannelizer->getInputSampleRate(), // always use maximum available bandwidth
                m_objChannelMarker.getCenterFrequency());

        m_objATVDemod->configure(m_objATVDemod->getInputMessageQueue(),
                getNominalLineTime(ui->nbLines->currentIndex(), ui->fps->currentIndex()) + ui->lineTime->value() * m_fltLineTimeMultiplier,
                getNominalLineTime(ui->nbLines->currentIndex(), ui->fps->currentIndex()) * (4.7f / 64.0f) + ui->topTime->value() * m_fltTopTimeMultiplier,
                getFps(ui->fps->currentIndex()),
                (ATVDemod::ATVStd) ui->standard->currentIndex(),
                getNumberOfLines(ui->nbLines->currentIndex()),
                (ui->halfImage->checkState() == Qt::Checked) ? 0.5f : 1.0f,
                ui->synchLevel->value() / 1000.0f,
                ui->blackLevel->value() / 1000.0f,
                ui->hSync->isChecked(),
                ui->vSync->isChecked(),
                ui->invertVideo->isChecked(),
				ui->screenTabWidget->currentIndex());

        qDebug() << "ATVDemodGUI::applySettings:"
                << " m_objChannelizer.inputSampleRate: " << m_objChannelizer->getInputSampleRate()
                << " m_objATVDemod.sampleRate: " << m_objATVDemod->getSampleRate();
    }
}

void ATVDemodGUI::applyRFSettings()
{
    if (m_blnDoApplySettings)
    {
        m_objATVDemod->configureRF(m_objATVDemod->getInputMessageQueue(),
                (ATVDemod::ATVModulation) ui->modulation->currentIndex(),
                ui->rfBW->value() * m_rfSliderDivisor * 1.0f,
                ui->rfOppBW->value() * m_rfSliderDivisor * 1.0f,
                ui->rfFiltering->isChecked(),
                ui->decimatorEnable->isChecked(),
                ui->bfo->value() * 10.0f,
                ui->fmDeviation->value() / 100.0f);
    }
}

void ATVDemodGUI::setChannelMarkerBandwidth()
{
    m_blnDoApplySettings = false; // avoid infinite recursion

    if (ui->rfFiltering->isChecked()) // FFT filter
    {
        m_objChannelMarker.setBandwidth(ui->rfBW->value()*m_rfSliderDivisor);
        m_objChannelMarker.setOppositeBandwidth(ui->rfOppBW->value()*m_rfSliderDivisor);

        if (ui->modulation->currentIndex() == (int) ATVDemod::ATV_LSB) {
            m_objChannelMarker.setSidebands(ChannelMarker::vlsb);
        } else if (ui->modulation->currentIndex() == (int) ATVDemod::ATV_USB) {
            m_objChannelMarker.setSidebands(ChannelMarker::vusb);
        } else {
            m_objChannelMarker.setSidebands(ChannelMarker::vusb);
        }
    }
    else
    {
        if (ui->decimatorEnable->isChecked()) {
            m_objChannelMarker.setBandwidth(ui->rfBW->value()*m_rfSliderDivisor);
        } else {
            m_objChannelMarker.setBandwidth(m_objChannelizer->getInputSampleRate());
        }

        m_objChannelMarker.setSidebands(ChannelMarker::dsb);
    }

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
    blockApplySettings(true);
    m_objChannelMarker.setHighlighted(false);
    blockApplySettings(false);
}

void ATVDemodGUI::enterEvent(QEvent*)
{
    blockApplySettings(true);
    m_objChannelMarker.setHighlighted(true);
    blockApplySettings(false);
}

void ATVDemodGUI::tick()
{
    if (m_intTickCount < 4) // ~200 ms
    {
        m_intTickCount++;
    }
    else
    {
        if (m_objATVDemod)
        {
            m_objMagSqAverage.feed(m_objATVDemod->getMagSq());
            double magSqDB = CalcDb::dbPower(m_objMagSqAverage.average() / (1<<30));
            ui->channePowerText->setText(tr("%1 dB").arg(magSqDB, 0, 'f', 1));

            if (m_objATVDemod->getBFOLocked()) {
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
    applySettings();
    ui->synchLevelText->setText(QString("%1 mV").arg(value));
}

void ATVDemodGUI::on_blackLevel_valueChanged(int value)
{
    applySettings();
    ui->blackLevelText->setText(QString("%1 mV").arg(value));
}

void ATVDemodGUI::on_lineTime_valueChanged(int value)
{
	ui->lineTime->setToolTip(QString("Line length adjustment (%1)").arg(value));
    lineTimeUpdate();
    applySettings();
}

void ATVDemodGUI::on_topTime_valueChanged(int value)
{
	ui->topTime->setToolTip(QString("Horizontal sync pulse length adjustment (%1)").arg(value));
    topTimeUpdate();
    applySettings();
}

void ATVDemodGUI::on_hSync_clicked()
{
    applySettings();
}

void ATVDemodGUI::on_vSync_clicked()
{
    applySettings();
}

void ATVDemodGUI::on_invertVideo_clicked()
{
    applySettings();
}

void ATVDemodGUI::on_halfImage_clicked()
{
    applySettings();
}

void ATVDemodGUI::on_nbLines_currentIndexChanged(int index)
{
    lineTimeUpdate();
    topTimeUpdate();
    applySettings();
}

void ATVDemodGUI::on_fps_currentIndexChanged(int index)
{
    lineTimeUpdate();
    topTimeUpdate();
    applySettings();
}

void ATVDemodGUI::on_standard_currentIndexChanged(int index)
{
    applySettings();
}

void ATVDemodGUI::on_reset_clicked(bool checked)
{
    resetToDefaults();
}

void ATVDemodGUI::on_modulation_currentIndexChanged(int index)
{
    setRFFiltersSlidersRange(m_objATVDemod->getEffectiveSampleRate());
    setChannelMarkerBandwidth();
    applyRFSettings();
}

void ATVDemodGUI::on_rfBW_valueChanged(int value)
{
    ui->rfBWText->setText(QString("%1k").arg((value * m_rfSliderDivisor) / 1000.0, 0, 'f', 0));
    setChannelMarkerBandwidth();
    applyRFSettings();
}

void ATVDemodGUI::on_rfOppBW_valueChanged(int value)
{
    ui->rfOppBWText->setText(QString("%1k").arg((value * m_rfSliderDivisor) / 1000.0, 0, 'f', 0));
    setChannelMarkerBandwidth();
    applyRFSettings();
}

void ATVDemodGUI::on_rfFiltering_toggled(bool checked)
{
    setRFFiltersSlidersRange(m_objATVDemod->getEffectiveSampleRate());
    setChannelMarkerBandwidth();
    applyRFSettings();
}

void ATVDemodGUI::on_decimatorEnable_toggled(bool checked)
{
    setChannelMarkerBandwidth();
    applyRFSettings();
}

void ATVDemodGUI::on_deltaFrequency_changed(quint64 value)
{
    if (ui->deltaFrequencyMinus->isChecked()) {
        m_objChannelMarker.setCenterFrequency(-value);
    } else {
        m_objChannelMarker.setCenterFrequency(value);
    }
}

void ATVDemodGUI::on_deltaFrequencyMinus_toggled(bool minus)
{
    int deltaFrequency = m_objChannelMarker.getCenterFrequency();
    bool minusDelta = (deltaFrequency < 0);

    if (minus ^ minusDelta) // sign change
    {
        m_objChannelMarker.setCenterFrequency(-deltaFrequency);
    }
}

void ATVDemodGUI::on_bfo_valueChanged(int value)
{
    ui->bfoText->setText(QString("%1").arg(value * 10.0, 0, 'f', 0));
    applyRFSettings();
}

void ATVDemodGUI::on_fmDeviation_valueChanged(int value)
{
    ui->fmDeviationText->setText(QString("%1").arg(value));
    applyRFSettings();
}

void ATVDemodGUI::on_screenTabWidget_currentChanged(int index)
{
	applySettings();
}

void ATVDemodGUI::lineTimeUpdate()
{
    float nominalLineTime = getNominalLineTime(ui->nbLines->currentIndex(), ui->fps->currentIndex());
    int lineTimeScaleFactor = (int) std::log10(nominalLineTime);

    if (m_objATVDemod->getEffectiveSampleRate() == 0) {
        m_fltLineTimeMultiplier = std::pow(10.0, lineTimeScaleFactor-3);
    } else {
        m_fltLineTimeMultiplier = 1.0f / m_objATVDemod->getEffectiveSampleRate();
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
    float nominalTopTime = getNominalLineTime(ui->nbLines->currentIndex(), ui->fps->currentIndex()) * (4.7f / 64.0f);
    int topTimeScaleFactor = (int) std::log10(nominalTopTime);

    if (m_objATVDemod->getEffectiveSampleRate() == 0) {
        m_fltTopTimeMultiplier = std::pow(10.0, topTimeScaleFactor-3);
    } else {
        m_fltTopTimeMultiplier = 1.0f / m_objATVDemod->getEffectiveSampleRate();
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

float ATVDemodGUI::getFps(int fpsIndex)
{
    switch(fpsIndex)
    {
    case 0:
        return 30.0f;
        break;
    case 2:
        return 20.0f;
        break;
    case 3:
        return 16.0f;
        break;
    case 4:
        return 12.0f;
        break;
    case 5:
        return 10.0f;
        break;
    case 6:
        return 8.0f;
        break;
    case 1:
    default:
        return 25.0f;
        break;
    }
}

float ATVDemodGUI::getNominalLineTime(int nbLinesIndex, int fpsIndex)
{
    float fps = getFps(fpsIndex);
    int   nbLines = getNumberOfLines(nbLinesIndex);

    return 1.0f / (nbLines * fps);
}

int ATVDemodGUI::getNumberOfLines(int nbLinesIndex)
{
    switch(nbLinesIndex)
    {
    case 1:
        return 525;
        break;
    case 2:
        return 405;
        break;
    case 3:
        return 343;
        break;
    case 4:
        return 240;
        break;
    case 5:
        return 180;
        break;
    case 6:
        return 120;
        break;
    case 7:
        return 90;
        break;
    case 8:
        return 60;
        break;
    case 9:
        return 32;
        break;
    case 0:
    default:
        return 625;
        break;
    }
}
