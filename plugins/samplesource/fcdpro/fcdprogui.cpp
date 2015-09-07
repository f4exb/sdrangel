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
	ui->checkBoxB->setChecked(m_settings.biasT);
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

void FCDProGui::on_ppm_valueChanged(int value)
{
	m_settings.LOppmTenths = value;
	displaySettings();
	sendSettings();
}

void FCDProGui::on_checkBoxB_stateChanged(int state)
{
	if (state == Qt::Checked)
	{
		m_settings.biasT = 1;
	}
	else
	{
		m_settings.biasT = 0;
	}

	sendSettings();
}

void FCDProGui::on_lnaGain_currentIndexChanged(int index)
{

}

void FCDProGui::on_rfFilter_currentIndexChanged(int index)
{

}

void FCDProGui::on_lnaEnhance_currentIndexChanged(int index)
{

}

void FCDProGui::on_band_currentIndexChanged(int index)
{

}

void FCDProGui::on_mixGain_currentIndexChanged(int index)
{

}

void FCDProGui::on_mixFilter_currentIndexChanged(int index)
{

}

void FCDProGui::on_bias_currentIndexChanged(int index)
{

}

void FCDProGui::on_mode_currentIndexChanged(int index)
{

}

void FCDProGui::on_gain1_currentIndexChanged(int index)
{

}

void FCDProGui::on_rcFilter_currentIndexChanged(int index)
{

}

void FCDProGui::on_gain2_currentIndexChanged(int index)
{

}

void FCDProGui::on_gain3_currentIndexChanged(int index)
{

}

void FCDProGui::on_gain4_currentIndexChanged(int index)
{

}

void FCDProGui::on_ifFilter_currentIndexChanged(int index)
{

}

void FCDProGui::on_gain5_currentIndexChanged(int index)
{

}

void FCDProGui::on_gain6_currentIndexChanged(int index)
{

}

void FCDProGui::updateHardware()
{
	FCDProInput::MsgConfigureFCD* message = FCDProInput::MsgConfigureFCD::create(m_settings);
	m_sampleSource->getInputMessageQueue()->push(message);
	m_updateTimer.stop();
}

