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

#include "transverterdialog.h"
#include "transverterbutton.h"

TransverterButton::TransverterButton(QWidget* parent) :
    QPushButton(parent),
    m_deltaFrequency(0),
    m_deltaFrequencyActive(false)
{
    setObjectName("TransverterButton");
    connect(this, SIGNAL(clicked()), this, SLOT(onClicked()));
}

void TransverterButton::onClicked()
{
    TransverterDialog transverterDialog(m_deltaFrequency, m_deltaFrequencyActive, this);
    transverterDialog.exec();
    updateState();
}

void TransverterButton::updateState()
{
    setToolTip(tr("Transverter frequency translation dialog. Delta frequency %1 MHz %2")
            .arg(m_deltaFrequency/1000000.0)
            .arg(m_deltaFrequencyActive ? "enabled" : "disabled"));

    if(m_deltaFrequencyActive)
    {
        setStyleSheet("TransverterButton { background:rgb(128, 70, 0); }");
    }
    else
    {
        setStyleSheet("TransverterButton { background:rgb(48, 48, 48); }");
    }

}
