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
	m_fftOverlap(0),
	m_fftWindow(FFTWindow::Hamming),
	m_refLevel(0),
	m_powerRange(100),
	m_decay(0),
	m_histogramLateHoldoff(1),
	m_histogramStroke(40),
	m_displayGridIntensity(5),
	m_displayTraceIntensity(50),
	m_displayWaterfall(true),
	m_invertedWaterfall(false),
	m_displayMaxHold(false),
	m_displayCurrent(false),
	m_displayHistogram(false),
	m_displayGrid(false),
	m_invert(true)
{
	ui->setupUi(this);
	for(int ref = 0; ref >= -110; ref -= 5)
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
	m_fftOverlap = 0;
	m_fftWindow = FFTWindow::Hamming;
	m_refLevel = 0;
	m_powerRange = 100;
	m_decay = 0;
	m_histogramLateHoldoff = 1;
	m_histogramStroke = 40;
	m_displayGridIntensity = 5,
	m_displayWaterfall = true;
	m_invertedWaterfall = false;
	m_displayMaxHold = false;
	m_displayHistogram = false;
	m_displayGrid = false;
	m_invert = true;
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
	s.writeS32(13, m_displayGridIntensity);
	s.writeS32(14, m_histogramLateHoldoff);
	s.writeS32(15, m_histogramStroke);
	s.writeBool(16, m_displayCurrent);
	s.writeS32(17, m_displayTraceIntensity);
	s.writeReal(18, m_glSpectrum->getWaterfallShare());
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
		d.readS32(2, &m_fftOverlap, 0);
		d.readS32(3, &m_fftWindow, FFTWindow::Hamming);
		d.readReal(4, &m_refLevel, 0);
		d.readReal(5, &m_powerRange, 100);
		d.readBool(6, &m_displayWaterfall, true);
		d.readBool(7, &m_invertedWaterfall, false);
		d.readBool(8, &m_displayMaxHold, false);
		d.readBool(9, &m_displayHistogram, false);
		d.readS32(10, &m_decay, 0);
		d.readBool(11, &m_displayGrid, false);
		d.readBool(12, &m_invert, true);
		d.readS32(13, &m_displayGridIntensity, 5);
		d.readS32(14, &m_histogramLateHoldoff, 1);
		d.readS32(15, &m_histogramStroke, 40);
		d.readBool(16, &m_displayCurrent, false);
		d.readS32(17, &m_displayTraceIntensity, 50);
		Real waterfallShare;
		d.readReal(18, &waterfallShare, 0.66);
		m_glSpectrum->setWaterfallShare(waterfallShare);
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
	ui->decay->setSliderPosition(m_decay);
	ui->holdoff->setSliderPosition(m_histogramLateHoldoff);
	ui->stroke->setSliderPosition(m_histogramStroke);
	ui->waterfall->setChecked(m_displayWaterfall);
	ui->maxHold->setChecked(m_displayMaxHold);
	ui->current->setChecked(m_displayCurrent);
	ui->histogram->setChecked(m_displayHistogram);
	ui->invert->setChecked(m_invert);
	ui->grid->setChecked(m_displayGrid);
	ui->gridIntensity->setSliderPosition(m_displayGridIntensity);

	ui->decay->setToolTip(QString("Decay: %1").arg(m_decay));
	ui->holdoff->setToolTip(QString("Holdoff: %1").arg(m_histogramLateHoldoff));
	ui->stroke->setToolTip(QString("Stroke: %1").arg(m_histogramStroke));
	ui->gridIntensity->setToolTip(QString("Grid intensity: %1").arg(m_displayGridIntensity));
	ui->traceIntensity->setToolTip(QString("Trace intensity: %1").arg(m_displayTraceIntensity));

	m_glSpectrum->setDisplayWaterfall(m_displayWaterfall);
	m_glSpectrum->setInvertedWaterfall(m_invertedWaterfall);
	m_glSpectrum->setDisplayMaxHold(m_displayMaxHold);
	m_glSpectrum->setDisplayCurrent(m_displayCurrent);
	m_glSpectrum->setDisplayHistogram(m_displayHistogram);
	m_glSpectrum->setDecay(m_decay);
	m_glSpectrum->setHistoLateHoldoff(m_histogramLateHoldoff);
	m_glSpectrum->setHistoStroke(m_histogramStroke);
	m_glSpectrum->setInvertedWaterfall(m_invert);
	m_glSpectrum->setDisplayGrid(m_displayGrid);
	m_glSpectrum->setDisplayGridIntensity(m_displayGridIntensity);

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

void GLSpectrumGUI::on_decay_valueChanged(int index)
{
	m_decay = index;
	ui->decay->setToolTip(QString("Decay: %1").arg(m_decay));
	if(m_glSpectrum != NULL)
		m_glSpectrum->setDecay(m_decay);
}

void GLSpectrumGUI::on_holdoff_valueChanged(int index)
{
	if (index < 0) {
		return;
	}
	m_histogramLateHoldoff = index;
	//ui->holdoff->setToolTip(QString("Holdoff: %1").arg(m_histogramLateHoldoff));
	if(m_glSpectrum != NULL) {
		applySettings();
	}
}

void GLSpectrumGUI::on_stroke_valueChanged(int index)
{
	if (index < 4) {
		return;
	}
	m_histogramStroke = index;
	//ui->stroke->setToolTip(QString("Stroke: %1").arg(m_histogramStroke));
	if(m_glSpectrum != NULL) {
		applySettings();
	}
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

void GLSpectrumGUI::on_current_toggled(bool checked)
{
	m_displayCurrent = checked;
	if(m_glSpectrum != NULL)
		m_glSpectrum->setDisplayCurrent(m_displayCurrent);
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

void GLSpectrumGUI::on_gridIntensity_valueChanged(int index)
{
	m_displayGridIntensity = index;
	ui->gridIntensity->setToolTip(QString("Grid intensity: %1").arg(m_displayGridIntensity));
	if(m_glSpectrum != NULL)
		m_glSpectrum->setDisplayGridIntensity(m_displayGridIntensity);
}

void GLSpectrumGUI::on_traceIntensity_valueChanged(int index)
{
	m_displayTraceIntensity = index;
	ui->traceIntensity->setToolTip(QString("Trace intensity: %1").arg(m_displayTraceIntensity));
	if(m_glSpectrum != NULL)
		m_glSpectrum->setDisplayTraceIntensity(m_displayTraceIntensity);
}

void GLSpectrumGUI::on_clearSpectrum_clicked(bool checked __attribute__((unused)))
{
	if(m_glSpectrum != NULL)
		m_glSpectrum->clearSpectrumHistogram();
}
