///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_FT8DEMOD_FT8BUFFER_H
#define INCLUDE_FT8DEMOD_FT8BUFFER_H

#include <QObject>
#include <QMutex>

class FT8Buffer : public QObject
{
    Q_OBJECT
public:
    FT8Buffer();
    ~FT8Buffer();

    void feed(int16_t sample);
    void getCurrentBuffer(int16_t *bufferCopy);

private:
    int16_t *m_buffer;
    int m_bufferSize;
    int m_sampleIndex;
    QMutex m_mutex;
};

#endif // INCLUDE_FT8DEMOD_FT8BUFFER_H
