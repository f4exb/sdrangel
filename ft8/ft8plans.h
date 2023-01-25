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
#ifndef ft8plans_h
#define ft8plans_h

#include <map>
#include <QMutex>

#include "export.h"

namespace FT8
{

class Plan;

class FT8_API FT8Plans
{
public:
    FT8Plans(FT8Plans& other) = delete;
    void operator=(const FT8Plans &) = delete;
    static FT8Plans *GetInstance();
    Plan *getPlan(int n);

protected:
    FT8Plans();
    ~FT8Plans();
    static FT8Plans *m_instance;

private:
    std::map<int, Plan*> m_plans;
    static QMutex m_globalPlanMutex;

};

} // namespace FT8

#endif // ft8plans_h
