#ifndef INCLUDE_GLSCOPEGUI_H
#define INCLUDE_GLSCOPEGUI_H

#include <QWidget>
#include "dsp/dsptypes.h"
#include "util/export.h"
#include "util/message.h"
#include "dsp/scopevis.h"
#include "settings/serializable.h"

namespace Ui {
	class GLScopeGUI;
}

class MessageQueue;
class GLScope;

class SDRANGEL_API GLScopeGUI : public QWidget, public Serializable {
	Q_OBJECT

public:
	explicit GLScopeGUI(QWidget* parent = NULL);
	~GLScopeGUI();

	void setBuddies(MessageQueue* messageQueue, ScopeVis* scopeVis, GLScope* glScope);

	void setSampleRate(int sampleRate);
	void resetToDefaults();
	virtual QByteArray serialize() const;
	virtual bool deserialize(const QByteArray& data);

	bool handleMessage(Message* message);

private:
	Ui::GLScopeGUI* ui;

	MessageQueue* m_messageQueue;
	ScopeVis* m_scopeVis;
	GLScope* m_glScope;

	int m_sampleRate;

	qint32 m_displayData;
	qint32 m_displayOrientation;
	qint32 m_displays;
	qint32 m_timeBase;
	qint32 m_timeOffset;
	qint32 m_amplification1;
	qint32 m_amp1OffsetCoarse;
	qint32 m_amp1OffsetFine;
	qint32 m_amplification2;
	qint32 m_amp2OffsetCoarse;
	qint32 m_amp2OffsetFine;
	int m_displayGridIntensity;
	int m_displayTraceIntensity;
	quint32 m_triggerIndex;
	qint32 m_triggerChannel[ScopeVis::m_nbTriggers];
	qint32 m_triggerLevelCoarse[ScopeVis::m_nbTriggers]; // percent of full range
	qint32 m_triggerLevelFine[ScopeVis::m_nbTriggers];   // percent of coarse
	bool   m_triggerPositiveEdge[ScopeVis::m_nbTriggers];
	bool   m_triggerBothEdges[ScopeVis::m_nbTriggers];
	qint32 m_triggerPre;
    qint32 m_triggerDelay[ScopeVis::m_nbTriggers];
    qint32 m_triggerCounts[ScopeVis::m_nbTriggers];
    qint32 m_traceLenMult;

	static const qreal amps[11];

	void applySettings();
	void applyTriggerSettings();
	void applyAllTriggerSettings();
	void setTimeScaleDisplay();
	void setTraceLenDisplay();
	void setTimeOfsDisplay();
	void setAmp1ScaleDisplay();
	void setAmp1OfsDisplay();
	void setAmp2ScaleDisplay();
	void setAmp2OfsDisplay();
	void setTrigLevelDisplay();
	void setTrigPreDisplay();
	void setTrigDelayDisplay();
	void setTrigUI(uint index);

private slots:
	void on_amp1_valueChanged(int value);
	void on_amp1OfsCoarse_valueChanged(int value);
	void on_amp1OfsFine_valueChanged(int value);
	void on_amp2_valueChanged(int value);
	void on_amp2OfsCoarse_valueChanged(int value);
	void on_amp2OfsFine_valueChanged(int value);
	void on_scope_traceSizeChanged(int value);
	void on_scope_sampleRateChanged(int value);
	void on_time_valueChanged(int value);
	void on_traceLen_valueChanged(int value);
	void on_timeOfs_valueChanged(int value);
	void on_dataMode_currentIndexChanged(int index);
	void on_gridIntensity_valueChanged(int index);
	void on_traceIntensity_valueChanged(int index);
	void on_trigPre_valueChanged(int value);
	void on_trigDelay_valueChanged(int value);
	void on_memIndex_valueChanged(int value);
	void on_trigCount_valueChanged(int value);
	void on_trigIndex_valueChanged(int value);

	void on_horizView_clicked();
	void on_vertView_clicked();
	void on_onlyPrimeView_clicked();
	void on_onlySecondView_clicked();

	void on_trigMode_currentIndexChanged(int index);
	void on_slopePos_clicked();
	void on_slopeNeg_clicked();
	void on_slopeBoth_clicked();
	void on_oneShot_clicked();
	void on_trigLevelCoarse_valueChanged(int value);
	void on_trigLevelFine_valueChanged(int value);
};

#endif // INCLUDE_GLSCOPEGUI_H
