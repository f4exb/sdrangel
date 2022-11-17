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

#ifndef LEANSDR_GENERIC_H
#define LEANSDR_GENERIC_H

#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>

#ifdef _MSC_VER
#include <stdlib.h>
#include <io.h>
#else
#include <unistd.h>
#endif

#include "leansdr/math.h"

namespace leansdr
{

//////////////////////////////////////////////////////////////////////
// Simple blocks
//////////////////////////////////////////////////////////////////////

/* // Commented out, as not needed, as does compile on Windows

// [file_reader] reads raw data from a file descriptor into a [pipebuf].
// If the file descriptor is seekable, data can be looped.

template <typename T>
struct file_reader : runnable
{
    bool loop;

    file_reader(
        scheduler *sch,
        int _fdin,
        pipebuf<T> &_out
    ) :
        runnable(sch, _out.name),
        loop(false),
        filler(nullptr),
        fdin(_fdin),
        out(_out)
    {
    }

    ~file_reader()
    {
        delete[] filler;
    }

    void run()
    {
        size_t size = out.writable() * sizeof(T);

        if (!size) {
            return;
        }

    again:
        ssize_t nr = read(fdin, out.wr(), size);

        if (nr < 0 && errno == EWOULDBLOCK)
        {
            if (filler)
            {
                if (sch->debug) {
                    fprintf(stderr, "U");
                }

                out.write(*filler);
            }

            return;
        }

        if (nr < 0) {
            fatal("read(file_reader)");
        }

        if (!nr)
        {
            if (!loop) {
                return;
            }

            if (sch->debug) {
                fprintf(stderr, "%s looping\n", name);
            }

            off_t res = lseek(fdin, 0, SEEK_SET);

            if (res == (off_t)-1) {
                fatal("lseek");
            }

            goto again;
        }

        // Always stop at element boundary (may block)
        size_t partial = nr % sizeof(T);
        size_t remain = partial ? sizeof(T) - partial : 0;

        while (remain)
        {
            if (sch->debug) {
                fprintf(stderr, "+");
            }

            ssize_t nr2 = read(fdin, (char *)out.wr() + nr, remain);

            if (nr2 <= 0) {
                fatal("partial read");
            }

            nr += nr2;
            remain -= nr2;
        }

        out.written(nr / sizeof(T));
    }

    void set_realtime(T &_filler)
    {
        int flags = fcntl(fdin, F_GETFL);

        if (fcntl(fdin, F_SETFL, flags | O_NONBLOCK)) {
            fatal("fcntl");
        }

        filler = new T(_filler);
    }

  private:
    T *filler;
    int fdin;
    pipewriter<T> out;
};

*/

// [file_writer] writes raw data from a [pipebuf] to a file descriptor.

template <typename T>
struct file_writer : runnable
{
    file_writer(
        scheduler *sch,
        pipebuf<T> &_in,
        int _fdout
    ) :
        runnable(sch, _in.name),
        in(_in),
        fdout(_fdout)
    {
    }

    void run()
    {
        int size = in.readable() * sizeof(T);

        if (!size) {
            return;
        }

        int nw = write(fdout, in.rd(), size);

        if (!nw) {
            fatal("pipe");
        }

        if (nw < 0) {
            fatal("write");
        }

        if (nw % sizeof(T)) {
            fatal("partial write");
        }

        in.read(nw / sizeof(T));
    }

  private:
    pipereader<T> in;
    int fdout;
};

// [file_printer] writes data from a [pipebuf] to a file descriptor,
// with printf-style formatting and optional scaling.

template <typename T>
struct file_printer : runnable
{
    file_printer(
        scheduler *sch,
        const char *_format,
        pipebuf<T> &_in,
        int _fdout,
        int _decimation = 1
    ) :
        runnable(sch, _in.name),
        scale(1),
        decimation(_decimation),
        in(_in),
        format(_format),
        fdout(_fdout),
        phase(0)
    {
    }

    void run()
    {
        int n = in.readable();
        T *pin = in.rd(), *pend = pin + n;

        for (; pin < pend; ++pin)
        {
            if (++phase >= decimation)
            {
                phase -= decimation;
                char buf[256];
                int len = snprintf(buf, sizeof(buf), format, (*pin) * scale);

                if (len < 0) {
                    fatal("obsolete glibc");
                }

                int nw = write(fdout, buf, len);

                if (nw != len) {
                    fatal("partial write");
                }
            }
        }

        in.read(n);
    }

    T scale;
    int decimation;

  private:
    pipereader<T> in;
    const char *format;
    int fdout;
    int phase;
};

// [file_carrayprinter] writes all data available from a [pipebuf]
// to a file descriptor on a single line.
// Special case for std::complex.

template <typename T>
struct file_carrayprinter : runnable
{
    file_carrayprinter(
        scheduler *sch,
        const char *_head,
        const char *_format,
        const char *_sep,
        const char *_tail,
        pipebuf<std::complex<T>> &_in,
        int _fdout
    ) :
        runnable(sch, _in.name),
        scale(1),
        fixed_size(0),
        in(_in),
        head(_head),
        format(_format),
        sep(_sep),
        tail(_tail),
        fout(fdopen(_fdout, "w"))
    {
    }

    void run()
    {
        int n, nmin = fixed_size ? fixed_size : 1;

        while ((n = in.readable()) >= nmin)
        {
            if (fixed_size) {
                n = fixed_size;
            }

            if (fout)
            {
                fprintf(fout, head, n);
                std::complex<T> *pin = in.rd();

                for (int i = 0; i < n; ++i)
                {
                    if (i) {
                        fprintf(fout, "%s", sep);
                    }

                    fprintf(fout, format, pin[i].real() * scale, pin[i].imag() * scale);
                }

                fprintf(fout, "%s", tail);
            }

            fflush(fout);
            in.read(n);
        }
    }

    T scale;
    int fixed_size; // Number of elements per batch, or 0.

  private:
    pipereader<std::complex<T>> in;
    const char *head, *format, *sep, *tail;
    FILE *fout;
};

template <typename T, int N>
struct file_vectorprinter : runnable
{
    file_vectorprinter(
        scheduler *sch,
        const char *_head,
        const char *_format,
        const char *_sep,
        const char *_tail,
        pipebuf<T[N]> &_in,
        int _fdout,
        int _n = N
    ) :
        runnable(sch, _in.name),
        scale(1),
        in(_in),
        head(_head),
        format(_format),
        sep(_sep),
        tail(_tail),
        n(_n)
    {
        fout = fdopen(_fdout, "w");

        if (!fout) {
            fatal("fdopen");
        }
    }

    void run()
    {
        while (in.readable() >= 1)
        {
            fprintf(fout, head, n);
            T(*pin)
            [N] = in.rd();

            for (int i = 0; i < n; ++i)
            {
                if (i) {
                    fprintf(fout, "%s", sep);
                }

                fprintf(fout, format, (*pin)[i] * scale);
            }

            fprintf(fout, "%s", tail);
            in.read(1);
        }

        fflush(fout);
    }

    T scale;

  private:
    pipereader<T[N]> in;
    const char *head, *format, *sep, *tail;
    FILE *fout;
    int n;
};

// [itemcounter] writes the number of input items to the output [pipebuf].
// [Tout] must be a numeric type.

template <typename Tin, typename Tout>
struct itemcounter : runnable
{
    itemcounter(
        scheduler *sch,
        pipebuf<Tin> &_in,
        pipebuf<Tout> &_out
    ) :
        runnable(sch, "itemcounter"),
        in(_in),
        out(_out)
    {
    }

    void run()
    {
        if (out.writable() < 1) {
            return;
        }

        unsigned long count = in.readable();

        if (!count) {
            return;
        }

        out.write(count);
        in.read(count);
    }

  private:
    pipereader<Tin> in;
    pipewriter<Tout> out;
};

// [decimator] forwards 1 in N sample.

template <typename T>
struct decimator : runnable
{
    int d;

    decimator(
        scheduler *sch,
        int _d,
        pipebuf<T> &_in,
        pipebuf<T> &_out
    ) :
        runnable(sch, "decimator"),
        d(_d),
        in(_in),
        out(_out)
    {
    }

    void run()
    {
        long count = min(in.readable() / d, out.writable());
        T *pin = in.rd(), *pend = pin + count * d, *pout = out.wr();

        for (; pin < pend; pin += d, ++pout) {
            *pout = *pin;
        }

        in.read(count * d);
        out.written(count);
    }

  private:
    pipereader<T> in;
    pipewriter<T> out;
};

// [rate_estimator] accumulates counts of two quantities
// and periodically outputs their ratio.

template <typename T>
struct rate_estimator : runnable
{
    int sample_size;

    rate_estimator(
        scheduler *sch,
        pipebuf<int> &_num,
        pipebuf<int> &_den,
        pipebuf<float> &_rate
    ) :
        runnable(sch, "rate_estimator"),
        sample_size(10000),
        num(_num),
        den(_den),
        rate(_rate),
        acc_num(0),
        acc_den(0)
    {
    }

    void run()
    {
        if (rate.writable() < 1) {
            return;
        }

        int count = min(num.readable(), den.readable());
        int *pnum = num.rd(), *pden = den.rd();

        for (int n = count; n--; ++pnum, ++pden)
        {
            acc_num += *pnum;
            acc_den += *pden;
        }

        num.read(count);
        den.read(count);

        if (acc_den >= sample_size)
        {
            rate.write((float)acc_num / acc_den);
            acc_num = acc_den = 0;
        }
    }

  private:
    pipereader<int> num, den;
    pipewriter<float> rate;
    T acc_num, acc_den;
};

// SERIALIZER

template <typename Tin, typename Tout>
struct serializer : runnable
{
    serializer(
        scheduler *sch,
        pipebuf<Tin> &_in,
        pipebuf<Tout> &_out
    ) :
        nin(max((size_t)1,
        sizeof(Tin) / sizeof(Tout))),
        nout(max((size_t)1,
        sizeof(Tout) / sizeof(Tin))),
        in(_in),
        out(_out, nout)
    {
        if (nin * sizeof(Tin) != nout * sizeof(Tout)) {
            fail("serializer: incompatible sizes");
        }
    }

    void run()
    {
        while (in.readable() >= nin && out.writable() >= nout)
        {
            memcpy(out.wr(), in.rd(), nout * sizeof(Tout));
            in.read(nin);
            out.written(nout);
        }
    }

  private:
    int nin, nout;
    pipereader<Tin> in;
    pipewriter<Tout> out;
}; // serializer

// [buffer_reader] reads from a user-supplied buffer.

template <typename T>
struct buffer_reader : runnable
{
    buffer_reader(
        scheduler *sch,
        T *_data,
        int _count,
        pipebuf<T> &_out
    ) :
        runnable(sch, "buffer_reader"),
        data(_data),
        count(_count),
        out(_out),
        pos(0)
    {
    }

    void run()
    {
        int n = min(out.writable(), (unsigned long)(count - pos));
        memcpy(out.wr(), &data[pos], n * sizeof(T));
        pos += n;
        out.written(n);
    }

  private:
    T *data;
    int count;
    pipewriter<T> out;
    int pos;
}; // buffer_reader

// [buffer_writer] writes to a user-supplied buffer.

template <typename T>
struct buffer_writer : runnable
{
    buffer_writer(
        scheduler *sch,
        pipebuf<T> &_in,
        T *_data,
        int _count
    ) :
        runnable(sch, "buffer_reader"),
        in(_in),
        data(_data),
        count(_count),
        pos(0)
    {
    }

    void run()
    {
        int n = min(in.readable(), (unsigned long)(count - pos));
        memcpy(&data[pos], in.rd(), n * sizeof(T));
        in.read(n);
        pos += n;
    }

  private:
    pipereader<T> in;
    T *data;
    int count;
    int pos;
}; // buffer_writer

} // namespace leansdr

#endif // LEANSDR_GENERIC_H
