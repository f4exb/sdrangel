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
    ButtonSwitch(parent),
    m_deltaFrequency(0)
{
    connect(this, SIGNAL(toggled(bool)), this, SLOT(onToggled(bool)));
}

void TransverterButton::onToggled(bool checked)
{
    if (checked) {
        TransverterDialog transverterDialog(&m_deltaFrequency, this);
        transverterDialog.exec();
        setToolTip(tr("Transverter frequency translation toggle. Delta frequency %1 MHz").arg(m_deltaFrequency/1000000.0));
    }
}

void TransverterButton::doToggle(bool checked)
{
    onToggled(checked);
}
