///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026                                                           //
//                                                                               //
// Experimental FT4 decoder scaffold derived from FT8 decoder architecture.      //
///////////////////////////////////////////////////////////////////////////////////
#ifndef ft4_h
#define ft4_h

#include <vector>

#include <QObject>

#include "ft8.h"
#include "export.h"

class QThread;

namespace FT8 {

class FT8_API FT4Params
{
public:
    int nthreads;
    int ldpc_iters;
    int use_osd;
    int osd_depth;
    int osd_ldpc_thresh;
    int max_candidates;

    FT4Params() :
        nthreads(8),
        ldpc_iters(25),
        use_osd(1),
        osd_depth(0),
        osd_ldpc_thresh(70),
        max_candidates(96)
    {}
};

class FT8_API FT4Decoder : public QObject
{
    Q_OBJECT
public:
    ~FT4Decoder();
    void entry(
        float xsamples[],
        int nsamples,
        int start,
        int rate,
        float min_hz,
        float max_hz,
        int hints1[],
        int hints2[],
        double time_left,
        double total_time_left,
        CallbackInterface *cb,
        int,
        struct cdecode *
    );
    void wait(double time_left);
    void forceQuit();
    FT4Params& getParams() { return params; }

private:
    FT4Params params;
    std::vector<QThread*> threads;
};

} // namespace FT8

#endif
