///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2019, 2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
//                                                                               //
// This is the code from ft8mon: https://github.com/rtmrtmrtmrtm/ft8mon          //
// reformatted and adapted to Qt and SDRangel context                            //
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

#include <QMutexLocker>

#include "ft8plan.h"
#include "ft8plans.h"

namespace FT8
{

FT8Plans* FT8Plans::m_instance= nullptr;
QMutex FT8Plans::m_globalPlanMutex;

FT8Plans::FT8Plans()
{}

FT8Plans::~FT8Plans()
{
    qDebug("FT8::FT8Plans::~FT8Plans: %lu plans to delete", m_plans.size());

    for (auto& plan : m_plans) {
        delete plan.second;
    }
}

FT8Plans *FT8Plans::GetInstance()
{
    if (!m_instance) {
        m_instance = new FT8Plans();
    }

    return m_instance;
}

Plan *FT8Plans::getPlan(int n)
{
    QMutexLocker mlock(&m_globalPlanMutex);

    if (m_plans.find(n) != m_plans.end()) {
        return m_plans[n];
    }

    fftwf_set_timelimit(5);

    m_plans[n] = new Plan(n);
    return m_plans[n];
}

}
