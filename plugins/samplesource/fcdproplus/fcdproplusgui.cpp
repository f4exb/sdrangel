#include "ui_fcdproplusgui.h"
#include "plugin/pluginapi.h"
#include "gui/colormapper.h"
#include "dsp/dspengine.h"
#include "fcdproplusgui.h"

FCDProPlusGui::FCDProPlusGui(PluginAPI* pluginAPI, QWidget* parent) :
	QWidget(parent),
	ui(new Ui::FCDProPlusGui),
	m_pluginAPI(pluginAPI),
	m_settings(),
	m_sampleSource(NULL)
{
	ui->setupUi(this);
	ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::ReverseGold));
	ui->centerFrequency->setValueRange(7, 64000U, 1700000U);
	connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
	displaySettings();

	m_sampleSource = new FCDProPlusInput();
	DSPEngine::instance()->setSource(m_sampleSource);
}

FCDProPlusGui::~FCDProPlusGui()
{
	delete ui;
}

void FCDProPlusGui::destroy()
{
	delete this;
}

void FCDProPlusGui::setName(const QString& name)
{
	setObjectName(name);
}

QString FCDProPlusGui::getName() const
{
	return objectName();
}

void FCDProPlusGui::resetToDefaults()
{
	m_settings.resetToDefaults();
	displaySettings();
	sendSettings();
}

qint64 FCDProPlusGui::getCenterFrequency() const
{
	return m_settings.centerFrequency;
}

QByteArray FCDProPlusGui::serialize() const
{
	return m_settings.serialize();
}

bool FCDProPlusGui::deserialize(const QByteArray& data)
{
	if(m_settings.deserialize(data))
	{
		displaySettings();
		sendSettings();
		return true;
	}
	else
	{
		resetToDefaults();
		return false;
	}
}

bool FCDProPlusGui::handleMessage(const Message& message)
{
	return false;
}

void FCDProPlusGui::displaySettings()
{
	ui->centerFrequency->setValue(m_settings.centerFrequency / 1000);
	ui->checkBoxR->setChecked(m_settings.range);
	ui->checkBoxG->setChecked(m_settings.gain);
	ui->checkBoxB->setChecked(m_settings.bias);
}

void FCDProPlusGui::sendSettings()
{
	if(!m_updateTimer.isActive())
		m_updateTimer.start(100);
}

void FCDProPlusGui::on_centerFrequency_changed(quint64 value)
{
	m_settings.centerFrequency = value * 1000;
	sendSettings();
}

void FCDProPlusGui::updateHardware()
{
	FCDProPlusInput::MsgConfigureFCD* message = FCDProPlusInput::MsgConfigureFCD::create(m_settings);
	m_sampleSource->getInputMessageQueue()->push(message);
	m_updateTimer.stop();
}

void FCDProPlusGui::on_checkBoxR_stateChanged(int state)
{
	if (state == Qt::Checked) // FIXME: this is for the Pro+ version only!
	{
		ui->centerFrequency->setValueRange(7, 150U, 240000U);
		ui->centerFrequency->setValue(7000);
		m_settings.centerFrequency = 7000 * 1000;
		m_settings.range = 1;
	}
	else
	{
		ui->centerFrequency->setValueRange(7, 64000U, 1900000U);
		ui->centerFrequency->setValue(435000);
		m_settings.centerFrequency = 435000 * 1000;
		m_settings.range = 0;
	}

	sendSettings();
}

void FCDProPlusGui::on_checkBoxG_stateChanged(int state)
{
	if (state == Qt::Checked)
	{
		m_settings.gain = 1;
	}
	else
	{
		m_settings.gain = 0;
	}

	sendSettings();
}

void FCDProPlusGui::on_checkBoxB_stateChanged(int state)
{
	if (state == Qt::Checked)
	{
		m_settings.bias = 1;
	}
	else
	{
		m_settings.bias = 0;
	}

	sendSettings();
}
