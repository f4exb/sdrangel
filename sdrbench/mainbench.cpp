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
    m_parser(parser)
{
    qDebug() << "MainBench::MainBench: start";
    m_instance = this;
    qDebug() << "MainBench::MainBench: end";
}

MainBench::~MainBench()
{}

void MainBench::run()
{
    QElapsedTimer timer;
    qint64 nsecs;

    qDebug() << "MainBench::run: parameters:" 
        << " test: " << m_parser.getTest()
        << " nsamples: " << m_parser.getNbSamples()
        << " repet: " << m_parser.getRepetition()
        << " log2f: " << m_parser.getLog2Factor();
    
    qDebug() << "MainBench::run: create test data";

    m_buf = new qint16[m_parser.getNbSamples()*2];
    m_convertBuffer.resize(m_parser.getNbSamples()/(1<<m_parser.getLog2Factor()));

    qDebug() << "MainBench::run: run test";
    timer.start();

    for (uint32_t i = 0; i < m_parser.getRepetition(); i++)
    {
        decimate(m_buf, m_parser.getNbSamples()*2);
    }

    nsecs = timer.nsecsElapsed();
    QDebug debug = qDebug();
    debug.noquote();
    debug << tr("MainBench::run: ran test in %L1 ns").arg(nsecs);


    qDebug() << "MainBench::run: cleanup test data";

    delete[] m_buf;

    emit finished();
}

void MainBench::decimate(const qint16* buf, int len)
{
    SampleVector::iterator it = m_convertBuffer.begin();

    switch (m_parser.getLog2Factor())
    {
    case 0:
        m_decimators.decimate1(&it, buf, len);
        break;
    case 1:
        m_decimators.decimate2_cen(&it, buf, len);
        break;
    case 2:
        m_decimators.decimate4_cen(&it, buf, len);
        break;
    case 3:
        m_decimators.decimate8_cen(&it, buf, len);
        break;
    case 4:
        m_decimators.decimate16_cen(&it, buf, len);
        break;
    case 5:
        m_decimators.decimate32_cen(&it, buf, len);
        break;
    case 6:
        m_decimators.decimate64_cen(&it, buf, len);
        break;
    default:
        break;
    }    
}