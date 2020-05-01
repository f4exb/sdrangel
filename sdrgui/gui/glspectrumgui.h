///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2020 F4EXB                                                 //
// written by Edouard Griffiths                                                  //
//                                                                               //
// OpenGL interface modernization.                                               //
// See: http://doc.qt.io/qt-5/qopenglshaderprogram.html                          //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_GLSPECTRUMGUI_H
#define INCLUDE_GLSPECTRUMGUI_H

#include <QWidget>

#include "dsp/dsptypes.h"
#include "dsp/glspectrumsettings.h"
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
        AvgModeFixed,
        AvgModeMax
    };

	explicit GLSpectrumGUI(QWidget* parent = NULL);
	~GLSpectrumGUI();

	void setBuddies(SpectrumVis* spectrumVis, GLSpectrum* glSpectrum);
    void setFFTSize(int log2FFTSize);

	void resetToDefaults();
	virtual QByteArray serialize() const;
	virtual bool deserialize(const QByteArray& data);

private:
	Ui::GLSpectrumGUI* ui;

	SpectrumVis* m_spectrumVis;
	GLSpectrum* m_glSpectrum;
	MessageQueue m_messageQueue;
    GLSpectrumSettings m_settings;
    bool m_doApplySettings;

    void blockApplySettings(bool block);
	void applySettings();
    void displaySettings();
    static int getAveragingMaxScale(GLSpectrumSettings::AveragingMode averagingMode); //!< Max power of 10 multiplier to 2,5,10 base ex: 2 -> 2,5,10,20,50,100,200,500,1000
	static int getAveragingIndex(int averaging, GLSpectrumSettings::AveragingMode averagingMode);
	static int getAveragingValue(int averagingIndex, GLSpectrumSettings::AveragingMode averagingMode);
	void setAveragingCombo();
	void setNumberStr(int n, QString& s);
	void setNumberStr(float v, int decimalPlaces, QString& s);
	void setAveragingToolitp();
	bool handleMessage(const Message& message);

private slots:
	void on_fftWindow_currentIndexChanged(int index);
	void on_fftSize_currentIndexChanged(int index);
	void on_refLevel_valueChanged(int value);
	void on_levelRange_valueChanged(int value);
	void on_decay_valueChanged(int index);
	void on_decayDivisor_valueChanged(int index);
	void on_stroke_valueChanged(int index);
	void on_gridIntensity_valueChanged(int index);
	void on_traceIntensity_valueChanged(int index);
	void on_averagingMode_currentIndexChanged(int index);
    void on_averaging_currentIndexChanged(int index);
    void on_linscale_toggled(bool checked);
    void on_wsSpectrum_toggled(bool checked);

	void on_waterfall_toggled(bool checked);
	void on_histogram_toggled(bool checked);
	void on_maxHold_toggled(bool checked);
	void on_current_toggled(bool checked);
	void on_invertWaterfall_toggled(bool checked);
	void on_grid_toggled(bool checked);
	void on_clearSpectrum_clicked(bool checked);
    void on_freeze_toggled(bool checked);

	void handleInputMessages();
    void openWebsocketSpectrumSettingsDialog(const QPoint& p);
};

#endif // INCLUDE_GLSPECTRUMGUI_H
