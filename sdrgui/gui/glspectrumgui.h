#ifndef INCLUDE_GLSPECTRUMGUI_H
#define INCLUDE_GLSPECTRUMGUI_H

#include <QWidget>
#include "dsp/dsptypes.h"
#include "export.h"
#include "settings/serializable.h"
#include "util/messagequeue.h"

namespace Ui {
	class GLSpectrumGUI;
}

class SpectrumVis;
class GLSpectrum;

class SDRGUI_API GLSpectrumGUI : public QWidget, public Serializable {
	Q_OBJECT

public:
    enum AveragingMode
    {
        AvgModeNone,
        AvgModeMoving,
        AvgModeFixed
    };

	explicit GLSpectrumGUI(QWidget* parent = NULL);
	~GLSpectrumGUI();

	void setBuddies(MessageQueue* messageQueue, SpectrumVis* spectrumVis, GLSpectrum* glSpectrum);

	void resetToDefaults();
	virtual QByteArray serialize() const;
	virtual bool deserialize(const QByteArray& data);

private:
	Ui::GLSpectrumGUI* ui;

	MessageQueue* m_messageQueueToVis;
	SpectrumVis* m_spectrumVis;
	GLSpectrum* m_glSpectrum;
	MessageQueue m_messageQueue;

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
	AveragingMode m_averagingMode;
	int m_averagingIndex;
	int m_averagingMaxScale; //!< Max power of 10 multiplier to 2,5,10 base ex: 2 -> 2,5,10,20,50,100,200,500,1000
	unsigned int m_averagingNb;
	bool m_linear; //!< linear else logarithmic scale

	void applySettings();
	int getAveragingIndex(int averaging) const;
	int getAveragingValue(int averagingIndex) const;
	void setAveragingCombo();
	void setNumberStr(int n, QString& s);
	void setNumberStr(float v, int decimalPlaces, QString& s);
	void setAveragingToolitp();
	bool handleMessage(const Message& message);

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
	void on_averagingMode_currentIndexChanged(int index);
    void on_averaging_currentIndexChanged(int index);
    void on_linscale_toggled(bool checked);

	void on_waterfall_toggled(bool checked);
	void on_histogram_toggled(bool checked);
	void on_maxHold_toggled(bool checked);
	void on_current_toggled(bool checked);
	void on_invert_toggled(bool checked);
	void on_grid_toggled(bool checked);
	void on_clearSpectrum_clicked(bool checked);

	void handleInputMessages();
};

#endif // INCLUDE_GLSPECTRUMGUI_H
