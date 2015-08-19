#include <QDockWidget>
#include <QMainWindow>
#include "lorademodgui.h"
#include "ui_lorademodgui.h"
#include "lorademodgui.h"
#include "ui_lorademodgui.h"
#include "dsp/threadedsamplesink.h"
#include "dsp/channelizer.h"
#include "lorademod.h"
#include "dsp/spectrumvis.h"
#include "gui/glspectrum.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "gui/basicchannelsettingswidget.h"
#include "dsp/dspengine.h"

LoRaDemodGUI* LoRaDemodGUI::create(PluginAPI* pluginAPI)
{
	LoRaDemodGUI* gui = new LoRaDemodGUI(pluginAPI);
	return gui;
}

void LoRaDemodGUI::destroy()
{
	delete this;
}

void LoRaDemodGUI::setName(const QString& name)
{
	setObjectName(name);
}

QString LoRaDemodGUI::getName() const
{
	return objectName();
}

qint64 LoRaDemodGUI::getCenterFrequency() const {
	return m_channelMarker->getCenterFrequency();
}

void LoRaDemodGUI::resetToDefaults()
{
	blockApplySettings(true);
    
	ui->BW->setValue(0);
	ui->Spread->setValue(0);
    
	blockApplySettings(false);
	applySettings();
}

QByteArray LoRaDemodGUI::serialize() const
{
	SimpleSerializer s(1);
	s.writeS32(1, m_channelMarker->getCenterFrequency());
	s.writeS32(2, ui->BW->value());
	s.writeS32(3, ui->Spread->value());
	s.writeBlob(4, ui->spectrumGUI->serialize());
	return s.final();
}

bool LoRaDemodGUI::deserialize(const QByteArray& data)
{
	SimpleDeserializer d(data);

	if(!d.isValid()) 
    {
		resetToDefaults();
		return false;
	}

	if(d.getVersion() == 1) 
    {
		QByteArray bytetmp;
		qint32 tmp;
        
		blockApplySettings(true);
	    m_channelMarker->blockSignals(true);
        
		d.readS32(1, &tmp, 0);
		m_channelMarker->setCenterFrequency(tmp);
		d.readS32(2, &tmp, 0);
		ui->BW->setValue(tmp);
		d.readS32(3, &tmp, 0);
		ui->Spread->setValue(tmp);
		d.readBlob(4, &bytetmp);
		ui->spectrumGUI->deserialize(bytetmp);
        
		blockApplySettings(false);
	    m_channelMarker->blockSignals(false);
        
		applySettings();
		return true;
	} 
    else 
    {
		resetToDefaults();
		return false;
	}
}

bool LoRaDemodGUI::handleMessage(const Message& message)
{
	return false;
}

void LoRaDemodGUI::viewChanged()
{
	applySettings();
}

void LoRaDemodGUI::on_BW_valueChanged(int value)
{
	const int loraBW[] = BANDWIDTHSTRING;
	int thisBW = loraBW[value];
	ui->BWText->setText(QString("%1 Hz").arg(thisBW));
	m_channelMarker->setBandwidth(thisBW);
	applySettings();
}

void LoRaDemodGUI::on_Spread_valueChanged(int value)
{
}

void LoRaDemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
	/*
	if((widget == ui->spectrumContainer) && (m_LoRaDemod != NULL))
		m_LoRaDemod->setSpectrum(m_threadedSampleSink->getMessageQueue(), rollDown);
	*/
}

void LoRaDemodGUI::onMenuDoubleClicked()
{
	if(!m_basicSettingsShown) {
		m_basicSettingsShown = true;
		BasicChannelSettingsWidget* bcsw = new BasicChannelSettingsWidget(m_channelMarker, this);
		bcsw->show();
	}
}

LoRaDemodGUI::LoRaDemodGUI(PluginAPI* pluginAPI, QWidget* parent) :
	RollupWidget(parent),
	ui(new Ui::LoRaDemodGUI),
	m_pluginAPI(pluginAPI),
	m_basicSettingsShown(false),
	m_doApplySettings(true)
{
	ui->setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose, true);
	connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
	connect(this, SIGNAL(menuDoubleClickEvent()), this, SLOT(onMenuDoubleClicked()));

	m_spectrumVis = new SpectrumVis(ui->glSpectrum);
	m_LoRaDemod = new LoRaDemod(m_spectrumVis);
	m_channelizer = new Channelizer(m_LoRaDemod);
	DSPEngine::instance()->addThreadedSink(m_channelizer);

	ui->glSpectrum->setCenterFrequency(16000);
	ui->glSpectrum->setSampleRate(32000);
	ui->glSpectrum->setDisplayWaterfall(true);
	ui->glSpectrum->setDisplayMaxHold(true);

	setTitleColor(Qt::magenta);

	m_channelMarker = new ChannelMarker(this);
	m_channelMarker->setColor(Qt::magenta);
	m_channelMarker->setBandwidth(7813);
	m_channelMarker->setCenterFrequency(0);
	m_channelMarker->setVisible(true);
	connect(m_channelMarker, SIGNAL(changed()), this, SLOT(viewChanged()));
	m_pluginAPI->addChannelMarker(m_channelMarker);

	ui->spectrumGUI->setBuddies(m_channelizer->getInputMessageQueue(), m_spectrumVis, ui->glSpectrum);

	applySettings();
}

LoRaDemodGUI::~LoRaDemodGUI()
{
	m_pluginAPI->removeChannelInstance(this);
	DSPEngine::instance()->removeThreadedSink(m_channelizer);
	delete m_channelizer;
	delete m_LoRaDemod;
	delete m_spectrumVis;
	delete m_channelMarker;
	delete ui;
}

void LoRaDemodGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void LoRaDemodGUI::applySettings()
{
	const int  loraBW[] = BANDWIDTHSTRING;
	int thisBW = loraBW[ui->BW->value()];

	m_channelizer->configure(m_channelizer->getInputMessageQueue(),
		thisBW,
		m_channelMarker->getCenterFrequency());

	m_LoRaDemod->configure(m_LoRaDemod->getInputMessageQueue(), thisBW);
}
