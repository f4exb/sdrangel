///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#ifndef PLUGINS_CHANNELTX_MODDSE_DSEMODFIFO_H_
#define PLUGINS_CHANNELTX_MODDSE_DSEMODFIFO_H_

#include <QObject>
#include <QMutex>

// Single producer / single consumer int16 FIFO carrying the shaped baseband
// (48 kS/s) from the processor thread to the DSP thread.
class DSEModFIFO : public QObject
{
    Q_OBJECT
public:
    DSEModFIFO();
    explicit DSEModFIFO(uint32_t numSamples);
    ~DSEModFIFO();

    void setSize(uint32_t numSamples);
    uint32_t getSize() const { return m_size; }
    uint32_t write(const int16_t* data, uint32_t numSamples);
    uint32_t readOne(int16_t* data);
    //! numSamples in one go (all or nothing: 0 when fewer are there), so that a sample pair is never split
    uint32_t read(int16_t* data, uint32_t numSamples);
    int getFill() const;
    void clear();

private:
    //! The fill without taking the mutex: for callers that already hold it
    int fillUnlocked() const;

    mutable QMutex m_mutex;   //!< getFill() is const and locks it too
    int16_t* m_fifo;
    uint32_t m_size;
    int m_writeIndex;
    int m_readIndex;
    bool m_fifoEmpty;

    void create(uint32_t numSamples);
};

#endif // PLUGINS_CHANNELTX_MODDSE_DSEMODFIFO_H_
