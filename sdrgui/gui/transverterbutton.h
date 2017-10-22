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

#ifndef SDRBASE_GUI_TRANSVERTERBUTTON_H_
#define SDRBASE_GUI_TRANSVERTERBUTTON_H_

#include <QPushButton>

class TransverterButton : public QPushButton {
    Q_OBJECT

public:
    TransverterButton(QWidget* parent = 0);
    qint64 getDeltaFrequency() const { return m_deltaFrequency; }
    bool getDeltaFrequencyAcive() const { return m_deltaFrequencyActive; }

    void setDeltaFrequency(qint64 deltaFrequency)
    {
        m_deltaFrequency = deltaFrequency;
        updateState();
    }

    void setDeltaFrequencyActive(bool active)
    {
        m_deltaFrequencyActive = active;
        updateState();
    }

private slots:
    void onClicked();

private:
    qint64 m_deltaFrequency;
    bool m_deltaFrequencyActive;

    void updateState();
};


#endif /* SDRBASE_GUI_TRANSVERTERBUTTON_H_ */
