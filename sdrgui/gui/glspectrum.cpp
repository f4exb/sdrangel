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
    m_position = SpectrumSettings::PositionBelow;
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
    m_position = position;
}

void GLSpectrum::setMeasurementParams(SpectrumSettings::Measurement measurement,
                                      int centerFrequencyOffset, int bandwidth, int chSpacing, int adjChBandwidth,
                                      int harmonics, int peaks, bool highlight, int precision)
{
    m_spectrum->setMeasurementParams(measurement, centerFrequencyOffset, bandwidth, chSpacing, adjChBandwidth, harmonics, peaks, highlight, precision);
    // Resize splitter so there's just enough space for the measurements table
    // But don't use more than 50%
    QList<int> sizes = m_splitter->sizes();
    if (parentWidget() && (sizes[0] == 0) && (sizes[1] == 0))
    {
        // Initial sizing when first created
        QSize s = parentWidget()->size();
        switch (m_position)
        {
        case SpectrumSettings::PositionAbove:
            sizes[0] = m_measurements->sizeHint().height();
            sizes[1] = s.height() - sizes[0] - m_splitter->handleWidth();
            sizes[1] = std::max(sizes[1], sizes[0]);
            break;
        case SpectrumSettings::PositionLeft:
            sizes[0] = m_measurements->sizeHint().width();
            sizes[1] = s.width() - sizes[0] - m_splitter->handleWidth();
            sizes[1] = std::max(sizes[1], sizes[0]);
            break;
        case SpectrumSettings::PositionBelow:
            sizes[1] = m_measurements->sizeHint().height();
            sizes[0] = s.height() - sizes[1] - m_splitter->handleWidth();
            sizes[0] = std::max(sizes[0], sizes[1]);
            break;
        case SpectrumSettings::PositionRight:
            sizes[1] = m_measurements->sizeHint().width();
            sizes[0] = s.width() - sizes[1] - m_splitter->handleWidth();
            sizes[0] = std::max(sizes[0], sizes[1]);
            break;
        }
    }
    else
    {
        // When measurement type is changed when already visible
        int diff = 0;
        switch (m_position)
        {
        case SpectrumSettings::PositionAbove:
            diff = m_measurements->sizeHint().height() - sizes[0];
            sizes[0] += diff;
            sizes[1] -= diff;
            sizes[1] = std::max(sizes[1], sizes[0]);
            break;
        case SpectrumSettings::PositionLeft:
            diff = m_measurements->sizeHint().width() - sizes[0];
            sizes[0] += diff;
            sizes[1] -= diff;
            sizes[1] = std::max(sizes[1], sizes[0]);
            break;
        case SpectrumSettings::PositionBelow:
            diff = m_measurements->sizeHint().height() - sizes[1];
            sizes[1] += diff;
            sizes[0] -= diff;
            sizes[0] = std::max(sizes[0], sizes[1]);
            break;
        case SpectrumSettings::PositionRight:
            diff = m_measurements->sizeHint().width() - sizes[1];
            sizes[1] += diff;
            sizes[0] -= diff;
            sizes[0] = std::max(sizes[0], sizes[1]);
            break;
        }
    }
    m_splitter->setSizes(sizes);
    //resize(size().expandedTo(minimumSizeHint()));
}
