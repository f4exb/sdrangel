#include <QDebug>
#include "rtlsdrgui.h"
#include "ui_rtlsdrgui.h"
#include "plugin/pluginapi.h"
#include "gui/colormapper.h"
#include "dsp/dspengine.h"

RTLSDRGui::RTLSDRGui(PluginAPI* pluginAPI, QWidget* parent) :
	QWidget(parent),
	ui(new Ui::RTLSDRGui),
	m_pluginAPI(pluginAPI),
	m_settings(),
	m_sampleSource(0)
{
	ui->setupUi(this);
	ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::ReverseGold));
	ui->centerFrequency->setValueRange(7, 28500U, 1700000U);
	connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
	displaySettings();

	m_sampleSource = new RTLSDRInput();
	connect(m_sampleSource->getOutputMessageQueueToGUI(), SIGNAL(messageEnqueued()), this, SLOT(HandleSourceMessages()));
	DSPEngine::instance()->setSource(m_sampleSource);
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

QString RTLSDRGui::getName() const
{
	return objectName();
}

void RTLSDRGui::resetToDefaults()
{
	m_settings.resetToDefaults();
	displaySettings();
	sendSettings();
}

qint64 RTLSDRGui::getCenterFrequency() const
{
	return m_settings.m_centerFrequency;
}

QByteArray RTLSDRGui::serialize() const
{
	return m_settings.serialize();
}

bool RTLSDRGui::deserialize(const QByteArray& data)
{
	if (m_settings.deserialize(data))
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

bool RTLSDRGui::handleMessage(const Message& message)
{
	if (RTLSDRInput::MsgReportRTLSDR::match(message))
	{
		qDebug() << "RTLSDRGui::handleMessage: MsgReportRTLSDR";
		m_gains = ((RTLSDRInput::MsgReportRTLSDR&) message).getGains();
		displaySettings();
		return true;
	}
	else
	{
		return false;
	}
}

void RTLSDRGui::HandleSourceMessages()
{
	Message* message;

	while ((message = m_sampleSource->getOutputMessageQueueToGUI()->pop()) != 0)
	{
		qDebug("RTLSDRGui::HandleSourceMessages: message: %s", message->getIdentifier());

		if (handleMessage(*message))
		{
			delete message;
		}
	}
}

void RTLSDRGui::displaySettings()
{
	ui->centerFrequency->setValue(m_settings.m_centerFrequency / 1000);
	ui->samplerateText->setText(tr("%1k").arg(m_settings.m_devSampleRate / 1000));
	unsigned int sampleRateIndex = RTLSDRSampleRates::getRateIndex(m_settings.m_devSampleRate);
	ui->samplerate->setValue(sampleRateIndex);
	ui->ppm->setValue(m_settings.m_loPpmCorrection);
	ui->ppmText->setText(tr("%1").arg(m_settings.m_loPpmCorrection));
	ui->decimText->setText(tr("%1").arg(1<<m_settings.m_log2Decim));
	ui->decim->setValue(m_settings.m_log2Decim);

	if (m_gains.size() > 0)
	{
		int dist = abs(m_settings.m_gain - m_gains[0]);
		int pos = 0;

		for (uint i = 1; i < m_gains.size(); i++)
		{
			if (abs(m_settings.m_gain - m_gains[i]) < dist)
			{
				dist = abs(m_settings.m_gain - m_gains[i]);
				pos = i;
			}
		}

		ui->gainText->setText(tr("%1.%2").arg(m_gains[pos] / 10).arg(abs(m_gains[pos] % 10)));
		ui->gain->setMaximum(m_gains.size() - 1);
		ui->gain->setEnabled(true);
		ui->gain->setValue(pos);
	}
	else
	{
		ui->gain->setMaximum(0);
		ui->gain->setEnabled(false);
		ui->gain->setValue(0);
	}
}

void RTLSDRGui::sendSettings()
{
	if(!m_updateTimer.isActive())
	{
		m_updateTimer.start(100);
	}
}

void RTLSDRGui::on_centerFrequency_changed(quint64 value)
{
	m_settings.m_centerFrequency = value * 1000;
	sendSettings();
}

void RTLSDRGui::on_decim_valueChanged(int value)
{
	if ((value <0) || (value > 4))
	{
		return;
	}

	ui->decimText->setText(tr("%1").arg(1<<value));
	m_settings.m_log2Decim = value;

	sendSettings();
}

void RTLSDRGui::on_ppm_valueChanged(int value)
{
	if ((value > 99) || (value < -99))
	{
		return;
	}

	ui->ppmText->setText(tr("%1").arg(value));
	m_settings.m_loPpmCorrection = value;

	sendSettings();
}

void RTLSDRGui::on_gain_valueChanged(int value)
{
	if (value > (int)m_gains.size())
	{
		return;
	}

	int gain = m_gains[value];
	ui->gainText->setText(tr("%1.%2").arg(gain / 10).arg(abs(gain % 10)));
	m_settings.m_gain = gain;

	sendSettings();
}

void RTLSDRGui::on_samplerate_valueChanged(int value)
{
	int newrate = RTLSDRSampleRates::getRate(value);
	ui->samplerateText->setText(tr("%1k").arg(newrate));
	m_settings.m_devSampleRate = newrate * 1000;

	sendSettings();
}

void RTLSDRGui::updateHardware()
{
	RTLSDRInput::MsgConfigureRTLSDR* message = RTLSDRInput::MsgConfigureRTLSDR::create(m_settings);
	m_sampleSource->getInputMessageQueue()->push(message);
	m_updateTimer.stop();
}

void RTLSDRGui::on_checkBox_stateChanged(int state)
{
	if (state == Qt::Checked)
	{
		// Direct Modes: 0: off, 1: I, 2: Q, 3: NoMod.
		((RTLSDRInput*)m_sampleSource)->set_ds_mode(3);
		ui->gain->setEnabled(false);
		ui->centerFrequency->setValueRange(7, 1000U, 275000U);
		ui->centerFrequency->setValue(7000);
		m_settings.m_centerFrequency = 7000 * 1000;
	}
	else
	{
		((RTLSDRInput*)m_sampleSource)->set_ds_mode(0);
		ui->gain->setEnabled(true);
		ui->centerFrequency->setValueRange(7, 28500U, 1700000U);
		ui->centerFrequency->setValue(434000);
		ui->gain->setValue(0);
		m_settings.m_centerFrequency = 435000 * 1000;
	}

	sendSettings();
}

unsigned int RTLSDRSampleRates::m_rates[] = {288, 1152, 1536, 2304};
unsigned int RTLSDRSampleRates::m_nb_rates = 4;

unsigned int RTLSDRSampleRates::getRate(unsigned int rate_index)
{
	if (rate_index < m_nb_rates)
	{
		return m_rates[rate_index];
	}
	else
	{
		return m_rates[0];
	}
}

unsigned int RTLSDRSampleRates::getRateIndex(unsigned int rate)
{
	for (unsigned int i=0; i < m_nb_rates; i++)
	{
		if (rate/1000 == m_rates[i])
		{
			return i;
		}
	}

	return 0;
}
