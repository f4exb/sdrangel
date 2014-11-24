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
	ui->centerFrequency->setValueRange(7, 28500U, 1700000U);
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
	ui->samplerate->setValue(0);

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

void RTLSDRGui::on_samplerate_valueChanged(int value)
{
	int Rates[] = {288, 1024, 1536, 1152, 2048, 2500 };
	int newrate = Rates[value];
	ui->samplerateText->setText(tr("%1k").arg(newrate));
	m_settings.m_samplerate = newrate * 1000;
	sendSettings();
}

void RTLSDRGui::updateHardware()
{
	RTLSDRInput::MsgConfigureRTLSDR* message = RTLSDRInput::MsgConfigureRTLSDR::create(m_generalSettings, m_settings);
	message->submit(m_pluginAPI->getDSPEngineMessageQueue());
	m_updateTimer.stop();
}

void RTLSDRGui::on_checkBox_stateChanged(int state) {
	if (state == Qt::Checked){
		// Direct Modes: 0: off, 1: I, 2: Q, 3: NoMod.
		((RTLSDRInput*)m_sampleSource)->set_ds_mode(3);
		ui->gain->setEnabled(false);
		ui->centerFrequency->setValueRange(7, 1000U, 275000U);
		ui->centerFrequency->setValue(7000);
		m_generalSettings.m_centerFrequency = 7000 * 1000;
	}
	else {
		((RTLSDRInput*)m_sampleSource)->set_ds_mode(0);
		ui->gain->setEnabled(true);
		ui->centerFrequency->setValueRange(7, 28500U, 1700000U);
		ui->centerFrequency->setValue(434000);
		ui->gain->setValue(0);
		m_generalSettings.m_centerFrequency = 434000 * 1000;
	}
	sendSettings();
}

