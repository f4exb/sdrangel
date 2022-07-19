/*
LDPC testbench
Copyright 2018 Ahmet Inan <xdsopl@gmail.com>

Transformed into external decoder for third-party applications
Copyright 2019 <pabr@pabr.org>

Transformed again in to a Qt worker.
*/

#include <QDebug>

#include <stdlib.h>
#ifdef _MSC_VER
#include <malloc.h>
#endif
#include <iostream>
#include <iomanip>
#include <random>
#include <cmath>
#include <cassert>
#include <chrono>
#include <cstring>
#include <algorithm>
#include <functional>
#include "ldpcworker.h"

LDPCWorker::LDPCWorker(int modcod, int maxTrials, int batchSize, bool shortFrames) :
    m_maxTrials(maxTrials),
    m_aligned_buffer(nullptr),
    m_ldpc(nullptr),
    m_code(nullptr),
    m_simd(nullptr)
{
    // DVB-S2 MODCOD definitions
    static const char *mc_tabnames[2][32] = { // [shortframes][modcod]
                                             {// Normal frames
                                              0, "B1", "B2", "B3", "B4", "B5", "B6", "B7",
                                              "B8", "B9", "B10", "B11", "B5", "B6", "B7", "B9",
                                              "B10", "B11", "B6", "B7", "B8", "B9", "B10", "B11",
                                              "B7", "B8", "B8", "B10", "B11", 0, 0, 0},
                                             {// Short frames
                                              0, "C1", "C2", "C3", "C4", "C5", "C6", "C7",
                                              "C8", "C9", "C10", 0, "C5", "C6", "C7", "C9",
                                              "C10", 0, "C6", "C7", "C8", "C9", "C10", 0,
                                              "C7", "C8", "C8", "C10", 0, 0, 0, 0}};

    const char *tabname = mc_tabnames[shortFrames][modcod];
    if (!tabname)
    {
        qCritical() << "LDPCWorker::LDPCWorker: unsupported modcod";
        return;
    }

    m_ldpc = ldpctool::create_ldpc((char *)"S2", tabname[0], atoi(tabname + 1));

    if (!m_ldpc)
    {
        qCritical() << "LDPCWorker::LDPCWorker: no such table!";
        return;
    }

    m_codeLen = m_ldpc->code_len();
    m_dataLen = m_ldpc->data_len();

    m_decode.init(m_ldpc);

    BLOCKS = batchSize;
    m_code = new ldpctool::code_type[BLOCKS * m_codeLen];
    m_aligned_buffer = ldpctool::LDPCUtil::aligned_malloc(sizeof(ldpctool::simd_type), sizeof(ldpctool::simd_type) * m_codeLen);
    m_simd = reinterpret_cast<ldpctool::simd_type *>(m_aligned_buffer);

    // Expect LLR values in int8_t format.
    if (sizeof(ldpctool::code_type) != 1) {
        qCritical() << "LDPCWorker::LDPCWorker: Unsupported code_type";
    }
}

LDPCWorker::~LDPCWorker()
{
    m_condOut.wakeAll();
    delete m_ldpc;

    ldpctool::LDPCUtil::aligned_free(m_aligned_buffer);
    delete[] m_code;
}

void LDPCWorker::process(QByteArray data)
{
    int trials_count = 0;
    int max_count = 0;
    int num_decodes = 0;

    int iosize = m_codeLen * sizeof(*m_code);

    m_mutexIn.lock();
    m_dataIn.append(data);

    if (m_dataIn.size() >= BLOCKS)
    {
        for (int j = 0; j < BLOCKS; j++)
        {
            QByteArray inputData = m_dataIn.takeAt(0);
            memcpy(m_code + j * m_codeLen, inputData.data(), inputData.size());
        }
        m_mutexIn.unlock();

        for (int j = 0; j < BLOCKS; j += ldpctool::SIMD_WIDTH)
        {
            int blocks = j + ldpctool::SIMD_WIDTH > BLOCKS ? BLOCKS - j : ldpctool::SIMD_WIDTH;

            for (int n = 0; n < blocks; ++n)
                for (int i = 0; i < m_codeLen; ++i)
                    reinterpret_cast<ldpctool::code_type *>(m_simd + i)[n] = m_code[(j + n) * m_codeLen + i];

            int count = m_decode(m_simd, m_simd + m_dataLen, m_maxTrials, blocks);
            num_decodes++;

            if (count < 0)
            {
                trials_count += m_maxTrials;
                max_count++;
            }
            else
            {
                trials_count += m_maxTrials - count;
            }

            if (num_decodes == 20)
            {
                qDebug() << "ldpc_tool: trials: " << ((trials_count*100)/(num_decodes*m_maxTrials)) << "% max: "
                        << ((max_count*100)/num_decodes) << "% at converging to a code word (max trials: " << m_maxTrials << ")";
                trials_count = 0;
                max_count = 0;
                num_decodes = 0;
            }

            for (int n = 0; n < blocks; ++n)
                for (int i = 0; i < m_codeLen; ++i)
                    m_code[(j + n) * m_codeLen + i] = reinterpret_cast<ldpctool::code_type *>(m_simd + i)[n];
        }

        m_mutexOut.lock();
        for (int j = 0; j < BLOCKS; j++)
        {
            QByteArray outputData((const char *)m_code + j * m_codeLen, iosize);
            m_dataOut.append(outputData);
        }
        m_condOut.wakeAll();
        m_mutexOut.unlock();
    }
    else
    {
        m_mutexIn.unlock();
    }
}
