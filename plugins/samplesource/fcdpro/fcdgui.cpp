#include "fcdgui.h"
#include "ui_fcdgui.h"
#include "plugin/pluginapi.h"
#include "gui/colormapper.h"
#include "dsp/dspengine.h"

FCDGui::FCDGui(PluginAPI* pluginAPI, QWidget* parent) :
	QWidget(parent),
	ui(new Ui::FCDGui),
	m_pluginAPI(pluginAPI),
	m_settings(),
	m_sampleSource(NULL)
{
	ui->setupUi(this);
	ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::ReverseGold));
	ui->centerFrequency->setValueRange(7, 64000U, 1700000U);
	connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
	displaySettings();

	m_sampleSource = new FCDInput();
	DSPEngine::instance()->setSource(m_sampleSource);
}

FCDGui::~FCDGui()
{
	delete ui;
}

void FCDGui::destroy()
{
	delete this;
}

void FCDGui::setName(const QString& name)
{
	setObjectName(name);
}

QString FCDGui::getName() const
{
	return objectName();
}

void FCDGui::resetToDefaults()
{
	m_settings.resetToDefaults();
	displaySettings();
	sendSettings();
}

qint64 FCDGui::getCenterFrequency() const
{
	return m_settings.centerFrequency;
}

QByteArray FCDGui::serialize() const
{
	return m_settings.serialize();
}

bool FCDGui::deserialize(const QByteArray& data)
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

bool FCDGui::handleMessage(const Message& message)
{
	return false;
}

void FCDGui::displaySettings()
{
	ui->centerFrequency->setValue(m_settings.centerFrequency / 1000);
	ui->checkBoxR->setChecked(m_settings.range);
	ui->checkBoxG->setChecked(m_settings.gain);
	ui->checkBoxB->setChecked(m_settings.bias);
}

void FCDGui::sendSettings()
{
	if(!m_updateTimer.isActive())
		m_updateTimer.start(100);
}

void FCDGui::on_centerFrequency_changed(quint64 value)
{
	m_settings.centerFrequency = value * 1000;
	sendSettings();
}

void FCDGui::updateHardware()
{
	FCDInput::MsgConfigureFCD* message = FCDInput::MsgConfigureFCD::create(m_settings);
	m_sampleSource->getInputMessageQueue()->push(message);
	m_updateTimer.stop();
}

void FCDGui::on_checkBoxR_stateChanged(int state)
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

void FCDGui::on_checkBoxG_stateChanged(int state)
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

void FCDGui::on_checkBoxB_stateChanged(int state)
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
