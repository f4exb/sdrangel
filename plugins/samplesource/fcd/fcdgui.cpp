#include "fcdgui.h"
#include "ui_fcdgui.h"
#include "plugin/pluginapi.h"

FCDGui::FCDGui(PluginAPI* pluginAPI, QWidget* parent) :
	QWidget(parent),
	ui(new Ui::FCDGui),
	m_pluginAPI(pluginAPI),
	m_settings(),
	m_sampleSource(NULL)
{
	ui->setupUi(this);
	ui->centerFrequency->setValueRange(7, 420000U, 1900000U);
	connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
	displaySettings();

	m_sampleSource = new FCDInput(m_pluginAPI->getMainWindowMessageQueue());
	m_pluginAPI->setSampleSource(m_sampleSource);
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
	m_generalSettings.resetToDefaults();
	m_settings.resetToDefaults();
	displaySettings();
	sendSettings();
}

QByteArray FCDGui::serializeGeneral() const
{
	return m_generalSettings.serialize();
}

bool FCDGui::deserializeGeneral(const QByteArray&data)
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

quint64 FCDGui::getCenterFrequency() const
{
	return m_generalSettings.m_centerFrequency;
}

QByteArray FCDGui::serialize() const
{
	return m_settings.serialize();
}

bool FCDGui::deserialize(const QByteArray& data)
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

bool FCDGui::handleMessage(Message* message)
{
	return true;
}

void FCDGui::displaySettings()
{
	ui->centerFrequency->setValue(m_generalSettings.m_centerFrequency / 1000);
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
	m_generalSettings.m_centerFrequency = value * 1000;
	sendSettings();
}

void FCDGui::updateHardware()
{
	FCDInput::MsgConfigureFCD* message = FCDInput::MsgConfigureFCD::create(m_generalSettings, m_settings);
	message->submit(m_pluginAPI->getDSPEngineMessageQueue());
	m_updateTimer.stop();
}

void FCDGui::on_checkBoxR_stateChanged(int state)
{
	if (state == Qt::Checked) {
		ui->centerFrequency->setValueRange(7, 150U, 240000U);
		ui->centerFrequency->setValue(7000);
		m_generalSettings.m_centerFrequency = 7000 * 1000;
		m_settings.range = 1;
	}
	else {
		ui->centerFrequency->setValueRange(7, 420000U, 1900000U);
		ui->centerFrequency->setValue(434450);
		m_generalSettings.m_centerFrequency = 434450 * 1000;
		m_settings.range = 0;
	}
	sendSettings();
}

void FCDGui::on_checkBoxG_stateChanged(int state)
{
	if (state == Qt::Checked) {
		m_settings.gain = 1;
	} else {
		m_settings.gain = 0;
	}
	sendSettings();
}
void FCDGui::on_checkBoxB_stateChanged(int state)
{
	if (state == Qt::Checked) {
		m_settings.bias = 1;
	} else {
		m_settings.bias = 0;
	}
	sendSettings();
}
