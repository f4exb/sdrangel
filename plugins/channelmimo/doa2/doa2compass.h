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
    /// \param y - yaw ( in degree)
    /// \param a - altitude ( in m)
    /// \param h - height from ground (in m)
    ///
    void setData(double y, double a, double h) {
        m_yaw = y;
        m_alt = a;
        m_h   = h;

        if( m_yaw < 0   ) m_yaw = 360 + m_yaw;
        if( m_yaw > 360 ) m_yaw = m_yaw - 360;

        emit canvasReplot();
    }

    ///
    /// \brief Set yaw angle (in degree)
    /// \param val - yaw angle (in degree)
    ///
    void setYaw(double val) {
        m_yaw  = val;
        if( m_yaw < 0   ) m_yaw = 360 + m_yaw;
        if( m_yaw > 360 ) m_yaw = m_yaw - 360;

        emit canvasReplot();
    }

    ///
    /// \brief Set altitude value
    /// \param val - altitude (in m)
    ///
    void setAlt(double val) {
        m_alt = val;

        emit canvasReplot();
    }

    ///
    /// \brief Set height from ground
    /// \param val - height (in m)
    ///
    void setH(double val) {
        m_h = val;

        emit canvasReplot();
    }

    ///
    /// \brief Get yaw angle
    /// \return yaw angle (in degree)
    ///
    double getYaw() {return m_yaw;}

    ///
    /// \brief Get altitude value
    /// \return altitude (in m)
    ///
    double getAlt() {return m_alt;}

    ///
    /// \brief Get height from ground
    /// \return height from ground (in m)
    ///
    double getH()   {return m_h;}

signals:
    void canvasReplot(void);

protected slots:
    void canvasReplot_slot(void);

protected:
    void paintEvent(QPaintEvent *event);
    void resizeEvent(QResizeEvent *event);
    void keyPressEvent(QKeyEvent *event);

protected:
    int     m_sizeMin, m_sizeMax;               ///< widget min/max size (in pixel)
    int     m_size, m_offset;                   ///< widget size and offset size

    double  m_yaw;                              ///< yaw angle (in degree)
    double  m_alt;                              ///< altitude (in m)
    double  m_h;                                ///< height from ground (in m)
};

#endif // INCLUDE_DOA2COMPASS_H
