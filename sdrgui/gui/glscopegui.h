///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#ifndef SDRBASE_GUI_GLSCOPENGGUI_H_
#define SDRBASE_GUI_GLSCOPENGGUI_H_

#include <QWidget>
#include <QComboBox>

#include "dsp/dsptypes.h"
#include "export.h"
#include "util/message.h"
#include "dsp/scopevis.h"
#include "settings/serializable.h"

namespace Ui {
    class GLScopeGUI;
}

class MessageQueue;
class GLScope;

class SDRGUI_API GLScopeGUI : public QWidget, public Serializable {
    Q_OBJECT

public:
    enum DisplayMode {
        DisplayXYH,
        DisplayXYV,
        DisplayX,
        DisplayY,
        DisplayPol
    };

    explicit GLScopeGUI(QWidget* parent = 0);
    ~GLScopeGUI();

    void setBuddies(MessageQueue* messageQueue, ScopeVis* scopeVis, GLScope* glScope);

    void setSampleRate(int sampleRate);
    void resetToDefaults();
    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    bool handleMessage(Message* message);

    // preconfiguration methods
    // global (first line):
    void setDisplayMode(DisplayMode displayMode);
    void setTraceIntensity(int value);
    void setGridIntensity(int value);
    void setTimeBase(int step);
    void setTimeOffset(int step);
    void setTraceLength(int step);
    void setPreTrigger(int step);
    // trace (second line):
    void changeTrace(int traceIndex, const ScopeVis::TraceData& traceData);
    void addTrace(const ScopeVis::TraceData& traceData);
    void focusOnTrace(int traceIndex);
    // trigger (third line):
    void changeTrigger(int triggerIndex, const ScopeVis::TriggerData& triggerData);
    void addTrigger(const ScopeVis::TriggerData& triggerData);
    void focusOnTrigger(int triggerIndex);
    void traceLengthChange();

private:
    class TrigUIBlocker
    {
    public:
        TrigUIBlocker(Ui::GLScopeGUI *ui);
        ~TrigUIBlocker();

        void unBlock();

    private:
        Ui::GLScopeGUI *m_ui;
        bool m_oldStateTrigMode;
        bool m_oldStateTrigCount;
        bool m_oldStateTrigPos;
        bool m_oldStateTrigNeg;
        bool m_oldStateTrigBoth;
        bool m_oldStateTrigLevelCoarse;
        bool m_oldStateTrigLevelFine;
        bool m_oldStateTrigDelayCoarse;
        bool m_oldStateTrigDelayFine;
    };

    class TraceUIBlocker
    {
    public:
        TraceUIBlocker(Ui::GLScopeGUI *ui);
        ~TraceUIBlocker();

        void unBlock();

    private:
        Ui::GLScopeGUI *m_ui;
        bool m_oldStateTrace;
        bool m_oldStateTraceAdd;
        bool m_oldStateTraceDel;
        bool m_oldStateTraceMode;
        bool m_oldStateAmp;
        bool m_oldStateOfsCoarse;
        bool m_oldStateOfsFine;
        bool m_oldStateTraceDelayCoarse;
        bool m_oldStateTraceDelayFine;
        bool m_oldStateTraceColor;
    };

    class MainUIBlocker
    {
    public:
        MainUIBlocker(Ui::GLScopeGUI *ui);
        ~MainUIBlocker();

        void unBlock();

    private:
        Ui::GLScopeGUI *m_ui;
        bool m_oldStateOnlyX;
        bool m_oldStateOnlyY;
        bool m_oldStateHorizontalXY;
        bool m_oldStateVerticalXY;
        bool m_oldStatePolar;
//        bool m_oldStateTime;
//        bool m_oldStateTimeOfs;
//        bool m_oldStateTraceLen;
    };

    Ui::GLScopeGUI* ui;

    MessageQueue* m_messageQueue;
    ScopeVis* m_scopeVis;
    GLScope* m_glScope;

    int m_sampleRate;
    int m_timeBase;
    int m_timeOffset;
    int m_traceLenMult;
    QColor m_focusedTraceColor;
    QColor m_focusedTriggerColor;

    static const double amps[27];

    void applySettings();
    // First row
    void setTraceIndexDisplay();
    void setTimeScaleDisplay();
    void setTraceLenDisplay();
    void setTimeOfsDisplay();
    // Second row
    void setAmpScaleDisplay();
    void setAmpOfsDisplay();
    void setTraceDelayDisplay();
    // Third row
    void setTrigIndexDisplay();
    void setTrigCountDisplay();
	void setTrigLevelDisplay();
	void setTrigDelayDisplay();
	void setTrigPreDisplay();

    void changeCurrentTrace();
    void changeCurrentTrigger();

    void fillTraceData(ScopeVis::TraceData& traceData);
    void fillTriggerData(ScopeVis::TriggerData& triggerData);
    void setTriggerUI(const ScopeVis::TriggerData& triggerData);
    void setTraceUI(const ScopeVis::TraceData& traceData);

    void fillProjectionCombo(QComboBox* comboBox);
    void disableLiveMode(bool disable);

private slots:
    void on_scope_sampleRateChanged(int value);
    void on_scope_traceSizeChanged(uint32_t value);
    void on_scope_preTriggerChanged(uint32_t value);
    // First row
    void on_onlyX_toggled(bool checked);
    void on_onlyY_toggled(bool checked);
    void on_horizontalXY_toggled(bool checked);
    void on_verticalXY_toggled(bool checked);
    void on_polar_toggled(bool checked);
    void on_polarPoints_toggled(bool checked);
    void on_polarGrid_toggled(bool checked);
    void on_traceIntensity_valueChanged(int value);
    void on_gridIntensity_valueChanged(int value);
    void on_time_valueChanged(int value);
    void on_timeOfs_valueChanged(int value);
    void on_traceLen_valueChanged(int value);
    // Second row
    void on_trace_valueChanged(int value);
    void on_traceAdd_clicked(bool checked);
    void on_traceDel_clicked(bool checked);
    void on_traceUp_clicked(bool checked);
    void on_traceDown_clicked(bool checked);
    void on_traceMode_currentIndexChanged(int index);
    void on_amp_valueChanged(int value);
    void on_ofsCoarse_valueChanged(int value);
    void on_ofsFine_valueChanged(int value);
    void on_traceDelayCoarse_valueChanged(int value);
    void on_traceDelayFine_valueChanged(int value);
    void on_traceView_toggled(bool checked);
    void on_traceColor_clicked();
    void on_memorySave_clicked(bool checked);
    void on_memoryLoad_clicked(bool checked);
    void on_mem_valueChanged(int value);
    // Third row
    void on_trig_valueChanged(int value);
    void on_trigAdd_clicked(bool checked);
    void on_trigDel_clicked(bool checked);
    void on_trigUp_clicked(bool checked);
    void on_trigDown_clicked(bool checked);
    void on_trigMode_currentIndexChanged(int index);
    void on_trigCount_valueChanged(int value);
    void on_trigPos_toggled(bool checked);
    void on_trigNeg_toggled(bool checked);
    void on_trigBoth_toggled(bool checked);
    void on_trigHoldoff_valueChanged(int value);
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
