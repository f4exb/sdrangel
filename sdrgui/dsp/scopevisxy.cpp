///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#include "scopevisxy.h"
#include "gui/tvscreen.h"
#include <unistd.h>

ScopeVisXY::ScopeVisXY(TVScreen *tvScreen) :
	m_tvScreen(tvScreen),
	m_scale(1.0),
	m_cols(0),
	m_rows(0),
	m_pixelsPerFrame(480),
	m_pixelCount(0),
	m_alphaTrace(128),
	m_alphaReset(128),
	m_plotRGB(qRgb(0, 255, 0)),
	m_gridRGB(qRgb(255, 255 ,255))
{
	setObjectName("ScopeVisXY");
	setPixelsPerFrame(m_pixelsPerFrame);
	m_tvScreen->setAlphaBlend(true);
}

ScopeVisXY::~ScopeVisXY()
{
}

void ScopeVisXY::setPixelsPerFrame(int pixelsPerFrame)
{
   m_pixelsPerFrame = pixelsPerFrame;
   m_pixelCount = 0;
   m_tvScreen->setAlphaReset();
}

void ScopeVisXY::feed(const SampleVector::const_iterator& cbegin, const SampleVector::const_iterator& end,
		bool positiveOnly __attribute__((unused)))
{
	SampleVector::const_iterator begin(cbegin);

	while (begin < end)
	{
		float x = m_scale * (begin->m_real/SDR_RX_SCALEF);
		float y = m_scale * (begin->m_imag/SDR_RX_SCALEF);

		int row = m_rows * ((1.0 - y) / 2.0);
		int col = m_cols * ((1.0 + x) / 2.0);

		row = row < 0 ? 0 : row >= m_rows ? m_rows-1 : row;
		col = col < 0 ? 0 : col >= m_cols ? m_cols-1 : col;

		m_tvScreen->selectRow(row);
		m_tvScreen->setDataColor(col, qRed(m_plotRGB), qGreen(m_plotRGB), qBlue(m_plotRGB), m_alphaTrace);

		++begin;
		m_pixelCount++;

		if (m_pixelCount == m_pixelsPerFrame)
		{
            int rows, cols;
            m_tvScreen->getSize(rows, cols);

            if ((rows != m_rows) || (cols != m_cols))
            {
                calculateGraticule(rows, cols);
                m_rows = rows;
                m_cols = cols;
            }

			m_tvScreen->renderImage(0);
			m_tvScreen->update();
			usleep(5000);
			m_tvScreen->resetImage(m_alphaReset);
            drawGraticule();
			m_pixelCount = 0;
		}
	}
}

void ScopeVisXY::start()
{
}

void ScopeVisXY::stop()
{
}

bool ScopeVisXY::handleMessage(const Message& message __attribute__((unused)))
{
	return false;
}

void ScopeVisXY::addGraticulePoint(const std::complex<float>& z) {
	m_graticule.push_back(z);
}

void ScopeVisXY::clearGraticule() {
	m_graticule.clear();
}

void ScopeVisXY::calculateGraticule(int rows, int cols)
{
    m_graticuleRows.clear();
    m_graticuleCols.clear();

    std::vector<std::complex<float> >::const_iterator grIt = m_graticule.begin();

    for (; grIt != m_graticule.end(); ++grIt)
    {
        int y = rows * ((1.0 - grIt->imag()) / 2.0);
        int x = cols * ((1.0 + grIt->real()) / 2.0);

        for (int d = -4; d <= 4; ++d)
        {
            m_graticuleRows.push_back(y+d);
            m_graticuleCols.push_back(x);
            m_graticuleRows.push_back(y);
            m_graticuleCols.push_back(x+d);
        }
    }
}

void ScopeVisXY::drawGraticule()
{
    std::vector<int>::const_iterator rowIt = m_graticuleRows.begin();
    std::vector<int>::const_iterator colIt = m_graticuleCols.begin();

    for(; (rowIt != m_graticuleRows.end()) && (colIt != m_graticuleCols.end()); ++rowIt, ++colIt)
    {
        m_tvScreen->selectRow(*rowIt);
        m_tvScreen->setDataColor(*colIt, qRed(m_gridRGB), qGreen(m_gridRGB), qBlue(m_gridRGB));
    }
}

