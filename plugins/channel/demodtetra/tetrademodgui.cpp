#include <QDockWidget>
#include <QMainWindow>
#include "tetrademodgui.h"
#include "ui_tetrademodgui.h"
#include "dsp/threadedsamplesink.h"
#include "dsp/channelizer.h"
#include "tetrademod.h"
#include "dsp/spectrumvis.h"
#include "gui/glspectrum.h"
#include "plugin/pluginapi.h"

TetraDemodGUI* TetraDemodGUI::create(PluginAPI* pluginAPI)
{
	QDockWidget* dock = pluginAPI->createMainWindowDock(Qt::RightDockWidgetArea, tr("Tetra Demodulator"));
	dock->setObjectName(QString::fromUtf8("Tetra Demodulator"));
	TetraDemodGUI* gui = new TetraDemodGUI(pluginAPI, dock);
	dock->setWidget(gui);
	return gui;
}

void TetraDemodGUI::destroy()
{
	delete m_dockWidget;
}

void TetraDemodGUI::setWidgetName(const QString& name)
{
	m_dockWidget->setObjectName(name);
}

void TetraDemodGUI::resetToDefaults()
{
}

QByteArray TetraDemodGUI::serializeGeneral() const
{
	return QByteArray();
}

bool TetraDemodGUI::deserializeGeneral(const QByteArray& data)
{
	return false;
}

QByteArray TetraDemodGUI::serialize() const
{
	return QByteArray();
}

bool TetraDemodGUI::deserialize(const QByteArray& data)
{
	return false;
}

bool TetraDemodGUI::handleMessage(Message* message)
{
	return false;
}

void TetraDemodGUI::viewChanged()
{
	m_channelizer->configure(m_threadedSampleSink->getMessageQueue(), 36000, m_channelMarker->getCenterFrequency());
}

TetraDemodGUI::TetraDemodGUI(PluginAPI* pluginAPI, QDockWidget* dockWidget, QWidget* parent) :
	PluginGUI(parent),
	ui(new Ui::TetraDemodGUI),
	m_pluginAPI(pluginAPI),
	m_dockWidget(dockWidget)
{
	ui->setupUi(this);

	m_spectrumVis = new SpectrumVis(ui->glSpectrum);
	m_tetraDemod = new TetraDemod(m_spectrumVis);
	m_channelizer = new Channelizer(m_tetraDemod);
	m_threadedSampleSink = new ThreadedSampleSink(m_channelizer);
	m_pluginAPI->addSampleSink(m_threadedSampleSink);

	ui->glSpectrum->setCenterFrequency(0);
	ui->glSpectrum->setSampleRate(36000);
	ui->glSpectrum->setDisplayWaterfall(true);
	ui->glSpectrum->setDisplayMaxHold(true);
	m_spectrumVis->configure(m_threadedSampleSink->getMessageQueue(), 64, 10, FFTWindow::BlackmanHarris);

	m_channelMarker = new ChannelMarker(this);
	m_channelMarker->setColor(Qt::darkGreen);
	m_channelMarker->setBandwidth(25000);
	m_channelMarker->setCenterFrequency(0);
	m_channelMarker->setVisible(true);
	connect(m_channelMarker, SIGNAL(changed()), this, SLOT(viewChanged()));
	m_pluginAPI->addChannelMarker(m_channelMarker);

	viewChanged();
}

TetraDemodGUI::~TetraDemodGUI()
{
	m_pluginAPI->removeSampleSink(m_threadedSampleSink);
	delete m_threadedSampleSink;
	delete m_channelizer;
	delete m_tetraDemod;
	delete m_spectrumVis;
	delete m_channelMarker;
	delete ui;
}

void TetraDemodGUI::on_test_clicked()
{
	m_tetraDemod->configure(m_threadedSampleSink->getMessageQueue());
}
