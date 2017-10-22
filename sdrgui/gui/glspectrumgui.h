#ifndef INCLUDE_GLSPECTRUMGUI_H
#define INCLUDE_GLSPECTRUMGUI_H

#include <QWidget>
#include "dsp/dsptypes.h"
#include "util/export.h"
#include "settings/serializable.h"

namespace Ui {
	class GLSpectrumGUI;
}

class MessageQueue;
class SpectrumVis;
class GLSpectrum;

class SDRANGEL_API GLSpectrumGUI : public QWidget, public Serializable {
	Q_OBJECT

public:
	explicit GLSpectrumGUI(QWidget* parent = NULL);
	~GLSpectrumGUI();

	void setBuddies(MessageQueue* messageQueue, SpectrumVis* spectrumVis, GLSpectrum* glSpectrum);

	void resetToDefaults();
	virtual QByteArray serialize() const;
	virtual bool deserialize(const QByteArray& data);

private:
	Ui::GLSpectrumGUI* ui;

	MessageQueue* m_messageQueue;
	SpectrumVis* m_spectrumVis;
	GLSpectrum* m_glSpectrum;

	qint32 m_fftSize;
	qint32 m_fftOverlap;
	qint32 m_fftWindow;
	Real m_refLevel;
	Real m_powerRange;
	int m_decay;
	int m_histogramLateHoldoff;
	int m_histogramStroke;
	int m_displayGridIntensity;
	int m_displayTraceIntensity;
	bool m_displayWaterfall;
	bool m_invertedWaterfall;
	bool m_displayMaxHold;
	bool m_displayCurrent;
	bool m_displayHistogram;
	bool m_displayGrid;
	bool m_invert;

	void applySettings();

private slots:
	void on_fftWindow_currentIndexChanged(int index);
	void on_fftSize_currentIndexChanged(int index);
	void on_refLevel_currentIndexChanged(int index);
	void on_levelRange_currentIndexChanged(int index);
	void on_decay_valueChanged(int index);
	void on_holdoff_valueChanged(int index);
	void on_stroke_valueChanged(int index);
	void on_gridIntensity_valueChanged(int index);
	void on_traceIntensity_valueChanged(int index);

	void on_waterfall_toggled(bool checked);
	void on_histogram_toggled(bool checked);
	void on_maxHold_toggled(bool checked);
	void on_current_toggled(bool checked);
	void on_invert_toggled(bool checked);
	void on_grid_toggled(bool checked);
	void on_clearSpectrum_clicked(bool checked);
};

#endif // INCLUDE_GLSPECTRUMGUI_H
