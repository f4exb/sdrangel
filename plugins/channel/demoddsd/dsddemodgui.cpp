#include <QDockWidget>
#include <QMainWindow>
#include <QDebug>
#include "ui_dsddemodgui.h"
#include "dsp/threadedsamplesink.h"
#include "dsp/channelizer.h"
#include "dsp/scopevis.h"
#include "gui/glscope.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "gui/basicchannelsettingswidget.h"
#include "dsp/dspengine.h"
#include "mainwindow.h"

#include "dsddemod.h"
#include "dsddemodgui.h"

DSDDemodGUI* DSDDemodGUI::create(PluginAPI* pluginAPI)
{
    DSDDemodGUI* gui = new DSDDemodGUI(pluginAPI);
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

DSDDemodGUI::DSDDemodGUI(PluginAPI* pluginAPI, QWidget* parent) :
	RollupWidget(parent),
	ui(new Ui::DSDDemodGUI),
	m_pluginAPI(pluginAPI),
	m_channelMarker(this),
	m_basicSettingsShown(false),
	m_doApplySettings(true),
	m_squelchOpen(false),
	m_channelPowerDbAvg(20,0)
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
	DSPEngine::instance()->addThreadedSink(m_threadedChannelizer);

	//m_channelMarker = new ChannelMarker(this);
	m_channelMarker.setColor(Qt::cyan);
	m_channelMarker.setBandwidth(10000);
	m_channelMarker.setCenterFrequency(0);
	m_channelMarker.setVisible(true);

	connect(&m_channelMarker, SIGNAL(changed()), this, SLOT(viewChanged()));

	m_pluginAPI->addChannelMarker(&m_channelMarker);

	ui->scopeGUI->setBuddies(m_scopeVis->getInputMessageQueue(), m_scopeVis, ui->glScope);

	applySettings();
}

DSDDemodGUI::~DSDDemodGUI()
{
	m_pluginAPI->removeChannelInstance(this);
	DSPEngine::instance()->removeThreadedSink(m_threadedChannelizer);
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

		m_dsdDemod->configure(m_dsdDemod->getInputMessageQueue(),
			ui->rfBW->value(),
			ui->demodGain->value(),
			ui->fmDeviation->value(),
			ui->volume->value(),
			ui->squelchGate->value(), // in 10ths of ms
			ui->squelch->value(),
			ui->audioMute->isChecked());
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

void DSDDemodGUI::tick()
{
	Real powDb = CalcDb::dbPower(m_dsdDemod->getMag()) * 2;
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
}
