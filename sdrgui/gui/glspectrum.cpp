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

#include <QSplitter>
#include <QVBoxLayout>
#include <QLabel>

#include "gui/glspectrum.h"
#include "gui/glspectrumview.h"
#include "gui/spectrummeasurements.h"

GLSpectrum::GLSpectrum(QWidget *parent) :
    QWidget(parent)
{
    m_splitter = new QSplitter(Qt::Vertical);
    m_spectrum = new GLSpectrumView();
    m_measurements = new SpectrumMeasurements();
    m_spectrum->setMeasurements(m_measurements);
    m_splitter->addWidget(m_spectrum);
    m_splitter->addWidget(m_measurements);
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_splitter);
    setLayout(layout);
    m_measurements->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void GLSpectrum::setMeasurementsVisible(bool visible)
{
    m_measurements->setVisible(visible);
}

void GLSpectrum::setMeasurementsPosition(SpectrumSettings::MeasurementsPosition position)
{
    switch (position)
    {
    case SpectrumSettings::PositionAbove:
        m_splitter->setOrientation(Qt::Vertical);
        m_splitter->insertWidget(0, m_measurements);
        break;
    case SpectrumSettings::PositionBelow:
        m_splitter->setOrientation(Qt::Vertical);
        m_splitter->insertWidget(0, m_spectrum);
        break;
    case SpectrumSettings::PositionLeft:
        m_splitter->setOrientation(Qt::Horizontal);
        m_splitter->insertWidget(0, m_measurements);
        break;
    case SpectrumSettings::PositionRight:
        m_splitter->setOrientation(Qt::Horizontal);
        m_splitter->insertWidget(0, m_spectrum);
        break;
    }
}
