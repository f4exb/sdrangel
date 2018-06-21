#ifndef LEANSDR_HDLC_H
#define LEANSDR_HDLC_H

#include "leansdr/framework.h"

namespace leansdr
{

// HDLC deframer

struct hdlc_dec
{

    hdlc_dec(int _minframesize,  // Including CRC, excluding HDLC flags.
            int _maxframesize, bool _invert) :
            minframesize(_minframesize),
            maxframesize(_maxframesize),
            invertmask(_invert ? 0xff : 0),
            framebuf(new u8[maxframesize]),
            debug(false)
    {
        byte_out = 0;
        nbits_out = 0;
        framesize = 0;
        crc16 = 0;

        reset();
    }

    void reset()
    {
        shiftreg = 0;
        inframe = false;
    }

    void begin_frame()
    {
        framesize = 0;
        crc16 = crc16_init;
    }

    // Decode (*ppin)[count] as MSB-packed HDLC bitstream.
    // Return pointer to buffer[*pdatasize], or NULL if no valid frame.
    // Return number of discarded bytes in *discarded.
    // Return number of checksum errors in *fcs_errors.
    // *ppin will have increased by at least 1 (unless count==0).

    u8 *decode(u8 **ppin, int count, int *pdatasize, int *hdlc_errors,
            int *fcs_errors)
    {
        *hdlc_errors = 0;
        *fcs_errors = 0;
        *pdatasize = -1;
        u8 *pin = *ppin, *pend = pin + count;
        for (; pin < pend; ++pin)
        {
            u8 byte_in = (*pin) ^ invertmask;
            for (int bits = 8; bits--; byte_in <<= 1)
            {
                u8 bit_in = byte_in & 128;
                shiftreg = (shiftreg >> 1) | bit_in;
                if (!inframe)
                {
                    if (shiftreg == 0x7e)
                    {  // HDLC flag 01111110
                        inframe = true;
                        nbits_out = 0;
                        begin_frame();
                    }
                }
                else
                {
                    if ((shiftreg & 0xfe) == 0x7c)
                    {  // 0111110x HDLC stuffing
                        // Unstuff this 0
                    }
                    else if (shiftreg == 0x7e)
                    {  // 01111110 HDLC flag
                        if (nbits_out != 7)
                        {
                            // Not at byte boundary
                            if (debug)
                                fprintf(stderr, "^");
                            ++*hdlc_errors;
                        }
                        else
                        {
                            // Checksum
                            crc16 ^= 0xffff;
                            if (framesize < 2 || framesize < minframesize
                                    || crc16 != crc16_check)
                            {
                                if (debug)
                                    fprintf(stderr, "!");
                                ++*hdlc_errors;
                                // Do not report random noise as FCS errors
                                if (framesize >= minframesize)
                                    ++*fcs_errors;
                            }
                            else
                            {
                                if (debug)
                                    fprintf(stderr, "_");
                                // This will trigger output, but we finish the byte first.
                                *pdatasize = framesize - 2;
                            }
                        }
                        nbits_out = 0;
                        begin_frame();
                        // Keep processing up to 7 remaining bits from byte_in.
                        // Special cases 0111111 and 1111111 cannot affect *pdatasize.
                    }
                    else if (shiftreg == 0xfe)
                    {  // 11111110 HDLC invalid
                        if (framesize)
                        {
                            if (debug)
                                fprintf(stderr, "^");
                            ++*hdlc_errors;
                        }
                        inframe = false;
                    }
                    else
                    {  // Data bit
                        byte_out = (byte_out >> 1) | bit_in; // HDLC is LSB first
                        ++nbits_out;
                        if (nbits_out == 8)
                        {
                            if (framesize < maxframesize)
                            {
                                framebuf[framesize++] = byte_out;
                                crc16_byte(byte_out);
                            }
                            nbits_out = 0;
                        }
                    }
                }  // inframe
            }  // bits
            if (*pdatasize != -1)
            {
                // Found a complete frame
                *ppin = pin + 1;
                return framebuf;
            }
        }
        *ppin = pin;
        return NULL;
    }

private:
    // Config
    int minframesize, maxframesize;
    u8 invertmask;
    u8 *framebuf;   // [maxframesize]
    // State
    u8 shiftreg;    // Input bitstream
    bool inframe;   // Currently receiving a frame ?
    u8 byte_out;    // Accumulator for data bits
    int nbits_out;  // Number of data bits in byte_out
    int framesize;  // Number of bytes in framebuf, if inframe
    u16 crc16;      // CRC of framebuf[framesize]
    // CRC
    static const u16 crc16_init = 0xffff;
    static const u16 crc16_poly = 0x8408;  // 0x1021 MSB-first
    static const u16 crc16_check = 0x0f47;
    void crc16_byte(u8 data)
    {
        crc16 ^= data;
        for (int bit = 8; bit--;)
            crc16 = (crc16 & 1) ? (crc16 >> 1) ^ crc16_poly : (crc16 >> 1);
    }

public:
    bool debug;
};
// hdlc_dec

// HDLC synchronizer with polarity detection

struct hdlc_sync: runnable
{
    hdlc_sync(scheduler *sch,
            pipebuf<u8> &_in,   // Packed bits
            pipebuf<u8> &_out,  // Bytes
            int _minframesize,  // Including CRC, excluding HDLC flags.
            int _maxframesize,
            // Status
            pipebuf<int> *_lock_out = NULL,
            pipebuf<int> *_framecount_out = NULL,
            pipebuf<int> *_fcserrcount_out = NULL,
            pipebuf<int> *_hdlcbytecount_out = NULL,
            pipebuf<int> *_databytecount_out = NULL) :
            runnable(sch, "hdlc_sync"), minframesize(_minframesize), maxframesize(
                    _maxframesize), chunk_size(maxframesize + 2), in(_in), out(
                    _out, _maxframesize + chunk_size), lock_out(
                    opt_writer(_lock_out)), framecount_out(
                    opt_writer(_framecount_out)), fcserrcount_out(
                    opt_writer(_fcserrcount_out)), hdlcbytecount_out(
                    opt_writer(_hdlcbytecount_out)), databytecount_out(
                    opt_writer(_databytecount_out)), cur_sync(0), resync_phase(
                    0), lock_state(false), resync_period(32), header16(false)
    {
        for (int s = 0; s < NSYNCS; ++s)
        {
            syncs[s].dec = new hdlc_dec(minframesize, maxframesize, s != 0);
            for (int h = 0; h < NERRHIST; ++h)
                syncs[s].errhist[h] = 0;
        }
        syncs[cur_sync].dec->debug = sch->debug;
        errslot = 0;
    }

    void run()
    {
        if (!opt_writable(lock_out) || !opt_writable(framecount_out)
                || !opt_writable(fcserrcount_out)
                || !opt_writable(hdlcbytecount_out)
                || !opt_writable(databytecount_out))
            return;

        bool previous_lock_state = lock_state;
        int fcserrcount = 0, framecount = 0;
        int hdlcbytecount = 0, databytecount = 0;

        // Note: hdlc_dec may already hold one frame ready for output.
        while ((long) in.readable() >= chunk_size
                && (long) out.writable() >= maxframesize + chunk_size)
        {
            if (!resync_phase)
            {
                // Once every resync_phase, try all decoders
                for (int s = 0; s < NSYNCS; ++s)
                {
                    if (s != cur_sync)
                        syncs[s].dec->reset();
                    syncs[s].errhist[errslot] = 0;
                    for (u8 *pin = in.rd(), *pend = pin + chunk_size;
                            pin < pend;)
                    {
                        int datasize, hdlc_errors, fcs_errors;
                        u8 *f = syncs[s].dec->decode(&pin, pend - pin,
                                &datasize, &hdlc_errors, &fcs_errors);
                        syncs[s].errhist[errslot] += hdlc_errors;
                        if (s == cur_sync)
                        {
                            if (f)
                            {
                                lock_state = true;
                                output_frame(f, datasize);
                                databytecount += datasize;
                                ++framecount;
                            }
                            fcserrcount += fcs_errors;
                            framecount += fcs_errors;
                        }
                    }
                }
                errslot = (errslot + 1) % NERRHIST;
                // Switch to another sync option ?
                // Compare total error counts over about NERRHIST frames.
                int total_errors[NSYNCS];
                for (int s = 0; s < NSYNCS; ++s)
                {
                    total_errors[s] = 0;
                    for (int h = 0; h < NERRHIST; ++h)
                        total_errors[s] += syncs[s].errhist[h];
                }
                int best = cur_sync;
                for (int s = 0; s < NSYNCS; ++s)
                    if (total_errors[s] < total_errors[best])
                        best = s;
                if (best != cur_sync)
                {
                    lock_state = false;
                    if (sch->debug)
                        fprintf(stderr, "[%d:%d->%d:%d]", cur_sync,
                                total_errors[cur_sync], best,
                                total_errors[best]);
                    // No verbose messages on candidate syncs
                    syncs[cur_sync].dec->debug = false;
                    cur_sync = best;
                    syncs[cur_sync].dec->debug = sch->debug;
                }
            }
            else
            {
                // Use only the currently selected decoder
                for (u8 *pin = in.rd(), *pend = pin + chunk_size; pin < pend;)
                {
                    int datasize, hdlc_errors, fcs_errors;
                    u8 *f = syncs[cur_sync].dec->decode(&pin, pend - pin,
                            &datasize, &hdlc_errors, &fcs_errors);
                    if (f)
                    {
                        lock_state = true;
                        output_frame(f, datasize);
                        databytecount += datasize;
                        ++framecount;
                    }
                    fcserrcount += fcs_errors;
                    framecount += fcs_errors;
                }
            }  // resync_phase
            in.read(chunk_size);
            hdlcbytecount += chunk_size;
            if (++resync_phase >= resync_period)
                resync_phase = 0;
        }  // Work to do

        if (lock_state != previous_lock_state)
            opt_write(lock_out, lock_state ? 1 : 0);
        opt_write(framecount_out, framecount);
        opt_write(fcserrcount_out, fcserrcount);
        opt_write(hdlcbytecount_out, hdlcbytecount);
        opt_write(databytecount_out, databytecount);
    }

private:
    void output_frame(u8 *f, int size)
    {
        if (header16)
        {
            // Removed 16-bit CRC, add 16-bit prefix -> Still <= maxframesize.
            out.write(size >> 8);
            out.write(size & 255);
        }
        memcpy(out.wr(), f, size);
        out.written(size);
        opt_write(framecount_out, 1);
    }

    int minframesize, maxframesize;
    int chunk_size;
    pipereader<u8> in;
    pipewriter<u8> out;
    pipewriter<int> *lock_out;
    pipewriter<int> *framecount_out, *fcserrcount_out;
    pipewriter<int> *hdlcbytecount_out, *databytecount_out;
    static const int NSYNCS = 2;    // Two possible polarities
    static const int NERRHIST = 2;  // Compare error counts over two frames
    struct
    {
        hdlc_dec *dec;
        int errhist[NERRHIST];
    } syncs[NSYNCS];
    int errslot;
    int cur_sync;
    int resync_phase;
    bool lock_state;
public:
    int resync_period;
    bool header16;  // Output length prefix
};
// hdlc_sync

}// namespace

#endif  // LEANSDR_HDLC_H
