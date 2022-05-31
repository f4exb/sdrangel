///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_DOA2COMPASS_H
#define INCLUDE_DOA2COMPASS_H

#include <QWidget>

class DOA2Compass : public QWidget
{
    Q_OBJECT

public:
    DOA2Compass(QWidget *parent = nullptr);
    ~DOA2Compass();

    ///
    /// \brief Set all data (yaw, alt, height)
    ///
    /// \param azPos - forward (positive angles side relative to antennas direction) azimuth (in degrees)
    /// \param azNeg - reverse (negatve angles side relative to antennas direction) azimuth (in degrees)
    /// \param azAnt - antennas azimuth from 1 (connected to stream 0) to 2 (connected to stream 1)
    ///
    void setData(double azPos, double azNeg, double azAnt) {
        m_azPos = azPos;
        m_azNeg = azNeg;
        m_azAnt = azAnt;

        if( m_azPos < 0   ) m_azPos = 360 + m_azPos;
        if( m_azPos > 360 ) m_azPos = m_azPos - 360;

        if( m_azNeg < 0   ) m_azNeg = 360 + m_azNeg;
        if( m_azNeg > 360 ) m_azNeg = m_azNeg - 360;

        if( azAnt < 0   ) azAnt = 360 + azAnt;
        if( azAnt > 360 ) azAnt = azAnt - 360;

        emit canvasReplot();
    }

    ///
    /// \brief Set forward azimoth (in degree)
    /// \param val - forward azimoth (in degree)
    ///
    void setAzPos(double val)
    {
        m_azPos  = val;
        if( m_azPos < 0   ) m_azPos = 360 + m_azPos;
        if( m_azPos > 360 ) m_azPos = m_azPos - 360;

        emit canvasReplot();
    }

    ///
    /// \brief Set reverse azimoth (in degree)
    /// \param val - reverse azimoth (in degree)
    ///
    void setAzNeg(double val)
    {
        m_azNeg  = val;
        if( m_azNeg < 0   ) m_azNeg = 360 + m_azNeg;
        if( m_azNeg > 360 ) m_azNeg = m_azNeg - 360;

        emit canvasReplot();
    }

    ///
    /// \brief Set antennas azimoth (in degree)
    /// \param val - antennas azimoth (in degree)
    ///
    void setAzAnt(double val)
    {
        m_azAnt  = val;
        if( m_azAnt < 0   ) m_azAnt = 360 + m_azAnt;
        if( m_azAnt > 360 ) m_azAnt = m_azAnt - 360;

        emit canvasReplot();
    }

    ///
    /// \brief Set half blind angle (in degree)
    /// \param val - half blind angle (in degree)
    ///
    void setBlindAngle(double val)
    {
        m_blindAngle  = val;
        if( m_blindAngle < 0   ) m_blindAngle = 360 + m_blindAngle;
        if( m_blindAngle > 360 ) m_blindAngle = m_blindAngle - 360;

        emit canvasReplot();
    }

    ///
    /// \brief Set blind scector color
    /// \param val - blind sector color
    ///
    void setBlindColor(const QColor& color)
    {
        m_blindColor  = color;
        emit canvasReplot();
    }

    ///
    /// \brief Draw border along blind angle
    /// \param drawLegend - true to draw border along blind angle
    ///
    void setBlindAngleBorder(bool value)
    {
        m_blindAngleBorder = value;
        emit canvasReplot();
    }

    ///
    /// \brief Draw legend in the center of the compass
    /// \param drawLegend - true to draw legend else false
    ///
    void drawLegend(bool drawLegend)
    {
        m_drawLegend = drawLegend;
        emit canvasReplot();
    }

    ///
    /// \brief Get forward azimuth
    /// \return forward azimuth (in degree)
    ///
    double getAzPos() const {return m_azPos; }

    ///
    /// \brief Get reverse azimuth
    /// \return reverse azimuth (in degree)
    ///
    double getAzNeg() const {return m_azNeg; }

    ///
    /// \brief Get antennas azimuth
    /// \return antennas azimuth (in degree)
    ///
    double getAzAnt() const {return m_azAnt; }

signals:
    void canvasReplot(void);

protected slots:
    void canvasReplot_slot(void);

protected:
    void paintEvent(QPaintEvent *event);
    void resizeEvent(QResizeEvent *event);

protected:
    int     m_sizeMin, m_sizeMax;               ///< widget min/max size (in pixel)
    int     m_size, m_offset;                   ///< widget size and offset size
    bool    m_drawLegend;                       ///< draw legend in the center

    double  m_azPos;                            ///< forward (+) azimuth (in degree)
    double  m_azNeg;                            ///< reverse (-) azimuth (in degree)
    double  m_azAnt;                            ///< antennas azimuth from 1 (connected to stream 0) to 2 (connected to stream 1)
    double  m_blindAngle;                       //!< half the angle around antenna direction where DOA cannot be processed (when baseline distance exceeds half wavelength)
    QColor  m_blindColor;
    bool    m_blindAngleBorder;
};

#endif // INCLUDE_DOA2COMPASS_H
