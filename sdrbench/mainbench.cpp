///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// Swagger server adapter interface                                              //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include <QDebug>
#include <QElapsedTimer>

#include "mainbench.h"

MainBench *MainBench::m_instance = 0;

MainBench::MainBench(qtwebapp::LoggerWithFile *logger, const ParserBench& parser, QObject *parent) :
    QObject(parent),
    m_logger(logger),
    m_parser(parser),
    m_uniform_distribution_f(-1.0, 1.0),
    m_uniform_distribution_s16(-2048, 2047)
{
    qDebug() << "MainBench::MainBench: start";
    m_instance = this;
    qDebug() << "MainBench::MainBench: end";
}

MainBench::~MainBench()
{}

void MainBench::run()
{
    qDebug() << "MainBench::run: parameters:"
        << " testStr: " << m_parser.getTestStr()
        << " testType: " << (int) m_parser.getTestType()
        << " nsamples: " << m_parser.getNbSamples()
        << " repet: " << m_parser.getRepetition()
        << " log2f: " << m_parser.getLog2Factor();

    if (m_parser.getTestType() == ParserBench::TestDecimatorsII) {
        testDecimateII();
    } else if (m_parser.getTestType() == ParserBench::TestDecimatorsInfII) {
        testDecimateII(ParserBench::TestDecimatorsInfII);
    } else if (m_parser.getTestType() == ParserBench::TestDecimatorsSupII) {
        testDecimateII(ParserBench::TestDecimatorsSupII);
    } else if (m_parser.getTestType() == ParserBench::TestDecimatorsIF) {
        testDecimateIF();
    } else if (m_parser.getTestType() == ParserBench::TestDecimatorsFI) {
        testDecimateFI();
    } else if (m_parser.getTestType() == ParserBench::TestDecimatorsFF) {
        testDecimateFF();
    } else {
        qDebug() << "MainBench::run: unknown test type: " << m_parser.getTestType();
    }

    emit finished();
}

void MainBench::testDecimateII(ParserBench::TestType testType)
{
    QElapsedTimer timer;
    qint64 nsecs = 0;

    qDebug() << "MainBench::testDecimateII: create test data";

    qint16 *buf = new qint16[m_parser.getNbSamples()*2];
    m_convertBuffer.resize(m_parser.getNbSamples()/(1<<m_parser.getLog2Factor()));
    auto my_rand = std::bind(m_uniform_distribution_s16, m_generator);
    std::generate(buf, buf + m_parser.getNbSamples()*2 - 1, my_rand);

    qDebug() << "MainBench::testDecimateII: run test";

    for (uint32_t i = 0; i < m_parser.getRepetition(); i++)
    {
        switch (testType)
        {
        case ParserBench::TestDecimatorsInfII:
            timer.start();
            decimateInfII(buf, m_parser.getNbSamples()*2);
            nsecs += timer.nsecsElapsed();
            break;
        case ParserBench::TestDecimatorsSupII:
            timer.start();
            decimateSupII(buf, m_parser.getNbSamples()*2);
            nsecs += timer.nsecsElapsed();
            break;
        case ParserBench::TestDecimatorsII:
        default:
            timer.start();
            decimateII(buf, m_parser.getNbSamples()*2);
            nsecs += timer.nsecsElapsed();
            break;
        }
    }

    printResults("MainBench::testDecimateII", nsecs);

    qDebug() << "MainBench::testDecimateII: cleanup test data";
    delete[] buf;
}

void MainBench::testDecimateIF()
{
    QElapsedTimer timer;
    qint64 nsecs = 0;

    qDebug() << "MainBench::testDecimateIF: create test data";

    qint16 *buf = new qint16[m_parser.getNbSamples()*2];
    m_convertBufferF.resize(m_parser.getNbSamples()/(1<<m_parser.getLog2Factor()));
    auto my_rand = std::bind(m_uniform_distribution_s16, m_generator);
    std::generate(buf, buf + m_parser.getNbSamples()*2 - 1, my_rand);

    qDebug() << "MainBench::testDecimateIF: run test";

    for (uint32_t i = 0; i < m_parser.getRepetition(); i++)
    {
        timer.start();
        decimateIF(buf, m_parser.getNbSamples()*2);
        nsecs += timer.nsecsElapsed();
    }

    printResults("MainBench::testDecimateIF", nsecs);

    qDebug() << "MainBench::testDecimateIF: cleanup test data";
    delete[] buf;
}

void MainBench::testDecimateFI()
{
    QElapsedTimer timer;
    qint64 nsecs = 0;

    qDebug() << "MainBench::testDecimateFI: create test data";

    float *buf = new float[m_parser.getNbSamples()*2];
    m_convertBuffer.resize(m_parser.getNbSamples()/(1<<m_parser.getLog2Factor()));
    auto my_rand = std::bind(m_uniform_distribution_f, m_generator);
    std::generate(buf, buf + m_parser.getNbSamples()*2 - 1, my_rand); // make sure data is in [-1.0..1.0] range

    qDebug() << "MainBench::testDecimateFI: run test";

    for (uint32_t i = 0; i < m_parser.getRepetition(); i++)
    {
        timer.start();
        decimateFI(buf, m_parser.getNbSamples()*2);
        nsecs += timer.nsecsElapsed();
    }

    printResults("MainBench::testDecimateFI", nsecs);

    qDebug() << "MainBench::testDecimateFI: cleanup test data";
    delete[] buf;
}

void MainBench::testDecimateFF()
{
    QElapsedTimer timer;
    qint64 nsecs = 0;

    qDebug() << "MainBench::testDecimateFF: create test data";

    float *buf = new float[m_parser.getNbSamples()*2];
    m_convertBufferF.resize(m_parser.getNbSamples()/(1<<m_parser.getLog2Factor()));
    auto my_rand = std::bind(m_uniform_distribution_f, m_generator);
    std::generate(buf, buf + m_parser.getNbSamples()*2 - 1, my_rand); // make sure data is in [-1.0..1.0] range

    qDebug() << "MainBench::testDecimateFF: run test";

    for (uint32_t i = 0; i < m_parser.getRepetition(); i++)
    {
        timer.start();
        decimateFF(buf, m_parser.getNbSamples()*2);
        nsecs += timer.nsecsElapsed();
    }

    printResults("MainBench::testDecimateFF", nsecs);

    qDebug() << "MainBench::testDecimateFF: cleanup test data";
    delete[] buf;
}

void MainBench::decimateII(const qint16* buf, int len)
{
    SampleVector::iterator it = m_convertBuffer.begin();

    switch (m_parser.getLog2Factor())
    {
    case 0:
        m_decimatorsII.decimate1(&it, buf, len);
        break;
    case 1:
        m_decimatorsII.decimate2_cen(&it, buf, len);
        break;
    case 2:
        m_decimatorsII.decimate4_cen(&it, buf, len);
        break;
    case 3:
        m_decimatorsII.decimate8_cen(&it, buf, len);
        break;
    case 4:
        m_decimatorsII.decimate16_cen(&it, buf, len);
        break;
    case 5:
        m_decimatorsII.decimate32_cen(&it, buf, len);
        break;
    case 6:
        m_decimatorsII.decimate64_cen(&it, buf, len);
        break;
    default:
        break;
    }
}

void MainBench::decimateInfII(const qint16* buf, int len)
{
    SampleVector::iterator it = m_convertBuffer.begin();

    switch (m_parser.getLog2Factor())
    {
    case 0:
        m_decimatorsII.decimate1(&it, buf, len);
        break;
    case 1:
        m_decimatorsII.decimate2_inf(&it, buf, len);
        break;
    case 2:
        m_decimatorsII.decimate4_inf(&it, buf, len);
        break;
    case 3:
        m_decimatorsII.decimate8_inf(&it, buf, len);
        break;
    case 4:
        m_decimatorsII.decimate16_inf(&it, buf, len);
        break;
    case 5:
        m_decimatorsII.decimate32_inf(&it, buf, len);
        break;
    case 6:
        m_decimatorsII.decimate64_inf(&it, buf, len);
        break;
    default:
        break;
    }
}

void MainBench::decimateSupII(const qint16* buf, int len)
{
    SampleVector::iterator it = m_convertBuffer.begin();

    switch (m_parser.getLog2Factor())
    {
    case 0:
        m_decimatorsII.decimate1(&it, buf, len);
        break;
    case 1:
        m_decimatorsII.decimate2_sup(&it, buf, len);
        break;
    case 2:
        m_decimatorsII.decimate4_sup(&it, buf, len);
        break;
    case 3:
        m_decimatorsII.decimate8_sup(&it, buf, len);
        break;
    case 4:
        m_decimatorsII.decimate16_sup(&it, buf, len);
        break;
    case 5:
        m_decimatorsII.decimate32_sup(&it, buf, len);
        break;
    case 6:
        m_decimatorsII.decimate64_sup(&it, buf, len);
        break;
    default:
        break;
    }
}

void MainBench::decimateIF(const qint16* buf, int len)
{
    FSampleVector::iterator it = m_convertBufferF.begin();

    switch (m_parser.getLog2Factor())
    {
    case 0:
        m_decimatorsIF.decimate1(&it, buf, len);
        break;
    case 1:
        m_decimatorsIF.decimate2_cen(&it, buf, len);
        break;
    case 2:
        m_decimatorsIF.decimate4_cen(&it, buf, len);
        break;
    case 3:
        m_decimatorsIF.decimate8_cen(&it, buf, len);
        break;
    case 4:
        m_decimatorsIF.decimate16_cen(&it, buf, len);
        break;
    case 5:
        m_decimatorsIF.decimate32_cen(&it, buf, len);
        break;
    case 6:
        m_decimatorsIF.decimate64_cen(&it, buf, len);
        break;
    default:
        break;
    }
}

void MainBench::decimateFI(const float *buf, int len)
{
    SampleVector::iterator it = m_convertBuffer.begin();

    switch (m_parser.getLog2Factor())
    {
    case 0:
        m_decimatorsFI.decimate1(&it, buf, len);
        break;
    case 1:
        m_decimatorsFI.decimate2_cen(&it, buf, len);
        break;
    case 2:
        m_decimatorsFI.decimate4_cen(&it, buf, len);
        break;
    case 3:
        m_decimatorsFI.decimate8_cen(&it, buf, len);
        break;
    case 4:
        m_decimatorsFI.decimate16_cen(&it, buf, len);
        break;
    case 5:
        m_decimatorsFI.decimate32_cen(&it, buf, len);
        break;
    case 6:
        m_decimatorsFI.decimate64_cen(&it, buf, len);
        break;
    default:
        break;
    }
}

void MainBench::decimateFF(const float *buf, int len)
{
    FSampleVector::iterator it = m_convertBufferF.begin();

    switch (m_parser.getLog2Factor())
    {
    case 0:
        m_decimatorsFF.decimate1(&it, buf, len);
        break;
    case 1:
        m_decimatorsFF.decimate2_cen(&it, buf, len);
        break;
    case 2:
        m_decimatorsFF.decimate4_cen(&it, buf, len);
        break;
    case 3:
        m_decimatorsFF.decimate8_cen(&it, buf, len);
        break;
    case 4:
        m_decimatorsFF.decimate16_cen(&it, buf, len);
        break;
    case 5:
        m_decimatorsFF.decimate32_cen(&it, buf, len);
        break;
    case 6:
        m_decimatorsFF.decimate64_cen(&it, buf, len);
        break;
    default:
        break;
    }
}

void MainBench::printResults(const QString& prefix, qint64 nsecs)
{
    double ratekSs = (m_parser.getNbSamples()*m_parser.getRepetition() / (double) nsecs) * 1e6;
    QDebug info = qInfo();
    info.noquote();
    info << tr("%1: ran test in %L2 ns - sample rate: %3 kS/s").arg(prefix).arg(nsecs).arg(ratekSs);
}
