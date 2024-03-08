///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

// This source code file was last time modified by Arvo ES1JA on January 5th, 2019
// All changes are shown in the patch file coming together with the full JTDX source code.

#ifndef INCLUDE_CALLSIGN_H
#define INCLUDE_CALLSIGN_H

#include <QObject>
#include <QRegularExpression>

#include "export.h"
#include "countrydat.h"

class SDRBASE_API Callsign : public QObject
{
    Q_OBJECT

public:
    static Callsign *instance();
    Callsign();
    ~Callsign();

    static bool is_callsign(QString const& callsign);
    static bool is_compound_callsign(QString const&);
    static QString base_callsign(QString);
    static QString effective_prefix(QString);
    static QString striped_prefix(QString);
    CountryDat::CountryInfo getCountryInfo(QString const& callsign);

private:
    static const QRegularExpression valid_callsign_regexp;
    static const QRegularExpression prefix_re;
    CountryDat m_countryDat;
};

#endif
