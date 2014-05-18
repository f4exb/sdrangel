#include "rtlsdrgui.h"
#include "ui_rtlsdrgui.h"
#include "plugin/pluginapi.h"

RTLSDRGui::RTLSDRGui(PluginAPI* pluginAPI, QWidget* parent) :
	QWidget(parent),
	ui(new Ui::RTLSDRGui),
	m_pluginAPI(pluginAPI),
	m_settings(),
	m_sampleSource(NULL)
{
	ui->setupUi(this);
	ui->centerFrequency->setValueRange(7, 20000U, 2200000U);
	connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
	displaySettings();

	m_sampleSource = new RTLSDRInput(m_pluginAPI->getMainWindowMessageQueue());
	m_pluginAPI->setSampleSource(m_sampleSource);
}

RTLSDRGui::~RTLSDRGui()
{
	delete ui;
}

void RTLSDRGui::destroy()
{
	delete this;
}

void RTLSDRGui::setName(const QString& name)
{
	setObjectName(name);
}

void RTLSDRGui::resetToDefaults()
{
	m_generalSettings.resetToDefaults();
	m_settings.resetToDefaults();
	displaySettings();
	sendSettings();
}

QByteArray RTLSDRGui::serializeGeneral() const
{
	return m_generalSettings.serialize();
}

bool RTLSDRGui::deserializeGeneral(const QByteArray&data)
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

quint64 RTLSDRGui::getCenterFrequency() const
{
	return m_generalSettings.m_centerFrequency;
}

QByteArray RTLSDRGui::serialize() const
{
	return m_settings.serialize();
}

bool RTLSDRGui::deserialize(const QByteArray& data)
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

bool RTLSDRGui::handleMessage(Message* message)
{
	if(RTLSDRInput::MsgReportRTLSDR::match(message)) {
		m_gains = ((RTLSDRInput::MsgReportRTLSDR*)message)->getGains();
		displaySettings();
		message->completed();
		return true;
	} else {
		return false;
	}
}

void RTLSDRGui::displaySettings()
{
	ui->centerFrequency->setValue(m_generalSettings.m_centerFrequency / 1000);
	ui->decimation->setValue(m_settings.m_decimation);

	if(m_gains.size() > 0) {
		int dist = abs(m_settings.m_gain - m_gains[0]);
		int pos = 0;
		for(uint i = 1; i < m_gains.size(); i++) {
			if(abs(m_settings.m_gain - m_gains[i]) < dist) {
				dist = abs(m_settings.m_gain - m_gains[i]);
				pos = i;
			}
		}
		ui->gainText->setText(tr("%1.%2").arg(m_gains[pos] / 10).arg(abs(m_gains[pos] % 10)));
		ui->gain->setMaximum(m_gains.size() - 1);
		ui->gain->setEnabled(true);
		ui->gain->setValue(pos);
	} else {
		ui->gain->setMaximum(0);
		ui->gain->setEnabled(false);
		ui->gain->setValue(0);
	}
}

void RTLSDRGui::sendSettings()
{
	if(!m_updateTimer.isActive())
		m_updateTimer.start(100);
}

void RTLSDRGui::on_centerFrequency_changed(quint64 value)
{
	m_generalSettings.m_centerFrequency = value * 1000;
	sendSettings();
}

void RTLSDRGui::on_gain_valueChanged(int value)
{
	if(value > (int)m_gains.size())
		return;
	int gain = m_gains[value];
	ui->gainText->setText(tr("%1.%2").arg(gain / 10).arg(abs(gain % 10)));
	m_settings.m_gain = gain;
	sendSettings();
}

void RTLSDRGui::on_decimation_valueChanged(int value)
{
	ui->decimationText->setText(tr("1:%1").arg(1 << value));
	m_settings.m_decimation = value;
	sendSettings();
}

void RTLSDRGui::updateHardware()
{
	RTLSDRInput::MsgConfigureRTLSDR* message = RTLSDRInput::MsgConfigureRTLSDR::create(m_generalSettings, m_settings);
	message->submit(m_pluginAPI->getDSPEngineMessageQueue());
	m_updateTimer.stop();
}
