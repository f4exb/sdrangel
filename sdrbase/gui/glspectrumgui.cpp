#include "gui/glspectrumgui.h"
#include "dsp/fftwindow.h"
#include "dsp/spectrumvis.h"
#include "gui/glspectrum.h"
#include "util/simpleserializer.h"
#include "ui_glspectrumgui.h"

GLSpectrumGUI::GLSpectrumGUI(QWidget* parent) :
	QWidget(parent),
	ui(new Ui::GLSpectrumGUI),
	m_messageQueue(NULL),
	m_spectrumVis(NULL),
	m_glSpectrum(NULL),
	m_fftSize(1024),
	m_fftOverlap(10),
	m_fftWindow(FFTWindow::Hamming),
	m_refLevel(0),
	m_powerRange(100),
	m_decay(0),
	m_displayWaterfall(false),
	m_invertedWaterfall(false),
	m_displayMaxHold(true),
	m_displayHistogram(true),
	m_displayGrid(true),
	m_invert(false)
{
	ui->setupUi(this);
	for(int ref = 0; ref >= -95; ref -= 5)
		ui->refLevel->addItem(QString("%1").arg(ref));
	for(int range = 100; range >= 5; range -= 5)
		ui->levelRange->addItem(QString("%1").arg(range));
}

GLSpectrumGUI::~GLSpectrumGUI()
{
	delete ui;
}

void GLSpectrumGUI::setBuddies(MessageQueue* messageQueue, SpectrumVis* spectrumVis, GLSpectrum* glSpectrum)
{
	m_messageQueue = messageQueue;
	m_spectrumVis = spectrumVis;
	m_glSpectrum = glSpectrum;
	applySettings();
}

void GLSpectrumGUI::resetToDefaults()
{
	m_fftSize = 1024;
	m_fftOverlap = 10;
	m_fftWindow = FFTWindow::Hamming;
	m_refLevel = 0;
	m_powerRange = 100;
	m_decay = 0;
	m_displayWaterfall = false;
	m_invertedWaterfall = false;
	m_displayMaxHold = true;
	m_displayHistogram = true;
	m_displayGrid = true;
	m_invert = false;
	applySettings();
}

QByteArray GLSpectrumGUI::serialize() const
{
	SimpleSerializer s(1);
	s.writeS32(1, m_fftSize);
	s.writeS32(2, m_fftOverlap);
	s.writeS32(3, m_fftWindow);
	s.writeReal(4, m_refLevel);
	s.writeReal(5, m_powerRange);
	s.writeBool(6, m_displayWaterfall);
	s.writeBool(7, m_invertedWaterfall);
	s.writeBool(8, m_displayMaxHold);
	s.writeBool(9, m_displayHistogram);
	s.writeS32(10, m_decay);
	s.writeBool(11, m_displayGrid);
	s.writeBool(12, m_invert);
	return s.final();
}

bool GLSpectrumGUI::deserialize(const QByteArray& data)
{
	SimpleDeserializer d(data);

	if(!d.isValid()) {
		resetToDefaults();
		return false;
	}

	if(d.getVersion() == 1) {
		d.readS32(1, &m_fftSize, 1024);
		d.readS32(2, &m_fftOverlap, 10);
		d.readS32(3, &m_fftWindow, FFTWindow::Hamming);
		d.readReal(4, &m_refLevel, 0);
		d.readReal(5, &m_powerRange, 100);
		d.readBool(6, &m_displayWaterfall, true);
		d.readBool(7, &m_invertedWaterfall, false);
		d.readBool(8, &m_displayMaxHold, false);
		d.readBool(9, &m_displayHistogram, true);
		d.readS32(10, &m_decay, 0);
		d.readBool(11, &m_displayGrid, true);
		d.readBool(12, &m_invert, false);
		applySettings();
		return true;
	} else {
		resetToDefaults();
		return false;
	}
}

void GLSpectrumGUI::applySettings()
{
	ui->fftWindow->setCurrentIndex(m_fftWindow);
	for(int i = 0; i < 6; i++) {
		if(m_fftSize == (1 << (i + 7))) {
			ui->fftSize->setCurrentIndex(i);
			break;
		}
	}
	ui->refLevel->setCurrentIndex(-m_refLevel / 5);
	ui->levelRange->setCurrentIndex((100 - m_powerRange) / 5);
	ui->decay->setCurrentIndex(m_decay + 2);
	ui->waterfall->setChecked(m_displayWaterfall);
	ui->maxHold->setChecked(m_displayMaxHold);
	ui->histogram->setChecked(m_displayHistogram);
	ui->invert->setChecked(m_invert);
	ui->grid->setChecked(m_displayGrid);

	m_glSpectrum->setDisplayWaterfall(m_displayWaterfall);
	m_glSpectrum->setInvertedWaterfall(m_invertedWaterfall);
	m_glSpectrum->setDisplayMaxHold(m_displayMaxHold);
	m_glSpectrum->setDisplayHistogram(m_displayHistogram);
	m_glSpectrum->setDecay(m_decay);
	m_glSpectrum->setInvertedWaterfall(m_invert);
	m_glSpectrum->setDisplayGrid(m_displayGrid);

	m_spectrumVis->configure(m_messageQueue, m_fftSize, m_fftOverlap, (FFTWindow::Function)m_fftWindow);
}

void GLSpectrumGUI::on_fftWindow_currentIndexChanged(int index)
{
	m_fftWindow = index;
	if(m_spectrumVis == NULL)
		return;
	m_spectrumVis->configure(m_messageQueue, m_fftSize, m_fftOverlap, (FFTWindow::Function)m_fftWindow);
}

void GLSpectrumGUI::on_fftSize_currentIndexChanged(int index)
{
	m_fftSize = 1 << (7 + index);
	if(m_spectrumVis != NULL)
		m_spectrumVis->configure(m_messageQueue, m_fftSize, m_fftOverlap, (FFTWindow::Function)m_fftWindow);
}

void GLSpectrumGUI::on_refLevel_currentIndexChanged(int index)
{
	m_refLevel = 0 - index * 5;
	if(m_glSpectrum != NULL)
		m_glSpectrum->setReferenceLevel(m_refLevel);
}

void GLSpectrumGUI::on_levelRange_currentIndexChanged(int index)
{
	m_powerRange = 100 - index * 5;
	if(m_glSpectrum != NULL)
		m_glSpectrum->setPowerRange(m_powerRange);
}

void GLSpectrumGUI::on_decay_currentIndexChanged(int index)
{
	m_decay = index - 2;
	if(m_glSpectrum != NULL)
		m_glSpectrum->setDecay(m_decay);
}

void GLSpectrumGUI::on_waterfall_toggled(bool checked)
{
	m_displayWaterfall = checked;
	if(m_glSpectrum != NULL)
		m_glSpectrum->setDisplayWaterfall(m_displayWaterfall);
}

void GLSpectrumGUI::on_histogram_toggled(bool checked)
{
	m_displayHistogram = checked;
	if(m_glSpectrum != NULL)
		m_glSpectrum->setDisplayHistogram(m_displayHistogram);
}

void GLSpectrumGUI::on_maxHold_toggled(bool checked)
{
	m_displayMaxHold = checked;
	if(m_glSpectrum != NULL)
		m_glSpectrum->setDisplayMaxHold(m_displayMaxHold);
}

void GLSpectrumGUI::on_invert_toggled(bool checked)
{
	m_invert = checked;
	if(m_glSpectrum != NULL)
		m_glSpectrum->setInvertedWaterfall(m_invert);
}

void GLSpectrumGUI::on_grid_toggled(bool checked)
{
	m_displayGrid = checked;
	if(m_glSpectrum != NULL)
		m_glSpectrum->setDisplayGrid(m_displayGrid);
}
