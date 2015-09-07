#include "ui_fcdprogui.h"
#include "plugin/pluginapi.h"
#include "gui/colormapper.h"
#include "dsp/dspengine.h"
#include "fcdprogui.h"
#include "fcdproconst.h"

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

	ui->lnaGain->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_lna_gain_nb_values(); i++)
	{
		ui->lnaGain->addItem(QString(FCDProConstants::lna_gains[i].label.c_str()), i);
	}

	ui->rfFilter->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_rf_filter_nb_values(); i++)
	{
		ui->rfFilter->addItem(QString(FCDProConstants::rf_filters[i].label.c_str()), i);
	}

	ui->lnaEnhance->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_lna_enhance_nb_values(); i++)
	{
		ui->lnaEnhance->addItem(QString(FCDProConstants::lna_enhances[i].label.c_str()), i);
	}

	ui->band->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_band_nb_values(); i++)
	{
		ui->band->addItem(QString(FCDProConstants::bands[i].label.c_str()), i);
	}

	ui->mixGain->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_mixer_gain_nb_values(); i++)
	{
		ui->mixGain->addItem(QString(FCDProConstants::mixer_gains[i].label.c_str()), i);
	}

	ui->mixFilter->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_mixer_filter_nb_values(); i++)
	{
		ui->mixFilter->addItem(QString(FCDProConstants::mixer_filters[i].label.c_str()), i);
	}

	ui->bias->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_bias_current_nb_values(); i++)
	{
		ui->bias->addItem(QString(FCDProConstants::bias_currents[i].label.c_str()), i);
	}

	ui->mode->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_if_gain_mode_nb_values(); i++)
	{
		ui->mode->addItem(QString(FCDProConstants::if_gain_modes[i].label.c_str()), i);
	}

	ui->gain1->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_if_gain1_nb_values(); i++)
	{
		ui->gain1->addItem(QString(FCDProConstants::if_gains1[i].label.c_str()), i);
	}

	ui->rcFilter->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_if_rc_filter_nb_values(); i++)
	{
		ui->rcFilter->addItem(QString(FCDProConstants::if_rc_filters[i].label.c_str()), i);
	}

	ui->gain2->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_if_gain2_nb_values(); i++)
	{
		ui->gain2->addItem(QString(FCDProConstants::if_gains2[i].label.c_str()), i);
	}

	ui->gain3->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_if_gain3_nb_values(); i++)
	{
		ui->gain3->addItem(QString(FCDProConstants::if_gains3[i].label.c_str()), i);
	}

	ui->gain4->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_if_gain4_nb_values(); i++)
	{
		ui->gain4->addItem(QString(FCDProConstants::if_gains4[i].label.c_str()), i);
	}

	ui->ifFilter->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_if_filter_nb_values(); i++)
	{
		ui->ifFilter->addItem(QString(FCDProConstants::if_filters[i].label.c_str()), i);
	}

	ui->gain5->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_if_gain5_nb_values(); i++)
	{
		ui->gain5->addItem(QString(FCDProConstants::if_gains5[i].label.c_str()), i);
	}

	ui->gain6->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_if_gain6_nb_values(); i++)
	{
		ui->gain6->addItem(QString(FCDProConstants::if_gains6[i].label.c_str()), i);
	}

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
	ui->ppm->setValue(m_settings.LOppmTenths);
	ui->ppmText->setText(QString("%1").arg(QString::number(m_settings.LOppmTenths/10.0, 'f', 1)));

	ui->lnaGain->setCurrentIndex(m_settings.lnaGainIndex);
	ui->rfFilter->setCurrentIndex(m_settings.rfFilterIndex);
	ui->lnaEnhance->setCurrentIndex(m_settings.lnaEnhanceIndex);
	ui->band->setCurrentIndex(m_settings.bandIndex);
	ui->mixGain->setCurrentIndex(m_settings.mixerGainIndex);
	ui->mixFilter->setCurrentIndex(m_settings.mixerFilterIndex);
	ui->bias->setCurrentIndex(m_settings.biasCurrentIndex);
	ui->mode->setCurrentIndex(m_settings.modeIndex);
	ui->gain1->setCurrentIndex(m_settings.gain1Index);
	ui->gain2->setCurrentIndex(m_settings.gain2Index);
	ui->gain3->setCurrentIndex(m_settings.gain3Index);
	ui->gain4->setCurrentIndex(m_settings.gain4Index);
	ui->gain5->setCurrentIndex(m_settings.gain5Index);
	ui->gain6->setCurrentIndex(m_settings.gain6Index);
	ui->rcFilter->setCurrentIndex(m_settings.rcFilterIndex);
	ui->ifFilter->setCurrentIndex(m_settings.ifFilterIndex);
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
	m_settings.lnaGainIndex = index;
	sendSettings();
}

void FCDProGui::on_rfFilter_currentIndexChanged(int index)
{
	m_settings.rfFilterIndex = index;
	sendSettings();
}

void FCDProGui::on_lnaEnhance_currentIndexChanged(int index)
{
	m_settings.lnaEnhanceIndex = index;
	sendSettings();
}

void FCDProGui::on_band_currentIndexChanged(int index)
{
	m_settings.bandIndex = index;
	sendSettings();
}

void FCDProGui::on_mixGain_currentIndexChanged(int index)
{
	m_settings.mixerGainIndex = index;
	sendSettings();
}

void FCDProGui::on_mixFilter_currentIndexChanged(int index)
{
	m_settings.mixerFilterIndex = index;
	sendSettings();
}

void FCDProGui::on_bias_currentIndexChanged(int index)
{
	m_settings.biasCurrentIndex = index;
	sendSettings();
}

void FCDProGui::on_mode_currentIndexChanged(int index)
{
	m_settings.modeIndex = index;
	sendSettings();
}

void FCDProGui::on_gain1_currentIndexChanged(int index)
{
	m_settings.gain1Index = index;
	sendSettings();
}

void FCDProGui::on_rcFilter_currentIndexChanged(int index)
{
	m_settings.rcFilterIndex = index;
	sendSettings();
}

void FCDProGui::on_gain2_currentIndexChanged(int index)
{
	m_settings.gain2Index = index;
	sendSettings();
}

void FCDProGui::on_gain3_currentIndexChanged(int index)
{
	m_settings.gain3Index = index;
	sendSettings();
}

void FCDProGui::on_gain4_currentIndexChanged(int index)
{
	m_settings.gain4Index = index;
	sendSettings();
}

void FCDProGui::on_ifFilter_currentIndexChanged(int index)
{
	m_settings.ifFilterIndex = index;
	sendSettings();
}

void FCDProGui::on_gain5_currentIndexChanged(int index)
{
	m_settings.gain5Index = index;
	sendSettings();
}

void FCDProGui::on_gain6_currentIndexChanged(int index)
{
	m_settings.gain6Index = index;
	sendSettings();
}

void FCDProGui::updateHardware()
{
	FCDProInput::MsgConfigureFCD* message = FCDProInput::MsgConfigureFCD::create(m_settings);
	m_sampleSource->getInputMessageQueue()->push(message);
	m_updateTimer.stop();
}

