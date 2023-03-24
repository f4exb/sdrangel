///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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

#ifndef SDRGUI_GUI_COURSEDEVIATIONINDICATOR_H
#define SDRGUI_GUI_COURSEDEVIATIONINDICATOR_H

#include <QWidget>

#include "export.h"

// Aircraft Course Deviation Indicator (CDI)
class SDRGUI_API CourseDeviationIndicator : public QWidget {
    Q_OBJECT

public:

    enum Mode {
        LOC,
        GS,
        // TODO: BOTH
    };

    explicit CourseDeviationIndicator(QWidget *parent = nullptr);
    void setLocalizerDDM(float ddm);
    float getLocazlierDDM() const { return m_localizerDDM; }
    void setGlideSlopeDDM(float ddm);
    float getGlideSlopeDDM() const { return m_glideSlopeDDM; }
    void setMode(Mode mode);
    Mode getMode() const { return m_mode; }

    void paintEvent(QPaintEvent *event);
protected:

private:
    float m_localizerDDM;
    float m_glideSlopeDDM;
    Mode m_mode;

private slots:


};

#endif // SDRGUI_GUI_COURSEDEVIATIONINDICATOR_H

