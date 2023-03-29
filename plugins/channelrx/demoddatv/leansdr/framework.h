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

#ifndef LEANSDR_FRAMEWORK_H
#define LEANSDR_FRAMEWORK_H

#include <cstddef>
#include <algorithm>

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef VERSION
#define VERSION "undefined"
#endif

namespace leansdr
{

void fatal(const char *s);
void fail(const char *s);

//////////////////////////////////////////////////////////////////////
// DSP framework
//////////////////////////////////////////////////////////////////////

// [pipebuf] is a FIFO buffer with multiple readers.
// [pipewriter] is a client-side hook for writing into a [pipebuf].
// [pipereader] is a client-side hook reading from a [pipebuf].
// [runnable] is anything that moves data between [pipebufs].
// [scheduler] is a global context which invokes [runnables] until fixpoint.

static const int MAX_PIPES = 64;
static const int MAX_RUNNABLES = 64;
static const int MAX_READERS = 8;

struct pipebuf_common
{
    virtual int sizeofT() {
        return 0;
    }

    virtual long long hash() {
        return 0;
    }

    virtual void dump(std::size_t *total_bufs) {
        (void)total_bufs;
    }

    const char *name;

    pipebuf_common(const char *_name) : name(_name) {
    }

    virtual ~pipebuf_common() {
    }
};

struct runnable_common
{
    const char *name;

    runnable_common(const char *_name) : name(_name) {
    }

    virtual ~runnable_common() {
    }

    virtual void run() {
    }

    virtual void shutdown() {
    }

#ifdef DEBUG
    ~runnable_common()
    {
        fprintf(stderr, "Deallocating %s !\n", name);
    }
#endif
};

struct window_placement
{
    const char *name; // nullptr to terminate
    int x, y, w, h;
};

struct scheduler
{
    pipebuf_common *pipes[MAX_PIPES];
    int npipes;
    runnable_common *runnables[MAX_RUNNABLES];
    int nrunnables;
    window_placement *windows;
    bool verbose, debug, debug2;

    scheduler() :
        npipes(0),
        nrunnables(0),
        windows(nullptr),
        verbose(false),
        debug(false),
        debug2(false)
    {
    }

    void add_pipe(pipebuf_common *p)
    {
        if (npipes == MAX_PIPES) {
            fail("MAX_PIPES");
        }

        pipes[npipes++] = p;
    }

    void add_runnable(runnable_common *r)
    {
        if (nrunnables == MAX_RUNNABLES) {
            fail("MAX_RUNNABLES");
        }

        runnables[nrunnables++] = r;
    }

    void step()
    {
        for (int i = 0; i < nrunnables; ++i) {
            runnables[i]->run();
        }
    }

    void run()
    {
        unsigned long long prev_hash = 0;

        while (1)
        {
            step();
            unsigned long long h = hash();

            if (h == prev_hash) {
                break;
            }

            prev_hash = h;
        }
    }

    void shutdown()
    {
        for (int i = 0; i < nrunnables; ++i) {
            runnables[i]->shutdown();
        }
    }

    unsigned long long hash()
    {
        unsigned long long h = 0;

        for (int i = 0; i < npipes; ++i) {
            h += (1 + i) * pipes[i]->hash();
        }

        return h;
    }

    void dump()
    {
        fprintf(stderr, "\n");
        std::size_t total_bufs = 0;

        for (int i = 0; i < npipes; ++i) {
            pipes[i]->dump(&total_bufs);
        }

        fprintf(stderr, "Total buffer memory: %ld KiB\n",
                (unsigned long)total_bufs / 1024);
    }
};

struct runnable : runnable_common
{
    runnable(
        scheduler *_sch,
        const char *name
    ) :
        runnable_common(name),
        sch(_sch)
    {
        sch->add_runnable(this);
    }

  protected:
    scheduler *sch;
};

template <typename T>
struct pipebuf : pipebuf_common
{
    T *buf;
    T *rds[MAX_READERS];
    int nrd;
    T *wr;
    T *end;

    pipebuf(
        scheduler *sch,
        const char *name,
        unsigned long size
    ) :
        pipebuf_common(name),
        nrd(0),
        min_write(1),
        total_written(0),
        total_read(0)
    {
        buf = new T[size];
        wr = buf;
        end = buf + size;
        sch->add_pipe(this);
    }

    ~pipebuf()
    {
        delete[] buf;
    }

    int sizeofT() {
        return sizeof(T);
    }

    int add_reader()
    {
        if (nrd == MAX_READERS) {
            fail("too many readers");
        }

        rds[nrd] = wr;
        return nrd++;
    }

    void pack()
    {
        T *rd = wr;

        for (int i = 0; i < nrd; ++i)
        {
            if (rds[i] < rd) {
                rd = rds[i];
            }
        }

        memmove(buf, rd, (wr - rd) * sizeof(T));
        wr -= rd - buf;

        for (int i = 0; i < nrd; ++i) {
            rds[i] -= rd - buf;
        }
    }

    long long hash()
    {
        return total_written + total_read;
    }

    void dump(std::size_t *total_bufs)
    {
        if (total_written < 10000)
        {
            fprintf(stderr, ".%-16s : %4ld/%4ld", name, total_read,
                    total_written);
        }
        else if (total_written < 1000000)
        {
            fprintf(stderr, ".%-16s : %3ldk/%3ldk", name, total_read / 1000,
                    total_written / 1000);
        }
        else
        {
            fprintf(stderr, ".%-16s : %3ldM/%3ldM", name, total_read / 1000000,
                    total_written / 1000000);
        }

        *total_bufs += (end - buf) * sizeof(T);
        unsigned long nw = end - wr;
        fprintf(stderr, " %6ld writable %c,", nw, (nw < min_write) ? '!' : ' ');
        T *rd = wr;

        for (int j = 0; j < nrd; ++j)
        {
            if (rds[j] < rd) {
                rd = rds[j];
            }
        }

        fprintf(stderr, " %6d unread (", (int)(wr - rd));

        for (int j = 0; j < nrd; ++j) {
            fprintf(stderr, " %d", (int)(wr - rds[j]));
        }

        fprintf(stderr, " )\n");
    }

    unsigned long min_write;
    unsigned long total_written, total_read;
#ifdef DEBUG
    ~pipebuf()
    {
        fprintf(stderr, "Deallocating %s !\n", name);
    }
#endif
};

template <typename T>
struct pipewriter
{
    pipebuf<T> &buf;

    pipewriter(pipebuf<T> &_buf, unsigned long min_write = 1) : buf(_buf)
    {
        if (min_write > buf.min_write) {
            buf.min_write = min_write;
        }
    }
    // Return number of items writable at this->wr, 0 if full.
    long writable()
    {
        if (buf.end < buf.min_write + buf.wr) {
            buf.pack();
        }

        return buf.end - buf.wr;
    }

    T *wr() {
        return buf.wr;
    }

    void written(unsigned long n)
    {
        if (buf.wr + n > buf.end) {
            fprintf(stderr, "pipewriter::written: bug: overflow to %s\n", buf.name);
        }

        buf.wr += n;
        buf.total_written += n;
    }

    void write(const T &e)
    {
        *wr() = e;
        written(1);
    }

    void reset(unsigned long min_write = 1)
    {
        if (min_write > buf.min_write) {
            buf.min_write = min_write;
        }
    }
};

// Convenience functions for working with optional pipes

template <typename T>
pipewriter<T> *opt_writer(pipebuf<T> *buf, unsigned long min_write = 1)
{
    return buf ? new pipewriter<T>(*buf, min_write) : nullptr;
}

template <typename T>
bool opt_writable(pipewriter<T> *p, int n = 1)
{
    return (p == nullptr) || p->writable() >= n;
}

template <typename T>
void opt_write(pipewriter<T> *p, T val)
{
    if (p) {
        p->write(val);
    }
}

template <typename T>
struct pipereader
{
    pipebuf<T> &buf;
    int id;

    pipereader(pipebuf<T> &_buf) : buf(_buf), id(_buf.add_reader())
    {}

    long readable() {
        return buf.wr - buf.rds[id];
    }

    T *rd() {
        return buf.rds[id];
    }

    void read(unsigned long n)
    {
        if (buf.rds[id] + n > buf.wr)
        {
            fprintf(stderr, "Bug: underflow from %s\n", buf.name);
        }

        buf.rds[id] += n;
        buf.total_read += n;
    }
};

// Math functions for templates

template <typename T>
T gen_sqrt(T x);
inline float gen_sqrt(float x) {
    return sqrtf(x);
}

inline unsigned int gen_sqrt(unsigned int x) {
    return sqrtl(x);
}

inline long double gen_sqrt(long double x) {
    return sqrtl(x);
}

template <typename T>
T gen_abs(T x);
inline float gen_abs(float x) {
    return fabsf(x);
}

inline int gen_abs(int x) {
    return abs(x);
}

inline long int gen_abs(long int x) {
    return labs(x);
}

template <typename T>
T gen_hypot(T x, T y);
inline float gen_hypot(float x, float y) {
    return hypotf(x, y);
}

inline long double gen_hypot(long double x, long double y) {
    return hypotl(x, y);
}

template <typename T>
T gen_atan2(T y, T x);
inline float gen_atan2(float y, float x) {
    return atan2f(y, x);
}

inline long double gen_atan2(long double y, long double x) {
    return atan2l(y, x);
}

template <typename T>
T min(const T &x, const T &y) {
    return (x < y) ? x : y;
}

template <typename T>
T max(const T &x, const T &y) {
    return (x < y) ? y : x;
}

// Abreviations for integer types

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;
typedef signed char s8;
typedef signed short s16;
typedef signed long s32;

} // namespace leansdr

#endif // LEANSDR_FRAMEWORK_H
