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
	ui->centerFrequency->setValueRange(7, 20000U, 2200000U);
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
	if(V4LInput::MsgReportV4L::match(message)) {
		m_gains = ((V4LInput::MsgReportV4L*)message)->getGains();
		displaySettings();
		message->completed();
		return true;
	} else {
		return false;
	}
}

void V4LGui::displaySettings()
{
	ui->centerFrequency->setValue(m_generalSettings.m_centerFrequency / 1000);
	ui->samplerate->setValue(m_settings.m_samplerate);

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

void V4LGui::on_gain_valueChanged(int value)
{
	if(value > (int)m_gains.size())
		return;
	int gain = m_gains[value];
	ui->gainText->setText(tr("%1.%2").arg(gain / 10).arg(abs(gain % 10)));
	m_settings.m_gain = gain;
	sendSettings();
}

void V4LGui::on_samplerate_valueChanged(int value)
{
	int Rates[] = { 2500, 1536, 3072, 288, 1000, 0};
	int newrate = Rates[value];
	ui->samplerateText->setText(tr("%1kHz").arg(newrate));
	m_settings.m_samplerate = 1000 * newrate;
	sendSettings();
}

void V4LGui::updateHardware()
{
	V4LInput::MsgConfigureV4L* message = V4LInput::MsgConfigureV4L::create(m_generalSettings, m_settings);
	message->submit(m_pluginAPI->getDSPEngineMessageQueue());
	m_updateTimer.stop();
}
