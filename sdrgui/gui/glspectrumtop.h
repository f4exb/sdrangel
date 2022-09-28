///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Jon Beniston, M7RCE                                        //
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

#ifndef SDRGUI_GLSPECTRUMTOP_H_
#define SDRGUI_GLSPECTRUMTOP_H_

#include <QWidget>

#include "export.h"

class QSplitter;
class GLSpectrum;
class SpectrumMeasurements;

// Combines GLSpectrum in a QMainWindow with SpectrumMeasurements in a QDockWidget
class SDRGUI_API GLSpectrumTop : public QWidget {
    Q_OBJECT

public:
    GLSpectrumTop(QWidget *parent = nullptr);
    GLSpectrum *getSpectrum() const { return m_spectrum; }
    SpectrumMeasurements *getMeasurements() const { return m_measurements; }
    void setMeasurementsVisible(bool visible);

private:
    QSplitter *m_splitter;
    GLSpectrum *m_spectrum;
    SpectrumMeasurements *m_measurements;

};

#endif // SDRGUI_GLSPECTRUMTOP_H_
