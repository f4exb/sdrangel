/*
 * rtpendian.h
 *
 *  Created on: Feb 27, 2018
 *      Author: f4exb
 */

#ifndef QRTPLIB_RTPENDIAN_H_
#define QRTPLIB_RTPENDIAN_H_

#include <QtEndian>

namespace qrtplib
{

class RTPEndian
{
public:
    RTPEndian()
    {
        uint32_t endianTest32 = 1;
        uint8_t *ptr = (uint8_t*) &endianTest32;
        m_isLittleEndian = (*ptr == 1);
    }

    template<typename T>
    T qToHost(const T& x) const
    {
        return m_isLittleEndian ? qToLittleEndian(x) : qToBigEndian(x);
    }

private:
    bool m_isLittleEndian;
};

}

#endif /* QRTPLIB_RTPENDIAN_H_ */
