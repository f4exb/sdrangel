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

#ifndef INCLUDE_UTIL_BAUDOT_H
#define INCLUDE_UTIL_BAUDOT_H

#include <QString>
#include <QDateTime>
#include <QMap>

#include "export.h"

class SDRBASE_API Baudot {

public:

    enum CharacterSet {
        ITA2,
        UK,
        EUROPEAN,
        US,
        RUSSIAN, // MTK-2
        MURRAY
    };

    // QString used for fractions in figure set
    static const QString m_ita2Letter[];
    static const QString m_ita2Figure[];
    static const QString m_ukLetter[];
    static const QString m_ukFigure[];
    static const QString m_europeanLetter[];
    static const QString m_europeanFigure[];
    static const QString m_usLetter[];
    static const QString m_usFigure[];
    static const QString m_russianLetter[];
    static const QString m_russianFigure[];
    static const QString m_murrayLetter[];
    static const QString m_murrayFigure[];

};

class SDRBASE_API BaudotDecoder {

public:

    BaudotDecoder();
    void setCharacterSet(Baudot::CharacterSet characterSet=Baudot::ITA2);
    void setUnshiftOnSpace(bool unshiftOnSpace);
    void init();
    QString decode(char bits);

private:

    Baudot::CharacterSet m_characterSet;
    bool m_unshiftOnSpace;
    const QString *m_letters;
    const QString *m_figures;
    bool m_figure;

};

#endif // INCLUDE_UTIL_BAUDOT_H

