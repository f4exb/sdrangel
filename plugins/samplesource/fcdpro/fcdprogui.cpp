#include "ui_fcdprogui.h"
#include "plugin/pluginapi.h"
#include "gui/colormapper.h"
#include "dsp/dspengine.h"
#include "fcdprogui.h"

FCDProGui::FCDProGui(PluginAPI* pluginAPI, QWidget* parent) :
	QWidget(parent),
	ui(new Ui::FCDProGui),
	m_pluginAPI(pluginAPI),
	m_settings(),
	m_sampleSource(NULL)
{
	ui->setupUi(this);
	ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::ReverseGold));
	ui->centerFrequency->setValueRange(7, 64000U, 1700000U);
	connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
	displaySettings();

	m_sampleSource = new FCDProInput();
	DSPEngine::instance()->setSource(m_sampleSource);
}

FCDProGui::~FCDProGui()
{
	delete ui;
}

void FCDProGui::destroy()
{
	delete this;
}

void FCDProGui::setName(const QString& name)
{
	setObjectName(name);
}

QString FCDProGui::getName() const
{
	return objectName();
}

void FCDProGui::resetToDefaults()
{
	m_settings.resetToDefaults();
	displaySettings();
	sendSettings();
}

qint64 FCDProGui::getCenterFrequency() const
{
	return m_settings.centerFrequency;
}

QByteArray FCDProGui::serialize() const
{
	return m_settings.serialize();
}

bool FCDProGui::deserialize(const QByteArray& data)
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

bool FCDProGui::handleMessage(const Message& message)
{
	return false;
}

void FCDProGui::displaySettings()
{
	ui->centerFrequency->setValue(m_settings.centerFrequency / 1000);
	ui->checkBoxR->setChecked(m_settings.range);
	ui->checkBoxG->setChecked(m_settings.gain);
	ui->checkBoxB->setChecked(m_settings.bias);
}

void FCDProGui::sendSettings()
{
	if(!m_updateTimer.isActive())
		m_updateTimer.start(100);
}

void FCDProGui::on_centerFrequency_changed(quint64 value)
{
	m_settings.centerFrequency = value * 1000;
	sendSettings();
}

void FCDProGui::updateHardware()
{
	FCDProInput::MsgConfigureFCD* message = FCDProInput::MsgConfigureFCD::create(m_settings);
	m_sampleSource->getInputMessageQueue()->push(message);
	m_updateTimer.stop();
}

void FCDProGui::on_checkBoxR_stateChanged(int state)
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

void FCDProGui::on_checkBoxG_stateChanged(int state)
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

void FCDProGui::on_checkBoxB_stateChanged(int state)
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
