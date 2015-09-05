#include "ui_fcdproplusgui.h"
#include "plugin/pluginapi.h"
#include "gui/colormapper.h"
#include "dsp/dspengine.h"
#include "fcdproplusgui.h"
#include "fcdproplusconst.h"

FCDProPlusGui::FCDProPlusGui(PluginAPI* pluginAPI, QWidget* parent) :
	QWidget(parent),
	ui(new Ui::FCDProPlusGui),
	m_pluginAPI(pluginAPI),
	m_settings(),
	m_sampleSource(NULL)
{
	ui->setupUi(this);

	ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::ReverseGold));
	ui->centerFrequency->setValueRange(7, 400000U, 2000000U);

	ui->filterIF->clear();
	for (int i = 0; i < FCDProPlusConstants::fcdproplus_if_filter_nb_values(); i++)
	{
		ui->filterIF->addItem(QString(FCDProPlusConstants::if_filters[i].label.c_str()), i);
	}

	ui->filterRF->clear();
	for (int i = 0; i < FCDProPlusConstants::fcdproplus_rf_filter_nb_values(); i++)
	{
		ui->filterRF->addItem(QString(FCDProPlusConstants::rf_filters[i].label.c_str()), i);
	}

	connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
	displaySettings();

	m_sampleSource = new FCDProPlusInput();
	DSPEngine::instance()->setSource(m_sampleSource);
}

FCDProPlusGui::~FCDProPlusGui()
{
	delete ui;
}

void FCDProPlusGui::destroy()
{
	delete this;
}

void FCDProPlusGui::setName(const QString& name)
{
	setObjectName(name);
}

QString FCDProPlusGui::getName() const
{
	return objectName();
}

void FCDProPlusGui::resetToDefaults()
{
	m_settings.resetToDefaults();
	displaySettings();
	sendSettings();
}

qint64 FCDProPlusGui::getCenterFrequency() const
{
	return m_settings.centerFrequency;
}

QByteArray FCDProPlusGui::serialize() const
{
	return m_settings.serialize();
}

bool FCDProPlusGui::deserialize(const QByteArray& data)
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

bool FCDProPlusGui::handleMessage(const Message& message)
{
	return false;
}

void FCDProPlusGui::displaySettings()
{
	ui->centerFrequency->setValue(m_settings.centerFrequency / 1000);
	ui->checkBoxR->setChecked(m_settings.rangeLow);
	ui->checkBoxG->setChecked(m_settings.lnaGain);
	ui->checkBoxB->setChecked(m_settings.biasT);
}

void FCDProPlusGui::sendSettings()
{
	if(!m_updateTimer.isActive())
		m_updateTimer.start(100);
}

void FCDProPlusGui::on_centerFrequency_changed(quint64 value)
{
	m_settings.centerFrequency = value * 1000;
	sendSettings();
}

void FCDProPlusGui::updateHardware()
{
	FCDProPlusInput::MsgConfigureFCD* message = FCDProPlusInput::MsgConfigureFCD::create(m_settings);
	m_sampleSource->getInputMessageQueue()->push(message);
	m_updateTimer.stop();
}

void FCDProPlusGui::on_checkBoxR_stateChanged(int state)
{
	if (state == Qt::Checked)
	{
		ui->centerFrequency->setValueRange(7, 150U, 240000U);

		if ((ui->centerFrequency->getValue() < 150U) || (ui->centerFrequency->getValue() > 240000U))
		{
			ui->centerFrequency->setValue(7000);
			m_settings.centerFrequency = 7000 * 1000;
			m_settings.rangeLow = true;
		}
	}
	else
	{
		ui->centerFrequency->setValueRange(7, 400000U, 2000000U);

		if ((ui->centerFrequency->getValue() < 150U) || (ui->centerFrequency->getValue() > 240000U))
		{
			ui->centerFrequency->setValue(435000);
			m_settings.centerFrequency = 435000 * 1000;
			m_settings.rangeLow = false;
		}
	}

	sendSettings();
}

void FCDProPlusGui::on_checkBoxG_stateChanged(int state)
{
	m_settings.lnaGain = (state == Qt::Checked);
	sendSettings();
}

void FCDProPlusGui::on_checkBoxB_stateChanged(int state)
{
	m_settings.biasT = (state == Qt::Checked);
	sendSettings();
}

void FCDProPlusGui::on_mixGain_stateChanged(int state)
{
	m_settings.mixGain = (state == Qt::Checked);
	sendSettings();
}

void FCDProPlusGui::on_filterIF_currentIndexChanged(int index)
{
	m_settings.ifFilterIndex = index;
	sendSettings();
}

void FCDProPlusGui::on_filterRF_currentIndexChanged(int index)
{
	m_settings.rfFilterIndex = index;
	sendSettings();
}

void FCDProPlusGui::on_ifGain_valueChanged(int value)
{
	m_settings.ifGain = value;
	sendSettings();
}

