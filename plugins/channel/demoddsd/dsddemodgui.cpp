///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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
#include <QDebug>
#include "ui_dsddemodgui.h"
#include "dsp/threadedsamplesink.h"
#include "dsp/channelizer.h"
#include "dsp/scopevis.h"
#include "gui/glscope.h"
#include "plugin/pluginapi.h"
#include "device/deviceapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "gui/basicchannelsettingswidget.h"
#include "dsp/dspengine.h"
#include "mainwindow.h"

#include "dsddemod.h"
#include "dsddemodgui.h"

const QString DSDDemodGUI::m_channelID = "sdrangel.channel.dsddemod";

unsigned int DSDDemodBaudRates::m_rates[] = {2400, 4800};
unsigned int DSDDemodBaudRates::m_nb_rates = 2;
unsigned int DSDDemodBaudRates::m_defaultRateIndex = 1; // 4800 bauds

char DSDDemodGUI::m_dpmrFrameTypes[][3] = {
		"--", // 0: no frame sync
		"XS", // 1: no frame - extensive search of FS2
		"HD", // 2: header frame
		"PY", // 3: payload super frame not categorized yet
		"VO", // 4: voice super frame
		"VD", // 5: voice and data superframe
		"D1", // 6: data type 1 super frame
		"D2", // 7: data type 2 super frame
		"EN", // 8: end frame
};

const char * DSDDemodGUI::m_ysfChannelTypeText[4] = {
        "H", //!< Header Channel
        "C", //!< Communications Channel
        "T", //!< Termination Channel
        "S"  //!< Test
};

const char * DSDDemodGUI::m_ysfDataTypeText[4] = {
        "V1", //!< Voice/Data type 1
        "DF", //!< Data Full Rate
        "V2", //!< Voice/Data type 2
        "VF"  //!< Voice Full Rate
};

const char * DSDDemodGUI::m_ysfCallModeText[4] = {
        "GC", //!< Group CQ
        "RI", //!< Radio ID
        "RS", //!< Reserved
        "IN"  //!< Individual
};

DSDDemodGUI* DSDDemodGUI::create(PluginAPI* pluginAPI, DeviceAPI *deviceAPI)
{
    DSDDemodGUI* gui = new DSDDemodGUI(pluginAPI, deviceAPI);
	return gui;
}

void DSDDemodGUI::destroy()
{
	delete this;
}

void DSDDemodGUI::setName(const QString& name)
{
	setObjectName(name);
}

QString DSDDemodGUI::getName() const
{
	return objectName();
}

qint64 DSDDemodGUI::getCenterFrequency() const
{
	return m_channelMarker.getCenterFrequency();
}

void DSDDemodGUI::setCenterFrequency(qint64 centerFrequency)
{
	m_channelMarker.setCenterFrequency(centerFrequency);
	applySettings();
}

void DSDDemodGUI::resetToDefaults()
{
	blockApplySettings(true);

	ui->rfBW->setValue(100); // x100 Hz
	ui->demodGain->setValue(100); // 100ths
	ui->fmDeviation->setValue(50); // x100 Hz
	ui->volume->setValue(20); // /10.0
	ui->baudRate->setCurrentIndex(DSDDemodBaudRates::getDefaultRateIndex());
	ui->squelchGate->setValue(5);
	ui->squelch->setValue(-40);
	ui->deltaFrequency->setValue(0);

	blockApplySettings(false);
	applySettings();
}

QByteArray DSDDemodGUI::serialize() const
{
	SimpleSerializer s(1);
	s.writeS32(1, m_channelMarker.getCenterFrequency());
	s.writeS32(2, ui->rfBW->value());
	s.writeS32(3, ui->demodGain->value());
	s.writeS32(4, ui->fmDeviation->value());
	s.writeS32(5, ui->squelch->value());
	s.writeU32(7, m_channelMarker.getColor().rgb());
	s.writeS32(8, ui->squelchGate->value());
	s.writeS32(9, ui->volume->value());
    s.writeBlob(10, ui->scopeGUI->serialize());
    s.writeS32(11, ui->baudRate->currentIndex());
    s.writeBool(12, m_enableCosineFiltering);
    s.writeBool(13, m_syncOrConstellation);
    s.writeBool(14, m_slot1On);
    s.writeBool(15, m_slot2On);
    s.writeBool(16, m_tdmaStereo);
	return s.final();
}

bool DSDDemodGUI::deserialize(const QByteArray& data)
{
	SimpleDeserializer d(data);

	if (!d.isValid())
	{
		resetToDefaults();
		return false;
	}

	if (d.getVersion() == 1)
	{
		QByteArray bytetmp;
		quint32 u32tmp;
		qint32 tmp;
		bool boolTmp;

		blockApplySettings(true);
		m_channelMarker.blockSignals(true);

		d.readS32(1, &tmp, 0);
		m_channelMarker.setCenterFrequency(tmp);
		d.readS32(2, &tmp, 4);
		ui->rfBW->setValue(tmp);
		d.readS32(3, &tmp, 3);
		ui->demodGain->setValue(tmp);
		d.readS32(4, &tmp, 20);
		ui->fmDeviation->setValue(tmp);
		d.readS32(5, &tmp, -40);
		ui->squelch->setValue(tmp);

		if(d.readU32(7, &u32tmp))
		{
			m_channelMarker.setColor(u32tmp);
		}

		d.readS32(8, &tmp, 5);
		ui->squelchGate->setValue(tmp);
        d.readS32(9, &tmp, 20);
        ui->volume->setValue(tmp);
        d.readBlob(10, &bytetmp);
        ui->scopeGUI->deserialize(bytetmp);
        d.readS32(11, &tmp, 20);
        ui->baudRate->setCurrentIndex(tmp);
        d.readBool(12, &m_enableCosineFiltering, false);
        d.readBool(13, &m_syncOrConstellation, false);
        d.readBool(14, &m_slot1On, false);
        d.readBool(15, &m_slot2On, false);
        d.readBool(16, &m_tdmaStereo, false);

		blockApplySettings(false);
		m_channelMarker.blockSignals(false);

		applySettings();
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
	return false;
}

void DSDDemodGUI::viewChanged()
{
	applySettings();
}

void DSDDemodGUI::on_deltaMinus_toggled(bool minus)
{
	int deltaFrequency = m_channelMarker.getCenterFrequency();
	bool minusDelta = (deltaFrequency < 0);

	if (minus ^ minusDelta) // sign change
	{
		m_channelMarker.setCenterFrequency(-deltaFrequency);
	}
}

void DSDDemodGUI::on_deltaFrequency_changed(quint64 value)
{
	if (ui->deltaMinus->isChecked())
	{
		m_channelMarker.setCenterFrequency(-value);
	}
	else
	{
		m_channelMarker.setCenterFrequency(value);
	}
}

void DSDDemodGUI::on_rfBW_valueChanged(int value)
{
	qDebug() << "DSDDemodGUI::on_rfBW_valueChanged" << value * 100;
	m_channelMarker.setBandwidth(value * 100);
	applySettings();
}

void DSDDemodGUI::on_demodGain_valueChanged(int value)
{
	applySettings();
}

void DSDDemodGUI::on_fmDeviation_valueChanged(int value)
{
	applySettings();
}

void DSDDemodGUI::on_volume_valueChanged(int value)
{
    applySettings();
}

void DSDDemodGUI::on_baudRate_currentIndexChanged(int index)
{
    applySettings();
}

void DSDDemodGUI::on_enableCosineFiltering_toggled(bool enable)
{
	m_enableCosineFiltering = enable;
	applySettings();
}

void DSDDemodGUI::on_syncOrConstellation_toggled(bool checked)
{
    m_syncOrConstellation = checked;
    applySettings();
}

void DSDDemodGUI::on_slot1On_toggled(bool checked)
{
    m_slot1On = checked;
    applySettings();
}

void DSDDemodGUI::on_slot2On_toggled(bool checked)
{
    m_slot2On = checked;
    applySettings();
}

void DSDDemodGUI::on_tdmaStereoSplit_toggled(bool checked)
{
    m_tdmaStereo = checked;
    applySettings();
}

void DSDDemodGUI::on_squelchGate_valueChanged(int value)
{
	applySettings();
}

void DSDDemodGUI::on_squelch_valueChanged(int value)
{
	ui->squelchText->setText(QString("%1").arg(value / 10.0, 0, 'f', 1));
	applySettings();
}

void DSDDemodGUI::on_audioMute_toggled(bool checked)
{
    m_audioMute = checked;
    applySettings();
}

void DSDDemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
	/*
	if((widget == ui->spectrumContainer) && (DSDDemodGUI != NULL))
		m_dsdDemod->setSpectrum(m_threadedSampleSink->getMessageQueue(), rollDown);
	*/
}

void DSDDemodGUI::onMenuDoubleClicked()
{
	if (!m_basicSettingsShown)
	{
		m_basicSettingsShown = true;
		BasicChannelSettingsWidget* bcsw = new BasicChannelSettingsWidget(&m_channelMarker, this);
		bcsw->show();
	}
}

DSDDemodGUI::DSDDemodGUI(PluginAPI* pluginAPI, DeviceAPI *deviceAPI, QWidget* parent) :
	RollupWidget(parent),
	ui(new Ui::DSDDemodGUI),
	m_pluginAPI(pluginAPI),
	m_deviceAPI(deviceAPI),
	m_channelMarker(this),
	m_basicSettingsShown(false),
	m_doApplySettings(true),
	m_signalFormat(signalFormatNone),
	m_enableCosineFiltering(false),
	m_syncOrConstellation(false),
	m_slot1On(false),
	m_slot2On(false),
	m_tdmaStereo(false),
	m_squelchOpen(false),
	m_channelPowerDbAvg(20,0),
	m_tickCount(0)
{
	ui->setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose, true);

	connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
	connect(this, SIGNAL(menuDoubleClickEvent()), this, SLOT(onMenuDoubleClicked()));

	m_scopeVis = new ScopeVis(ui->glScope);
	m_dsdDemod = new DSDDemod(m_scopeVis);
	m_dsdDemod->registerGUI(this);

    ui->glScope->setSampleRate(48000);
    m_scopeVis->setSampleRate(48000);

	ui->glScope->connectTimer(m_pluginAPI->getMainWindow()->getMasterTimer());

	connect(&m_pluginAPI->getMainWindow()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

	ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::ReverseGold));

	m_channelizer = new Channelizer(m_dsdDemod);
	m_threadedChannelizer = new ThreadedSampleSink(m_channelizer, this);
	m_deviceAPI->addThreadedSink(m_threadedChannelizer);

	//m_channelMarker = new ChannelMarker(this);
	m_channelMarker.setColor(Qt::cyan);
	m_channelMarker.setBandwidth(10000);
	m_channelMarker.setCenterFrequency(0);
	m_channelMarker.setVisible(true);

	connect(&m_channelMarker, SIGNAL(changed()), this, SLOT(viewChanged()));

	m_deviceAPI->registerChannelInstance(m_channelID, this);
	m_deviceAPI->addChannelMarker(&m_channelMarker);
	m_deviceAPI->addRollupWidget(this);

	ui->scopeGUI->setBuddies(m_scopeVis->getInputMessageQueue(), m_scopeVis, ui->glScope);

	applySettings();
}

DSDDemodGUI::~DSDDemodGUI()
{
    m_deviceAPI->removeChannelInstance(this);
	m_deviceAPI->removeThreadedSink(m_threadedChannelizer);
	delete m_threadedChannelizer;
	delete m_channelizer;
	delete m_dsdDemod;
	//delete m_channelMarker;
	delete ui;
}

void DSDDemodGUI::applySettings()
{
	if (m_doApplySettings)
	{
		qDebug() << "DSDDemodGUI::applySettings";

		setTitleColor(m_channelMarker.getColor());

		m_channelizer->configure(m_channelizer->getInputMessageQueue(),
			48000,
			m_channelMarker.getCenterFrequency());

		ui->deltaFrequency->setValue(abs(m_channelMarker.getCenterFrequency()));
		ui->deltaMinus->setChecked(m_channelMarker.getCenterFrequency() < 0);
	    ui->rfBWText->setText(QString("%1k").arg(ui->rfBW->value() / 10.0, 0, 'f', 1));
	    ui->demodGainText->setText(QString("%1").arg(ui->demodGain->value() / 100.0, 0, 'f', 2));
	    ui->fmDeviationText->setText(QString("%1k").arg(ui->fmDeviation->value() / 10.0, 0, 'f', 1));
		ui->squelchGateText->setText(QString("%1").arg(ui->squelchGate->value() * 10.0, 0, 'f', 0));
	    ui->volumeText->setText(QString("%1").arg(ui->volume->value() / 10.0, 0, 'f', 1));
	    ui->enableCosineFiltering->setChecked(m_enableCosineFiltering);
	    ui->syncOrConstellation->setChecked(m_syncOrConstellation);
	    ui->slot1On->setChecked(m_slot1On);
        ui->slot2On->setChecked(m_slot2On);
        ui->tdmaStereoSplit->setChecked(m_tdmaStereo);

		m_dsdDemod->configure(m_dsdDemod->getInputMessageQueue(),
			ui->rfBW->value(),
			ui->demodGain->value(),
			ui->fmDeviation->value(),
			ui->volume->value(),
			DSDDemodBaudRates::getRate(ui->baudRate->currentIndex()),
			ui->squelchGate->value(), // in 10ths of ms
			ui->squelch->value(),
			ui->audioMute->isChecked(),
			m_enableCosineFiltering,
			m_syncOrConstellation,
			m_slot1On,
			m_slot2On,
			m_tdmaStereo);
	}
}

void DSDDemodGUI::leaveEvent(QEvent*)
{
	blockApplySettings(true);
	m_channelMarker.setHighlighted(false);
	blockApplySettings(false);
}

void DSDDemodGUI::enterEvent(QEvent*)
{
	blockApplySettings(true);
	m_channelMarker.setHighlighted(true);
	blockApplySettings(false);
}

void DSDDemodGUI::blockApplySettings(bool block)
{
	m_doApplySettings = !block;
}

void DSDDemodGUI::formatStatusText()
{
    switch (m_dsdDemod->getDecoder().getSyncType())
    {
    case DSDcc::DSDDecoder::DSDSyncDMRDataMS:
    case DSDcc::DSDDecoder::DSDSyncDMRDataP:
    case DSDcc::DSDDecoder::DSDSyncDMRVoiceMS:
    case DSDcc::DSDDecoder::DSDSyncDMRVoiceP:
        if (m_signalFormat != signalFormatDMR)
        {
            strcpy(m_formatStatusText, "Sta: __ S1: __________________________ S2: __________________________");
        }

        switch (m_dsdDemod->getDecoder().getStationType())
        {
        case DSDcc::DSDDecoder::DSDBaseStation:
            memcpy(&m_formatStatusText[5], "BS ", 3);
            break;
        case DSDcc::DSDDecoder::DSDMobileStation:
            memcpy(&m_formatStatusText[5], "MS ", 3);
            break;
        default:
            memcpy(&m_formatStatusText[5], "NA ", 3);
            break;
        }

        memcpy(&m_formatStatusText[12], m_dsdDemod->getDecoder().getSlot0Text(), 26);
        memcpy(&m_formatStatusText[43], m_dsdDemod->getDecoder().getSlot1Text(), 26);
        m_signalFormat = signalFormatDMR;
        break;
    case DSDcc::DSDDecoder::DSDSyncDStarHeaderN:
    case DSDcc::DSDDecoder::DSDSyncDStarHeaderP:
    case DSDcc::DSDDecoder::DSDSyncDStarN:
    case DSDcc::DSDDecoder::DSDSyncDStarP:
        if (m_signalFormat != signalFormatDStar)
        {
            strcpy(m_formatStatusText, "RPT1: ________ RPT2: ________ YOUR: ________ MY: ________/____");
        }

        {
            const std::string& rpt1 = m_dsdDemod->getDecoder().getDStarDecoder().getRpt1();
            const std::string& rpt2 = m_dsdDemod->getDecoder().getDStarDecoder().getRpt2();
            const std::string& mySign = m_dsdDemod->getDecoder().getDStarDecoder().getMySign();
            const std::string& yrSign = m_dsdDemod->getDecoder().getDStarDecoder().getYourSign();

            if (rpt1.length() > 0) { // 0 or 8
                memcpy(&m_formatStatusText[6], rpt1.c_str(), 8);
            }
            if (rpt2.length() > 0) { // 0 or 8
                memcpy(&m_formatStatusText[21], rpt2.c_str(), 8);
            }
            if (yrSign.length() > 0) { // 0 or 8
                memcpy(&m_formatStatusText[36], yrSign.c_str(), 8);
            }
            if (mySign.length() > 0) { // 0 or 13
                memcpy(&m_formatStatusText[49], mySign.c_str(), 13);
            }
        }

        m_formatStatusText[72] = '\0';
        m_signalFormat = signalFormatDStar;
        break;
    case DSDcc::DSDDecoder::DSDSyncDPMR:
        sprintf(m_formatStatusText, "%s CC: %04d OI: %08d CI: %08d",
                m_dpmrFrameTypes[(int) m_dsdDemod->getDecoder().getDPMRDecoder().getFrameType()],
                m_dsdDemod->getDecoder().getDPMRDecoder().getColorCode(),
                m_dsdDemod->getDecoder().getDPMRDecoder().getOwnId(),
                m_dsdDemod->getDecoder().getDPMRDecoder().getCalledId());
        m_signalFormat = signalFormatDPMR;
        break;
    case DSDcc::DSDDecoder::DSDSyncYSF:
        //           1    1    2    2    3    3    4    4    5    5    6    6    7
        // 0....5....0....5....0....5....0....5....0....5....0....5....0....5....0..
        // C V2 RI 0:7 WL000 ssssssssss>dddddddddd |UUUUUUUUUU>DDDDDDDDDD|44444
    	if (m_dsdDemod->getDecoder().getYSFDecoder().getFICHError() == DSDcc::DSDYSF::FICHNoError)
    	{
            sprintf(m_formatStatusText, "%s ", m_ysfChannelTypeText[(int) m_dsdDemod->getDecoder().getYSFDecoder().getFICH().getFrameInformation()]);
    	}
    	else
    	{
            sprintf(m_formatStatusText, "%d ", (int) m_dsdDemod->getDecoder().getYSFDecoder().getFICHError());
    	}

        sprintf(&m_formatStatusText[2], "%s %s %d:%d %c%c",
    			m_ysfDataTypeText[(int) m_dsdDemod->getDecoder().getYSFDecoder().getFICH().getDataType()],
				m_ysfCallModeText[(int) m_dsdDemod->getDecoder().getYSFDecoder().getFICH().getCallMode()],
				m_dsdDemod->getDecoder().getYSFDecoder().getFICH().getBlockTotal(),
				m_dsdDemod->getDecoder().getYSFDecoder().getFICH().getFrameTotal(),
				(m_dsdDemod->getDecoder().getYSFDecoder().getFICH().isNarrowMode() ? 'N' : 'W'),
				(m_dsdDemod->getDecoder().getYSFDecoder().getFICH().isInternetPath() ? 'I' : 'L'));

        if (m_dsdDemod->getDecoder().getYSFDecoder().getFICH().isSquelchCodeEnabled())
    	{
            sprintf(&m_formatStatusText[14], "%03d", m_dsdDemod->getDecoder().getYSFDecoder().getFICH().getSquelchCode());
    	}
    	else
    	{
            strcpy(&m_formatStatusText[14], "---");
    	}

        char dest[11];

        if ( m_dsdDemod->getDecoder().getYSFDecoder().radioIdMode())
        {
            sprintf(dest, "%-5s:%-5s",
                    m_dsdDemod->getDecoder().getYSFDecoder().getDestId(),
                    m_dsdDemod->getDecoder().getYSFDecoder().getSrcId());
        }
        else
        {
            sprintf(dest, "%-10s", m_dsdDemod->getDecoder().getYSFDecoder().getDest());
        }

        sprintf(&m_formatStatusText[17], "|%-10s>%s|%-10s>%-10s|%-5s",
                m_dsdDemod->getDecoder().getYSFDecoder().getSrc(),
                dest,
                m_dsdDemod->getDecoder().getYSFDecoder().getUplink(),
                m_dsdDemod->getDecoder().getYSFDecoder().getDownlink(),
                m_dsdDemod->getDecoder().getYSFDecoder().getRem4());

        m_signalFormat = signalFormatYSF;
    	break;
    default:
        m_signalFormat = signalFormatNone;
        m_formatStatusText[0] = '\0';
        break;
    }

    m_formatStatusText[72] = '\0'; // guard
}

void DSDDemodGUI::tick()
{
	Real powDb = CalcDb::dbPower(m_dsdDemod->getMagSq());
    m_channelPowerDbAvg.feed(powDb);
	ui->channelPower->setText(QString::number(m_channelPowerDbAvg.average(), 'f', 1));

	bool squelchOpen = m_dsdDemod->getSquelchOpen();

	if (squelchOpen != m_squelchOpen)
	{
		m_squelchOpen = squelchOpen;

		if (m_squelchOpen) {
			ui->audioMute->setStyleSheet("QToolButton { background-color : green; }");
		} else {
			ui->audioMute->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
		}
	}

	// "slow" updates

	if (m_tickCount < 10)
	{
	    m_tickCount++;
	}
	else
	{
	    ui->inLevelText->setText(QString::number(m_dsdDemod->getDecoder().getInLevel()));
        ui->inCarrierPosText->setText(QString::number(m_dsdDemod->getDecoder().getCarrierPos()));
        ui->zcPosText->setText(QString::number(m_dsdDemod->getDecoder().getZeroCrossingPos()));
        ui->symbolSyncQualityText->setText(QString::number(m_dsdDemod->getDecoder().getSymbolSyncQuality()));

        if (m_dsdDemod->getDecoder().getVoice1On()) {
            ui->slot1On->setStyleSheet("QToolButton { background-color : green; }");
        } else {
            ui->slot1On->setStyleSheet("QToolButton { background-color : rgb(79,79,79); }");
        }

        if (m_dsdDemod->getDecoder().getVoice2On()) {
            ui->slot2On->setStyleSheet("QToolButton { background-color : green; }");
        } else {
            ui->slot2On->setStyleSheet("QToolButton { background-color : rgb(79,79,79); }");
        }

        const char *frameTypeText = m_dsdDemod->getDecoder().getFrameTypeText();

	    if (frameTypeText[0] == '\0') {
	        ui->syncText->setStyleSheet("QLabel { background:rgb(53,53,53); }"); // turn off background
	    } else {
            ui->syncText->setStyleSheet("QLabel { background:rgb(37,53,39); }"); // turn on background
	    }

	    ui->syncText->setText(QString(frameTypeText));

	    formatStatusText();
	    ui->formatStatusText->setText(QString(m_formatStatusText));

	    if (m_formatStatusText[0] == '\0') {
	        ui->formatStatusText->setStyleSheet("QLabel { background:rgb(53,53,53); }"); // turn off background
	    } else {
            ui->formatStatusText->setStyleSheet("QLabel { background:rgb(37,53,39); }"); // turn on background
	    }

	    m_tickCount = 0;
	}
}

unsigned int DSDDemodBaudRates::getRate(unsigned int rate_index)
{
    if (rate_index < m_nb_rates)
    {
        return m_rates[rate_index];
    }
    else
    {
        return m_rates[m_defaultRateIndex];
    }
}

unsigned int DSDDemodBaudRates::getRateIndex(unsigned int rate)
{
    for (unsigned int i=0; i < m_nb_rates; i++)
    {
        if (rate == m_rates[i])
        {
            return i;
        }
    }

    return m_defaultRateIndex;
}
