///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
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

#ifndef INCLUDE_SCOPEWINDOW_H
#define INCLUDE_SCOPEWINDOW_H

#include <QWidget>
#include "dsp/dsptypes.h"
#include "util/export.h"

class DSPEngine;

namespace Ui {
	class ScopeWindow;
}

class SDRANGELOVE_API ScopeWindow : public QWidget {
	Q_OBJECT

public:
	explicit ScopeWindow(DSPEngine* dspEngine, QWidget* parent = NULL);
	~ScopeWindow();

	void setSampleRate(int sampleRate);

	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);

private slots:
	void on_amp_valueChanged(int value);
	void on_scope_traceSizeChanged(int value);
	void on_time_valueChanged(int value);
	void on_timeOfs_valueChanged(int value);
	void on_displayMode_currentIndexChanged(int index);

	void on_horizView_clicked();
	void on_vertView_clicked();

private:
	Ui::ScopeWindow *ui;
	int m_sampleRate;

	qint32 m_displayData;
	qint32 m_displayOrientation;
	qint32 m_timeBase;
	qint32 m_timeOffset;
	qint32 m_amplification;

	void applySettings();
};

#endif // INCLUDE_SCOPEWINDOW_H
