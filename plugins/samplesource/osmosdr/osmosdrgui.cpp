#include "osmosdrgui.h"
#include "ui_osmosdrgui.h"
#include "plugin/pluginapi.h"

OsmoSDRGui::OsmoSDRGui(PluginAPI* pluginAPI, QWidget* parent) :
	QWidget(parent),
	ui(new Ui::OsmoSDRGui),
	m_pluginAPI(pluginAPI),
	m_settings(),
	m_sampleSource(NULL)
{
	ui->setupUi(this);
	ui->centerFrequency->setValueRange(7, 20000U, 2200000U);
	connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
	displaySettings();

	m_sampleSource = new OsmoSDRInput(m_pluginAPI->getMainWindowMessageQueue());
	m_pluginAPI->setSampleSource(m_sampleSource);
}

OsmoSDRGui::~OsmoSDRGui()
{
	delete ui;
}

void OsmoSDRGui::destroy()
{
	delete this;
}

void OsmoSDRGui::setName(const QString& name)
{
	setObjectName(name);
}

void OsmoSDRGui::resetToDefaults()
{
	m_generalSettings.resetToDefaults();
	m_settings.resetToDefaults();
	displaySettings();
	sendSettings();
}

QByteArray OsmoSDRGui::serializeGeneral() const
{
	return m_generalSettings.serialize();
}

bool OsmoSDRGui::deserializeGeneral(const QByteArray&data)
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

quint64 OsmoSDRGui::getCenterFrequency() const
{
	return m_generalSettings.m_centerFrequency;
}

QByteArray OsmoSDRGui::serialize() const
{
	return m_settings.serialize();
}

bool OsmoSDRGui::deserialize(const QByteArray& data)
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

bool OsmoSDRGui::handleMessage(Message* message)
{
	return false;
	/*
	if(message->id() == OsmoSDRInput::MsgReportOsmoSDR::ID()) {
		m_gains = ((RTLSDRInput::MsgReportRTLSDR*)message)->getGains();
		displaySettings();
		message->completed();
		return true;
	} else {
		return false;
	}*/
}
#if 0

OsmoSDRGui::OsmoSDRGui(MessageQueue* msgQueue, QWidget* parent) :
	SampleSourceGUI(parent),
	ui(new Ui::OsmoSDRGui),
	m_msgQueue(msgQueue),
	m_settings()
{
	ui->setupUi(this);
	ui->centerFrequency->setValueRange(7, 20000U, 2200000U);
	connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
	displaySettings();
}

OsmoSDRGui::~OsmoSDRGui()
{
	delete ui;
}

QString OsmoSDRGui::serializeSettings() const
{
	return m_settings.serialize();
}

bool OsmoSDRGui::deserializeSettings(const QString& settings)
{
	if(m_settings.deserialize(settings)) {
		displaySettings();
		sendSettings();
		return true;
	} else {
		return false;
	}
}

bool OsmoSDRGui::handleSourceMessage(DSPCmdSourceToGUI* cmd)
{
	return false;
}
#endif

void OsmoSDRGui::displaySettings()
{
	ui->centerFrequency->setValue(m_generalSettings.m_centerFrequency / 1000);
	ui->iqSwap->setChecked(m_settings.m_swapIQ);
	ui->decimation->setValue(m_settings.m_decimation);
	ui->e4000LNAGain->setValue(e4kLNAGainToIdx(m_settings.m_lnaGain));

	ui->e4000MixerGain->setCurrentIndex((m_settings.m_mixerGain - 40) / 80);
	if(m_settings.m_mixerEnhancement == 0)
		ui->e4000MixerEnh->setCurrentIndex(0);
	else ui->e4000MixerEnh->setCurrentIndex((m_settings.m_mixerEnhancement + 10) / 20);

	ui->e4000if1->setCurrentIndex((m_settings.m_if1gain + 30) / 90);
	ui->e4000if2->setCurrentIndex(m_settings.m_if2gain / 30);
	ui->e4000if3->setCurrentIndex(m_settings.m_if3gain / 30);
	ui->e4000if4->setCurrentIndex(m_settings.m_if4gain / 10);
	ui->e4000if5->setCurrentIndex(m_settings.m_if5gain / 30 - 1);
	ui->e4000if6->setCurrentIndex(m_settings.m_if6gain / 30 - 1);
	ui->filterI1->setValue(m_settings.m_opAmpI1);
	ui->filterI2->setValue(m_settings.m_opAmpI2);
	ui->filterQ1->setValue(m_settings.m_opAmpQ1);
	ui->filterQ2->setValue(m_settings.m_opAmpQ2);

	ui->e4kI->setValue(m_settings.m_dcOfsI);
	ui->e4kQ->setValue(m_settings.m_dcOfsQ);
}

void OsmoSDRGui::sendSettings()
{
	if(!m_updateTimer.isActive())
		m_updateTimer.start(100);
}

int OsmoSDRGui::e4kLNAGainToIdx(int gain) const
{
	static const quint32 gainList[13] = {
		-50, -25, 0, 25, 50, 75, 100, 125, 150, 175, 200, 250, 300
	};
	for(int i = 0; i < 13; i++) {
		if(gainList[i] == gain)
			return i;
	}
	return 0;
}

int OsmoSDRGui::e4kIdxToLNAGain(int idx) const
{
	static const quint32 gainList[13] = {
		-50, -25, 0, 25, 50, 75, 100, 125, 150, 175, 200, 250, 300
	};
	if((idx < 0) || (idx >= 13))
		return -50;
	else return gainList[idx];
}

void OsmoSDRGui::on_iqSwap_toggled(bool checked)
{
	m_settings.m_swapIQ = checked;
	sendSettings();
}

void OsmoSDRGui::on_e4000MixerGain_currentIndexChanged(int index)
{
	m_settings.m_mixerGain = index * 80 + 40;
	sendSettings();
}

void OsmoSDRGui::on_e4000MixerEnh_currentIndexChanged(int index)
{
	if(index == 0)
		m_settings.m_mixerEnhancement = 0;
	else m_settings.m_mixerEnhancement = index * 20 - 10;
	sendSettings();
}

void OsmoSDRGui::on_e4000if1_currentIndexChanged(int index)
{
	m_settings.m_if1gain = index * 90 - 30;
	sendSettings();
}

void OsmoSDRGui::on_e4000if2_currentIndexChanged(int index)
{
	m_settings.m_if2gain = index * 30;
	sendSettings();
}

void OsmoSDRGui::on_e4000if3_currentIndexChanged(int index)
{
	m_settings.m_if3gain = index * 30;
	sendSettings();
}

void OsmoSDRGui::on_e4000if4_currentIndexChanged(int index)
{
	m_settings.m_if4gain = index * 10;
	sendSettings();
}

void OsmoSDRGui::on_e4000if5_currentIndexChanged(int index)
{
	m_settings.m_if5gain = (index + 1) * 30;
	sendSettings();
}

void OsmoSDRGui::on_e4000if6_currentIndexChanged(int index)
{
	m_settings.m_if6gain = (index + 1) * 30;
	sendSettings();
}

void OsmoSDRGui::on_centerFrequency_changed(quint64 value)
{
	m_generalSettings.m_centerFrequency = value * 1000;
	sendSettings();
}

void OsmoSDRGui::on_filterI1_valueChanged(int value)
{
	m_settings.m_opAmpI1 = value;
	sendSettings();
}

void OsmoSDRGui::on_filterI2_valueChanged(int value)
{
	m_settings.m_opAmpI2 = value;
	sendSettings();
}

void OsmoSDRGui::on_filterQ1_valueChanged(int value)
{
	m_settings.m_opAmpQ1 = value;
	sendSettings();
}

void OsmoSDRGui::on_filterQ2_valueChanged(int value)
{
	m_settings.m_opAmpQ2 = value;
	sendSettings();
}

void OsmoSDRGui::on_decimation_valueChanged(int value)
{
	ui->decimationDisplay->setText(tr("1:%1").arg(1 << value));
	m_settings.m_decimation = value;
	sendSettings();
}

void OsmoSDRGui::on_e4000LNAGain_valueChanged(int value)
{
	int gain = e4kIdxToLNAGain(value);
	ui->e4000LNAGainDisplay->setText(tr("%1.%2").arg(gain / 10).arg(abs(gain % 10)));
	m_settings.m_lnaGain = gain;
	sendSettings();
}

void OsmoSDRGui::on_e4kI_valueChanged(int value)
{
	m_settings.m_dcOfsI = value;
	sendSettings();
}

void OsmoSDRGui::on_e4kQ_valueChanged(int value)
{
	m_settings.m_dcOfsQ = value;
	sendSettings();
}

void OsmoSDRGui::updateHardware()
{
	m_updateTimer.stop();
	Message* msg = OsmoSDRInput::MsgConfigureOsmoSDR::create(m_generalSettings, m_settings);
	msg->submit(m_pluginAPI->getDSPEngineMessageQueue());
}
