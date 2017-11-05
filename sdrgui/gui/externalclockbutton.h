///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// OpenGL interface modernization.                                               //
// See: http://doc.qt.io/qt-5/qopenglshaderprogram.html                          //
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

#ifndef SDRBASE_GUI_EXTERNALCLOCKBUTTON_H_
#define SDRBASE_GUI_EXTERNALCLOCKBUTTON_H_

#include <QPushButton>

class ExternalClockButton : public QPushButton {
    Q_OBJECT

public:
    ExternalClockButton(QWidget* parent = 0);
    qint64 getExternalClockFrequency() const { return m_externalClockFrequency; }
    bool getExternalClockActive() const { return m_externalClockActive; }

    void setExternalClockFrequency(qint64 deltaFrequency)
    {
        m_externalClockFrequency = deltaFrequency;
        updateState();
    }

    void setExternalClockActive(bool active)
    {
        m_externalClockActive = active;
        updateState();
    }

private slots:
    void onClicked();

private:
    qint64 m_externalClockFrequency;
    bool m_externalClockActive;

    void updateState();
};


#endif /* SDRBASE_GUI_EXTERNALCLOCKBUTTON_H_ */
