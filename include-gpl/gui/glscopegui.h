#ifndef INCLUDE_GLSCOPEGUI_H
#define INCLUDE_GLSCOPEGUI_H

#include <QWidget>
#include "dsp/dsptypes.h"
#include "util/export.h"
#include "util/message.h"

namespace Ui {
	class GLScopeGUI;
}

class MessageQueue;
class ScopeVis;
class GLScope;

class SDRANGELOVE_API GLScopeGUI : public QWidget {
	Q_OBJECT

public:
	explicit GLScopeGUI(QWidget* parent = NULL);
	~GLScopeGUI();

	void setBuddies(MessageQueue* messageQueue, ScopeVis* scopeVis, GLScope* glScope);

	void setSampleRate(int sampleRate);
	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);

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
	qint32 m_amplification;
	qint32 m_ampOffset;
	int m_displayGridIntensity;
	qint32 m_triggerChannel;
	qint32 m_triggerLevel; // percent
	bool   m_triggerPositiveEdge;

	static const qreal amps[11];

	void applySettings();
	void applyTriggerSettings();
	void setTimeScaleDisplay();
	void setTimeOfsDisplay();
	void setAmpScaleDisplay();
	void setAmpOfsDisplay();
	void setTrigLevelDisplay();

private slots:
	void on_amp_valueChanged(int value);
	void on_ampOfs_valueChanged(int value);
	void on_scope_traceSizeChanged(int value);
	void on_scope_sampleRateChanged(int value);
	void on_time_valueChanged(int value);
	void on_timeOfs_valueChanged(int value);
	void on_dataMode_currentIndexChanged(int index);
	void on_gridIntensity_valueChanged(int index);

	void on_horizView_clicked();
	void on_vertView_clicked();
	void on_onlyPrimeView_clicked();
	void on_onlySecondView_clicked();

	void on_trigMode_currentIndexChanged(int index);
	void on_slopePos_clicked();
	void on_slopeNeg_clicked();
	void on_oneShot_clicked();
	void on_trigLevel_valueChanged(int value);
};

#endif // INCLUDE_GLSCOPEGUI_H
