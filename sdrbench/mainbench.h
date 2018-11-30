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

#ifndef SDRBENCH_MAINBENCH_H_
#define SDRBENCH_MAINBENCH_H_

#include <QObject>
#include <random>
#include <functional>

#include "dsp/decimators.h"
#include "dsp/decimatorsif.h"
#include "dsp/decimatorsfi.h"
#include "dsp/decimatorsff.h"
#include "parserbench.h"

namespace qtwebapp {
    class LoggerWithFile;
}

class MainBench: public QObject {
    Q_OBJECT

public:
    explicit MainBench(qtwebapp::LoggerWithFile *logger, const ParserBench& parser, QObject *parent = 0);
    ~MainBench();

public slots:
    void run();

signals:
    void finished();

private:
    void testDecimateII(ParserBench::TestType testType = ParserBench::TestDecimatorsII);
    void testDecimateIF();
    void testDecimateFI();
    void testDecimateFF();
    void decimateII(const qint16 *buf, int len);
    void decimateInfII(const qint16 *buf, int len);
    void decimateSupII(const qint16 *buf, int len);
    void decimateIF(const qint16 *buf, int len);
    void decimateFI(const float *buf, int len);
    void decimateFF(const float *buf, int len);
    void printResults(const QString& prefix, qint64 nsecs);

    static MainBench *m_instance;
    qtwebapp::LoggerWithFile *m_logger;
    const ParserBench& m_parser;
    std::mt19937 m_generator;
    std::uniform_real_distribution<float> m_uniform_distribution_f;
    std::uniform_int_distribution<qint16> m_uniform_distribution_s16;

	Decimators<qint32, qint16, SDR_RX_SAMP_SZ, 12> m_decimatorsII;
	DecimatorsIF<qint16, 12> m_decimatorsIF;
	DecimatorsFI m_decimatorsFI;
    DecimatorsFF m_decimatorsFF;

    SampleVector m_convertBuffer;
    FSampleVector m_convertBufferF;
};

#endif // SDRBENCH_MAINBENCH_H_
