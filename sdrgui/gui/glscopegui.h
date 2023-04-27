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
#include "dsp/glscopesettings.h"
#include "settings/serializable.h"

namespace Ui {
    class GLScopeGUI;
}

class MessageQueue;
class GLScope;

class SDRGUI_API GLScopeGUI : public QWidget, public Serializable {
    Q_OBJECT

public:
    explicit GLScopeGUI(QWidget* parent = 0);
    ~GLScopeGUI();

    void setBuddies(MessageQueue* messageQueue, ScopeVis* scopeVis, GLScope* glScope);

    void setSampleRate(int sampleRate);
    void resetToDefaults();
    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);
    virtual void formatTo(SWGSDRangel::SWGObject *swgObject) const;
    virtual void updateFrom(const QStringList& keys, const SWGSDRangel::SWGObject *swgObject);
    void updateSettings();

    bool handleMessage(Message* message);
    unsigned int getNbTraces() const { return m_settings.m_tracesData.size(); }
    unsigned int getNbTriggers() const { return m_settings.m_triggersData.size(); }
    void setNbStreams(unsigned int nbStreams); //!< Set number of streams with default names (indexes)
    void setStreams(const QStringList& streamNames); //!< Give streans by their names in a list

    // preconfiguration methods
    // global (first line):
    void setDisplayMode(GLScopeSettings::DisplayMode displayMode);
    void setTraceIntensity(int value);
    void setGridIntensity(int value);
    void setTimeBase(int step);
    void setTimeOffset(int step);
    void setTraceLength(int step);
    void setPreTrigger(int step);
    // trace (second line):
    void changeTrace(int traceIndex, const GLScopeSettings::TraceData& traceData);
    void addTrace(const GLScopeSettings::TraceData& traceData);
    void focusOnTrace(int traceIndex);
    // trigger (third line):
    void changeTrigger(int triggerIndex, const GLScopeSettings::TriggerData& triggerData);
    void addTrigger(const GLScopeSettings::TriggerData& triggerData);
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
        bool m_oldStateTrigStream;
        bool m_oldStateTrigMode;
        bool m_oldStateTrigCount;
        bool m_oldStateTrigPos;
        bool m_oldStateTrigNeg;
        bool m_oldStateTrigBoth;
        bool m_oldStateTrigLevelCoarse;
        bool m_oldStateTrigLevelFine;
        bool m_oldStateTrigDelayCoarse;
        bool m_oldStateTrigDelayFine;
        bool m_oldStateTrigColor;
    };

    class TraceUIBlocker
    {
    public:
        TraceUIBlocker(Ui::GLScopeGUI *ui);
        ~TraceUIBlocker();

        void unBlock();

    private:
        Ui::GLScopeGUI *m_ui;
        bool m_oldStateTraceStream;
        bool m_oldStateTrace;
        bool m_oldStateTraceAdd;
        bool m_oldStateTraceDel;
        bool m_oldStateTraceMode;
        bool m_oldStateAmp;
        bool m_oldStateAmpCoarse;
        bool m_oldStateAmpExp;
        bool m_oldStateOfsCoarse;
        bool m_oldStateOfsFine;
        bool m_oldStateOfsExp;
        bool m_oldStateTraceDelayCoarse;
        bool m_oldStateTraceDelayFine;
        bool m_oldStateTraceColor;
        bool m_oldStateTraceView;
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
    GLScopeSettings m_settings;

    int m_sampleRate;
    int m_timeBase;
    int m_timeOffset;
    QColor m_focusedTraceColor;
    QColor m_focusedTriggerColor;
    int m_ctlTraceIndex;    //!< controlled trace index
    int m_ctlTriggerIndex;  //!< controlled trigger index

    void applySettings(const GLScopeSettings& settings, bool force = false);
    void displaySettings();
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

    void fillTraceData(GLScopeSettings::TraceData& traceData);
    void fillTriggerData(GLScopeSettings::TriggerData& triggerData);
    void setTriggerUI(const GLScopeSettings::TriggerData& triggerData);
    void setTraceUI(const GLScopeSettings::TraceData& traceData);

    void fillProjectionCombo(QComboBox* comboBox);
    void disableLiveMode(bool disable);

    void settingsTraceAdd(const GLScopeSettings::TraceData& traceData);
    void settingsTraceChange(const GLScopeSettings::TraceData& traceData, uint32_t traceIndex);
    void settingsTraceDel(uint32_t traceIndex);
    void settingsTraceMove(uint32_t traceIndex, bool upElseDown);

    void settingsTriggerAdd(const GLScopeSettings::TriggerData& triggerData);
    void settingsTriggerChange(const GLScopeSettings::TriggerData& triggerData, uint32_t triggerIndex);
    void settingsTriggerDel(uint32_t triggerIndex);
    void settingsTriggerMove(uint32_t triggerIndex, bool upElseDown);

private slots:
    void onScopeSampleRateChanged(int value);
    void onScopeTraceSizeChanged(uint32_t value);
    void onScopePreTriggerChanged(uint32_t value);
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
    void on_traceStream_currentIndexChanged(int index);
    void on_traceMode_currentIndexChanged(int index);
    void on_ampReset_clicked(bool checked);
    void on_amp_valueChanged(int value);
    void on_ampCoarse_valueChanged(int value);
    void on_ampExp_valueChanged(int value);
    void on_ofsReset_clicked(bool checked);
    void on_ofsCoarse_valueChanged(int value);
    void on_ofsFine_valueChanged(int value);
    void on_ofsExp_valueChanged(int value);
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
    void on_trigStream_currentIndexChanged(int index);
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
