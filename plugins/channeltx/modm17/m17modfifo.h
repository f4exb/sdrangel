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

#ifndef PLUGINS_CHANNELTX_MODM17_M17MODFIFO_H_
#define PLUGINS_CHANNELTX_MODM17_M17MODFIFO_H_

#include <QObject>
#include <QMutex>

class M17ModFIFO: public QObject
{
	Q_OBJECT
public:
	M17ModFIFO();
	M17ModFIFO(uint32_t numSamples);
	~M17ModFIFO();

	void setSize(uint32_t numSamples);
    uint32_t getSize() const { return m_size; }
	uint32_t write(const int16_t* data, uint32_t numSamples);
    uint32_t readOne(int16_t* data);
    int getFill() const;

private:
	QMutex m_mutex;
	int16_t* m_fifo;
    uint32_t m_size;
    int m_writeIndex;
    int m_readIndex;
    bool m_fifoEmpty;

    void create(uint32_t numSamples);
};


#endif // PLUGINS_CHANNELTX_MODM17_M17MODFIFO_H_
