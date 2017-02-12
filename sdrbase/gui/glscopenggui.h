///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef SDRBASE_GUI_GLSCOPENGGUI_H_
#define SDRBASE_GUI_GLSCOPENGGUI_H_

#include <QWidget>
#include <QComboBox>

#include "dsp/dsptypes.h"
#include "util/export.h"
#include "util/message.h"
#include "dsp/scopevisng.h"

namespace Ui {
    class GLScopeNGGUI;
}

class MessageQueue;
class GLScopeNG;

class SDRANGEL_API GLScopeNGGUI : public QWidget {
    Q_OBJECT

public:
    explicit GLScopeNGGUI(QWidget* parent = 0);
    ~GLScopeNGGUI();

    void setBuddies(MessageQueue* messageQueue, ScopeVisNG* scopeVis, GLScopeNG* glScope);

    void setSampleRate(int sampleRate);
    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);

    bool handleMessage(Message* message);

private:
    Ui::GLScopeNGGUI* ui;

    MessageQueue* m_messageQueue;
    ScopeVisNG* m_scopeVis;
    GLScopeNG* m_glScope;

    int m_sampleRate;
    int m_timeBase;
    int m_timeOffset;
    int m_traceLenMult;
    QColor m_focusedTraceColor;
    QColor m_focusedTriggerColor;

    static const double amps[11];

    void applySettings();
    // First row
    void setTraceIndexDisplay();
    void setTimeScaleDisplay();
    void setTraceLenDisplay();
    void setTimeOfsDisplay();
    // Second row
    void setAmpScaleDisplay();
    void setAmpOfsDisplay();
    // Third row
    void setTrigIndexDisplay();
    void setTrigCountDisplay();
	void setTrigLevelDisplay();
	void setTrigDelayDisplay();
	void setTrigPreDisplay();

    void changeCurrentTrace();
    void changeCurrentTrigger();

    void fillTraceData(ScopeVisNG::TraceData& traceData);
    void fillTriggerData(ScopeVisNG::TriggerData& triggerData);
    void setTriggerUI(ScopeVisNG::TriggerData& triggerData);

    void fillProjectionCombo(QComboBox* comboBox);

private slots:
    void on_scope_sampleRateChanged(int value);
    // First row
    void on_onlyX_toggled(bool checked);
    void on_onlyY_toggled(bool checked);
    void on_horizontalXY_toggled(bool checked);
    void on_verticalXY_toggled(bool checked);
    void on_polar_toggled(bool checked);
    void on_traceIntensity_valueChanged(int value);
    void on_gridIntensity_valueChanged(int value);
    void on_time_valueChanged(int value);
    void on_timeOfs_valueChanged(int value);
    void on_traceLen_valueChanged(int value);
    // Second row
    void on_traceMode_currentIndexChanged(int index);
    void on_amp_valueChanged(int value);
    void on_ofsCoarse_valueChanged(int value);
    void on_ofsFine_valueChanged(int value);
    void on_traceDelay_valueChanged(int value);
    void on_traceColor_clicked();
    // Third row
    void on_trig_valueChanged(int value);
    void on_trigAdd_clicked(bool checked);
    void on_trigDel_clicked(bool checked);
    void on_trigMode_currentIndexChanged(int index);
    void on_trigCount_valueChanged(int value);
    void on_trigPos_toggled(bool checked);
    void on_trigNeg_toggled(bool checked);
    void on_trigBoth_toggled(bool checked);
    void on_trigLevelCoarse_valueChanged(int value);
    void on_trigLevelFine_valueChanged(int value);
    void on_trigDelayCoarse_valueChanged(int value);
    void on_trigDelayFine_valueChanged(int value);
    void on_trigPre_valueChanged(int value);
    void on_trigColor_clicked();
    void on_trigOneShot_toggled(bool checked);
    void on_freerun_toggled(bool checked);
};


#endif /* SDRBASE_GUI_GLSCOPENGGUI_H_ */
