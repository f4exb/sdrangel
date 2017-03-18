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
    ui->lineTime->setValue(640);
    ui->topTime->setValue(3);
    ui->modulation->setCurrentIndex(0);
    ui->fps->setCurrentIndex(0);
    ui->hSync->setChecked(true);
    ui->vSync->setChecked(true);
    ui->halfImage->setChecked(false);

    blockApplySettings(false);
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

        if (d.readU32(2, &u32tmp)) {
            m_objChannelMarker.setColor(u32tmp);
        } else {
            m_objChannelMarker.setColor(Qt::white);
        }

        d.readS32(3, &tmp, 100);
        ui->synchLevel->setValue(tmp);
        d.readS32(4, &tmp, 310);
        ui->blackLevel->setValue(tmp);
        d.readS32(5, &tmp, 640);
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

        blockApplySettings(false);
        m_objChannelMarker.blockSignals(false);

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
        ui->channelSampleRateText->setText(tr("%1k").arg(sampleRate/1000.0f, 0, 'f', 0));
        setRFFiltersSlidersRange(sampleRate);

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

    m_objATVDemod = new ATVDemod();
    m_objATVDemod->setATVScreen(ui->screenTV);

    m_objChannelizer = new DownChannelizer(m_objATVDemod);
    m_objThreadedChannelizer = new ThreadedBasebandSampleSink(m_objChannelizer, this);
    m_objDeviceAPI->addThreadedSink(m_objThreadedChannelizer);

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

    connect(&m_objChannelMarker, SIGNAL(changed()), this, SLOT(viewChanged()));

    m_objDeviceAPI->registerChannelInstance(m_strChannelID, this);
    m_objDeviceAPI->addChannelMarker(&m_objChannelMarker);
    m_objDeviceAPI->addRollupWidget(this);

    //ui->screenTV->connectTimer(m_objPluginAPI->getMainWindow()->getMasterTimer());

    m_objMagSqAverage.resize(4, 1.0);

    resetToDefaults(); // does applySettings()

    connect(m_objATVDemod->getOutputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));
}

ATVDemodGUI::~ATVDemodGUI()
{
    m_objDeviceAPI->removeChannelInstance(this);
    m_objDeviceAPI->removeThreadedSink(m_objThreadedChannelizer);
    delete m_objThreadedChannelizer;
    delete m_objChannelizer;
    delete m_objATVDemod;
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
        setTitleColor(m_objChannelMarker.getColor());
		ui->deltaFrequency->setValue(abs(m_objChannelMarker.getCenterFrequency()));
        ui->deltaFrequencyMinus->setChecked(m_objChannelMarker.getCenterFrequency() < 0);

        m_objChannelizer->configure(m_objChannelizer->getInputMessageQueue(),
                m_objChannelizer->getInputSampleRate(), // always use maximum available bandwidth
                m_objChannelMarker.getCenterFrequency());

        m_objATVDemod->configure(m_objATVDemod->getInputMessageQueue(),
                ui->lineTime->value() / 10.0f,
                ui->topTime->value() * 1.0f,
                (ui->fps->currentIndex() == 0) ? 25.0f : 30.0f,
                (ui->halfImage->checkState() == Qt::Checked) ? 0.5f : 1.0f,
                ui->synchLevel->value() / 1000.0f,
                ui->blackLevel->value() / 1000.0f,
                ui->hSync->isChecked(),
                ui->vSync->isChecked());

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
                ui->rfBW->value() * 100000.0f,
                ui->rfOppBW->value() * 100000.0f,
                ui->rfFiltering->isChecked(),
                ui->decimatorEnable->isChecked());
    }
}

void ATVDemodGUI::setChannelMarkerBandwidth()
{
    m_blnDoApplySettings = false; // avoid infinite recursion

    if (ui->rfFiltering->isChecked()) // FFT filter
    {
        m_objChannelMarker.setBandwidth(ui->rfBW->value()*100000);
        m_objChannelMarker.setOppositeBandwidth(ui->rfOppBW->value()*100000);

        if (ui->modulation->currentIndex() == (int) ATVDemod::ATV_VAML) {
            m_objChannelMarker.setSidebands(ChannelMarker::vlsb);
        } else if (ui->modulation->currentIndex() == (int) ATVDemod::ATV_VAMU) {
            m_objChannelMarker.setSidebands(ChannelMarker::vusb);
        } else {
            m_objChannelMarker.setSidebands(ChannelMarker::vusb);
        }
    }
    else
    {
        if (ui->decimatorEnable->isChecked()) {
            m_objChannelMarker.setBandwidth(ui->rfBW->value()*100000);
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
    if (ui->rfFiltering->isChecked())
    {
        ui->rfBW->setMaximum(sampleRate / 200000);
        ui->rfOppBW->setMaximum(sampleRate / 200000);
    }
    else
    {
        ui->rfBW->setMaximum(sampleRate / 100000);
        ui->rfOppBW->setMaximum(sampleRate / 100000);
    }
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
    ui->lineTimeText->setText(QString("%1 uS").arg(((float) value) / 10.0f));
    applySettings();

}

void ATVDemodGUI::on_topTime_valueChanged(int value)
{
    ui->topTimeText->setText(QString("%1 uS").arg(value));
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

void ATVDemodGUI::on_halfImage_clicked()
{
    applySettings();
}

void ATVDemodGUI::on_fps_currentIndexChanged(int index)
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
    ui->rfBWText->setText(QString("%1 MHz").arg(value / 10.0, 0, 'f', 1));
    setChannelMarkerBandwidth();
    applyRFSettings();
}

void ATVDemodGUI::on_rfOppBW_valueChanged(int value)
{
    ui->rfOppBWText->setText(QString("%1").arg(value / 10.0, 0, 'f', 1));
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

