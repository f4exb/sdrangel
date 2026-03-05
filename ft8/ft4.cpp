///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026                                                           //
//                                                                               //
// Experimental FT4 decoder scaffold derived from FT8 decoder architecture.      //
///////////////////////////////////////////////////////////////////////////////////

#include <array>
#include <cmath>
#include <algorithm>

#include <QThread>
#include <QDebug>

#include "ft4worker.h"
#include "fft.h"

namespace FT8 {

FT4Decoder::~FT4Decoder()
{
    forceQuit();
}

void FT4Decoder::entry(
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
    int nprevdecs,
    struct cdecode *xprevdecs
)
{
    (void) hints1;
    (void) hints2;
    (void) time_left;
    (void) total_time_left;
    (void) nprevdecs;
    (void) xprevdecs;

    std::vector<float> samples(nsamples);

    for (int i = 0; i < nsamples; i++) {
        samples[i] = xsamples[i];
    }

    if (min_hz < 0) {
        min_hz = 0;
    }

    if (max_hz > rate / 2) {
        max_hz = rate / 2;
    }

    const float per = (max_hz - min_hz) / params.nthreads;

    for (int i = 0; i < params.nthreads; i++)
    {
        float hz0 = min_hz + i * per;
        float hz1 = min_hz + (i + 1) * per;
        hz0 = std::max(hz0, 0.0f);
        hz1 = std::min(hz1, (rate / 2.0f) - 50.0f);

        FT4Worker *ft4 = new FT4Worker(samples, hz0, hz1, start, rate, cb, params);
        QThread *th = new QThread();
        th->setObjectName(QString("ft4:%1:%2").arg(cb->get_name()).arg(i));
        threads.push_back(th);
        ft4->moveToThread(th);
        QObject::connect(th, &QThread::started, ft4, [ft4, th]() {
            ft4->start_work();
            th->quit();
        });
        QObject::connect(th, &QThread::finished, ft4, &QObject::deleteLater);
        QObject::connect(th, &QThread::finished, th, &QObject::deleteLater);
        th->start();
    }
}

void FT4Decoder::wait(double time_left)
{
    unsigned long thread_timeout = time_left * 1000;

    while (!threads.empty())
    {
        bool success = threads.front()->wait(thread_timeout);

        if (!success)
        {
            qDebug("FT8::FT4Decoder::wait: thread timed out");
            thread_timeout = 50;
        }

        threads.erase(threads.begin());
    }
}

void FT4Decoder::forceQuit()
{
    while (!threads.empty())
    {
        threads.front()->quit();
        threads.front()->wait();
        threads.erase(threads.begin());
    }
}

} // namespace FT8
