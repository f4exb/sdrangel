// This file is part of LeanSDR Copyright (C) 2016-2018 <pabr@pabr.org>.
// See the toplevel README for more information.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef LEANSDR_IESS_H
#define LEANSDR_IESS_H

#include "leansdr/framework.h"

namespace leansdr
{

// SELF-SYNCHRONIZING DESCRAMBLER
// Per ETSI TR 192 figure 8 (except Q20/ not connected to CLOCK).
// This implementation operates on packed bits, MSB first.

struct etr192_descrambler : runnable
{
    etr192_descrambler(
        scheduler *sch,
        pipebuf<u8> &_in, // Packed scrambled bits
        pipebuf<u8> &_out // Packed bits
    ) :
        runnable(sch, "etr192_dec"),
        in(_in),
        out(_out),
        shiftreg(0),
        counter(0)
    {
    }

    void run()
    {
        int count = min(in.readable(), out.writable());

        for (u8 *pin = in.rd(), *pend = pin + count, *pout = out.wr();
             pin < pend; ++pin, ++pout)
        {
            u8 byte_in = *pin, byte_out = 0;

            for (int b = 8; b--; byte_in <<= 1)
            {
                // Levels before clock transition
                int bit_in = (byte_in & 128) ? 1 : 0;
                int reset_counter = (shiftreg ^ (shiftreg >> 8)) & 1;
                int counter_overflow = (counter == 31) ? 1 : 0;
                int taps = (shiftreg >> 2) ^ (shiftreg >> 19);
                int bit_out = (taps ^ counter_overflow ^ bit_in ^ 1) & 1;
                // Execute clock transition
#if 1 // Descramble
                shiftreg = (shiftreg << 1) | bit_in;
#else // Scramble
                shiftreg = (shiftreg << 1) | bit_out;
#endif
                counter = reset_counter ? 0 : (counter + 1) & 31;
                byte_out = (byte_out << 1) | bit_out;
            }

            *pout = byte_out;
        }

        in.read(count);
        out.written(count);
    }

  private:
    pipereader<u8> in;
    pipewriter<u8> out;
    u32 shiftreg; // 20 bits
    u8 counter;   // 5 bits
};                // etr192_descrambler

} // namespace leansdr

#endif // LEANSDR_IESS_H
