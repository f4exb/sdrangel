///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2016, 2018-2019 Edouard Griffiths, F4EXB <f4exb06@gmail.com> //
// Copyright (C) 2023 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#ifndef INCLUDE_FEATURE_SPIDPROTOCOL_H_
#define INCLUDE_FEATURE_SPIDPROTOCOL_H_

#include "controllerprotocol.h"

class SPIDProtocol : public ControllerProtocol
{
public:
    SPIDProtocol();
    void setAzimuthElevation(float azimuth, float elevation) override;
    void readData() override;
    void update() override;
private:
    bool m_spidStatusSent;
    bool m_spidSetSent;
    bool m_spidSetOutstanding;
};

#endif // INCLUDE_FEATURE_SPIDPROTOCOL_H_

