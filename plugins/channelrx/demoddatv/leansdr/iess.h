#ifndef LEANSDR_IESS_H
#define LEANSDR_IESS_H

#include "leansdr/framework.h"

namespace leansdr {

  // SELF-SYNCHRONIZING DESCRAMBLER
  // Per ETSI TR 192 figure 8 (except Q20/ not connected to CLOCK).
  // This implementation operates on packed bits, MSB first.

  struct etr192_descrambler : runnable {
    etr192_descrambler(scheduler *sch,
		       pipebuf<u8> &_in,   // Packed scrambled bits
		       pipebuf<u8> &_out)  // Packed bits 
      : runnable(sch, "etr192_dec"),
	in(_in), out(_out),
	shiftreg(0), counter(0)
    {
    }

    void run() {
      int count = min(in.readable(), out.writable());
      for ( u8 *pin=in.rd(), *pend=pin+count, *pout=out.wr();
	    pin<pend; ++pin,++pout) {
	u8 byte_in=*pin, byte_out=0;
	for ( int b=8; b--; byte_in<<=1 ) {
	  // Levels before clock transition
	  int bit_in = (byte_in&128) ? 1 : 0;
	  int reset_counter = (shiftreg ^ (shiftreg>>8)) & 1;
	  int counter_overflow = (counter==31) ? 1 : 0;
	  int taps = (shiftreg>>2) ^ (shiftreg>>19);
	  int bit_out = (taps ^ counter_overflow ^ bit_in ^ 1) & 1;
	  // Execute clock transition
#if 1  // Descramble
	  shiftreg = (shiftreg<<1) | bit_in;
#else  // Scramble
	  shiftreg = (shiftreg<<1) | bit_out;
#endif
	  counter = reset_counter ? 0 : (counter+1)&31;
	  byte_out = (byte_out<<1) | bit_out;
	}
	*pout = byte_out;
      }
      in.read(count);
      out.written(count);
    }

  private:
    pipereader<u8> in;
    pipewriter<u8> out;
    u32 shiftreg;  // 20 bits
    u8 counter;    // 5 bits
  };  // etr192_descrambler

}  // namespace

#endif  // LEANSDR_IESS_H
