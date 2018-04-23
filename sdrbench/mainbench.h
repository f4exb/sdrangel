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

#include "dsp/decimators.h"
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
    void decimate(const qint16 *buf, int len);

    static MainBench *m_instance;
    qtwebapp::LoggerWithFile *m_logger;
    const ParserBench& m_parser;

#ifdef SDR_RX_SAMPLE_24BIT
    Decimators<qint64, qint16, SDR_RX_SAMP_SZ, 12> m_decimators;
#else
	Decimators<qint32, qint16, SDR_RX_SAMP_SZ, 12> m_decimators;
#endif
    
    qint16 *m_buf;
    SampleVector m_convertBuffer;
};

#endif // SDRBENCH_MAINBENCH_H_
