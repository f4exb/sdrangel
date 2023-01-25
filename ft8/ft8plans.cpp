///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// This is the code from ft8mon: https://github.com/rtmrtmrtmrtm/ft8mon          //
// written by Robert Morris, AB1HL                                               //
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

    for (auto& plan : m_plans)
    {
        if ((plan->n_ == n) && (plan->type_ == Plan::M_FFTW_TYPE)) {
            return plan;
        }
    }

    fftwf_set_timelimit(5);

    Plan *p = new Plan(n);
    m_plans.push_back(p);

    return p;
}

}
