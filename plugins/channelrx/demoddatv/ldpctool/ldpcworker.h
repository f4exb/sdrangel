///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Jon Beniston, M7RCE                                        //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include <QObject>
#include <QByteArray>
#include <QMutex>
#include <QWaitCondition>

#include "testbench.h"
#include "algorithms.h"
#include "ldpc.h"
#include "layered_decoder.h"

class LDPCWorker : public QObject {
    Q_OBJECT

public:
    LDPCWorker(int modcod, int maxTrials, int batchSize, bool shortFrames);
    ~LDPCWorker();

    // Are we busy
    bool busy()
    {
        QMutexLocker locker(&m_mutexIn);
        return m_dataIn.size() >= BLOCKS;
    }

    // Is processed data available?
    bool dataAvailable()
    {
        QMutexLocker locker(&m_mutexOut);
        return !m_dataOut.isEmpty();
    }

    // Get processed data
    QByteArray data()
    {
        m_mutexOut.lock();
        if (m_dataOut.isEmpty()) {
            m_condOut.wait(&m_mutexOut);
        }
        QByteArray data = m_dataOut.takeAt(0);
        m_mutexOut.unlock();
        return data;
    }

public slots:
    void process(QByteArray data);

signals:
    void finished();

private:
    QMutex m_mutexIn;
    QMutex m_mutexOut;
    QWaitCondition m_condOut;
    QList<QByteArray> m_dataIn;
    QList<QByteArray> m_dataOut;
    int m_maxTrials;
    int BLOCKS;
    int m_codeLen;
    int m_dataLen;
    void *m_aligned_buffer;

    ldpctool::LDPCInterface *m_ldpc;
    ldpctool::code_type *m_code;
    ldpctool::simd_type *m_simd;

    typedef ldpctool::NormalUpdate<ldpctool::simd_type> update_type;
    //typedef SelfCorrectedUpdate<simd_type> update_type;

    //typedef MinSumAlgorithm<simd_type, update_type> algorithm_type;
    //typedef OffsetMinSumAlgorithm<simd_type, update_type, FACTOR> algorithm_type;
    typedef ldpctool::MinSumCAlgorithm<ldpctool::simd_type, update_type, ldpctool::FACTOR> algorithm_type;
    //typedef LogDomainSPA<simd_type, update_type> algorithm_type;
    //typedef LambdaMinAlgorithm<simd_type, update_type, 3> algorithm_type;
    //typedef SumProductAlgorithm<simd_type, update_type> algorithm_type;

    ldpctool::LDPCDecoder<ldpctool::simd_type, algorithm_type> m_decode;
};
