#include "v4lgui.h"
#include "ui_v4lgui.h"
#include "plugin/pluginapi.h"

V4LGui::V4LGui(PluginAPI* pluginAPI, QWidget* parent) :
	QWidget(parent),
	ui(new Ui::V4LGui),
	m_pluginAPI(pluginAPI),
	m_settings(),
	m_sampleSource(NULL)
{
	ui->setupUi(this);
	ui->centerFrequency->setValueRange(7, 50000U, 1999000U);
	connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
	displaySettings();

	m_sampleSource = new V4LInput(m_pluginAPI->getMainWindowMessageQueue());
	m_pluginAPI->setSampleSource(m_sampleSource);
}

V4LGui::~V4LGui()
{
	delete ui;
}

void V4LGui::destroy()
{
	delete this;
}

void V4LGui::setName(const QString& name)
{
	setObjectName(name);
}

void V4LGui::resetToDefaults()
{
	m_generalSettings.resetToDefaults();
	m_settings.resetToDefaults();
	displaySettings();
	sendSettings();
}

QByteArray V4LGui::serializeGeneral() const
{
	return m_generalSettings.serialize();
}

bool V4LGui::deserializeGeneral(const QByteArray&data)
{
	if(m_generalSettings.deserialize(data)) {
		displaySettings();
		sendSettings();
		return true;
	} else {
		resetToDefaults();
		return false;
	}
}

quint64 V4LGui::getCenterFrequency() const
{
	return m_generalSettings.m_centerFrequency;
}

QByteArray V4LGui::serialize() const
{
	return m_settings.serialize();
}

bool V4LGui::deserialize(const QByteArray& data)
{
	if(m_settings.deserialize(data)) {
		displaySettings();
		sendSettings();
		return true;
	} else {
		resetToDefaults();
		return false;
	}
}

bool V4LGui::handleMessage(Message* message)
{
	return false;
}

void V4LGui::displaySettings()
{
	ui->centerFrequency->setValue(m_generalSettings.m_centerFrequency / 1000);
	ui->ifgain->setValue(1);
	ui->checkBoxL->setChecked(m_settings.m_lna);
        ui->checkBoxM->setChecked(m_settings.m_mix);
}

void V4LGui::sendSettings()
{
	if(!m_updateTimer.isActive())
		m_updateTimer.start(100);
}

void V4LGui::on_centerFrequency_changed(quint64 value)
{
	m_generalSettings.m_centerFrequency = value * 1000;
	sendSettings();
}

void V4LGui::on_ifgain_valueChanged(int value)
{
	if(value > 8)
		return;
	int gain = value * 6;
	ui->gainText->setText( tr("%1").arg(gain) );
	m_settings.m_gain = gain;
	sendSettings();
}

void V4LGui::updateHardware()
{
	V4LInput::MsgConfigureV4L* message = V4LInput::MsgConfigureV4L::create(m_generalSettings, m_settings);
	message->submit(m_pluginAPI->getDSPEngineMessageQueue());
	m_updateTimer.stop();
}

void V4LGui::on_checkBoxL_stateChanged(int state)
{
	if (state == Qt::Checked)
		m_settings.m_lna = 1;
	else
		m_settings.m_lna = 0;
	sendSettings();
}

void V4LGui::on_checkBoxM_stateChanged(int state)
{
	if (state == Qt::Checked)
		m_settings.m_mix = 1;
	else
		m_settings.m_mix = 0;
	sendSettings();
}
