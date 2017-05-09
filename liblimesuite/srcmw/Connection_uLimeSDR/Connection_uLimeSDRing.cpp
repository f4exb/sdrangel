/**
    @file Connection_uLimeSDRing.cpp
    @author Lime Microsystems
    @brief Implementation of uLimeSDR board connection (streaming API)
*/

#include "Connection_uLimeSDR.h"
#include "fifo.h"
#include <LMS7002M.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <complex>
#include <ciso646>
#include <vector>
#include <FPGA_common.h>
#include "ErrorReporting.h"

using namespace lime;
using namespace std;

#define __unix__

/** @brief Configures FPGA PLLs to LimeLight interface frequency
*/
int Connection_uLimeSDR::UpdateExternalDataRate(const size_t channel, const double txRate, const double rxRate, const double txPhase, const double rxPhase)
{
    const float txInterfaceClk = 2 * txRate;
    const float rxInterfaceClk = 2 * rxRate;
    int status = 0;

    mExpectedSampleRate = rxRate;

    lime::fpga::FPGA_PLL_clock clocks[4];

    clocks[0].bypass = false;
    clocks[0].index = 0;
    clocks[0].outFrequency = txInterfaceClk;
    clocks[0].phaseShift_deg = 0;
    clocks[0].findPhase = false;
    clocks[1].bypass = false;
    clocks[1].index = 1;
    clocks[1].outFrequency = txInterfaceClk;
    clocks[1].findPhase = false;
    clocks[1].phaseShift_deg = txPhase;
    clocks[2].bypass = false;
    clocks[2].index = 2;
    clocks[2].outFrequency = rxInterfaceClk;
    clocks[2].phaseShift_deg = 0;
    clocks[2].findPhase = false;
    clocks[3].bypass = false;
    clocks[3].index = 3;
    clocks[3].outFrequency = rxInterfaceClk;
    clocks[3].findPhase = false;
    clocks[3].phaseShift_deg = rxPhase;

    status = lime::fpga::SetPllFrequency(this, 0, rxInterfaceClk, clocks, 4);

    return status;
}

/** @brief Configures FPGA PLLs to LimeLight interface frequency
*/
int Connection_uLimeSDR::UpdateExternalDataRate(const size_t channel, const double txRate_Hz, const double rxRate_Hz)
{
    const float txInterfaceClk = 2 * txRate_Hz;
    const float rxInterfaceClk = 2 * rxRate_Hz;
    int status = 0;
    uint32_t reg20;
    const double rxPhC1[] = { 91.08, 89.46 };
    const double rxPhC2[] = { -1 / 6e6, 1.24e-6 };
    const double txPhC1[] = { 89.75, 89.61 };
    const double txPhC2[] = { -3.0e-7, 2.71e-7 };

    const std::vector<uint32_t> spiAddr = { 0x0021, 0x0022, 0x0023, 0x0024,
        0x0027, 0x002A, 0x0400, 0x040C,
        0x040B, 0x0400, 0x040B, 0x0400 };
    const int bakRegCnt = spiAddr.size() - 4;
    auto info = GetDeviceInfo();
    const int addrLMS7002M = info.addrsLMS7002M.at(0);
    bool phaseSearch = false;
    //if (this->chipVersion == 0x3841) //0x3840 LMS7002Mr2, 0x3841 LMS7002Mr3
    /*if (rxInterfaceClk >= 5e6 || txInterfaceClk >= 5e6)
        phaseSearch = true;*/
    mExpectedSampleRate = rxRate_Hz;
    std::vector<uint32_t> dataWr;
    std::vector<uint32_t> dataRd;

    if (phaseSearch)
    {
        dataWr.resize(spiAddr.size());
        dataRd.resize(spiAddr.size());
        //backup registers
        dataWr[0] = (uint32_t(0x0020) << 16);
        TransactSPI(addrLMS7002M, dataWr.data(), &reg20, 1);

        dataWr[0] = (1 << 31) | (uint32_t(0x0020) << 16) | 0xFFFD; //msbit 1=SPI write
        TransactSPI(addrLMS7002M, dataWr.data(), nullptr, 1);

        for (int i = 0; i < bakRegCnt; ++i)
            dataWr[i] = (spiAddr[i] << 16);
        TransactSPI(addrLMS7002M, dataWr.data(), dataRd.data(), bakRegCnt);
        //UpdateThreads(true);
    }

    if ((txInterfaceClk >= 5e6) && (rxInterfaceClk >= 5e6))
    {
        lime::fpga::FPGA_PLL_clock clocks[4];

        clocks[0].bypass = false;
        clocks[0].index = 0;
        clocks[0].outFrequency = txInterfaceClk;
        clocks[0].phaseShift_deg = 0;
        clocks[0].findPhase = false;
        clocks[1].bypass = false;
        clocks[1].index = 1;
        clocks[1].outFrequency = txInterfaceClk;
        clocks[1].findPhase = false;
        if (this->chipVersion == 0x3841)
            clocks[1].phaseShift_deg = txPhC1[1] + txPhC2[1] * txInterfaceClk;
        else
            clocks[1].phaseShift_deg = txPhC1[0] + txPhC2[0] * txInterfaceClk;
        clocks[2].bypass = false;
        clocks[2].index = 2;
        clocks[2].outFrequency = rxInterfaceClk;
        clocks[2].phaseShift_deg = 0;
        clocks[2].findPhase = false;
        clocks[3].bypass = false;
        clocks[3].index = 3;
        clocks[3].outFrequency = rxInterfaceClk;
        clocks[3].findPhase = false;
        if (this->chipVersion == 0x3841)
            clocks[3].phaseShift_deg = rxPhC1[1] + rxPhC2[1] * rxInterfaceClk;
        else
            clocks[3].phaseShift_deg = rxPhC1[0] + rxPhC2[0] * rxInterfaceClk;

        if (phaseSearch)
        {
            {
#ifndef NDEBUG
                printf("RX phase config:\n");
#endif
                clocks[3].findPhase = true;
                const std::vector<uint32_t> spiData = { 0x0E9F, 0x07FF, 0x5550, 0xE4E4,
                    0xE4E4, 0x0086, 0x028D, 0x00FF, 0x5555, 0x02CD, 0xAAAA, 0x02ED };
                //Load test config
                const int setRegCnt = spiData.size();
                for (int i = 0; i < setRegCnt; ++i)
                    dataWr[i] = (1 << 31) | (uint32_t(spiAddr[i]) << 16) | spiData[i]; //msbit 1=SPI write
                TransactSPI(addrLMS7002M, dataWr.data(), nullptr, setRegCnt);
                status = lime::fpga::SetPllFrequency(this, 0, rxInterfaceClk, clocks, 4);
            }
            {
#ifndef NDEBUG
                printf("TX phase config:\n");
#endif
                clocks[3].findPhase = false;
                const std::vector<uint32_t> spiData = { 0x0E9F, 0x07FF, 0x5550, 0xE4E4, 0xE4E4, 0x0484 };
                WriteRegister(0x000A, 0x0000);
                //Load test config
                const int setRegCnt = spiData.size();
                for (int i = 0; i < setRegCnt; ++i)
                    dataWr[i] = (1 << 31) | (uint32_t(spiAddr[i]) << 16) | spiData[i]; //msbit 1=SPI write
                TransactSPI(addrLMS7002M, dataWr.data(), nullptr, setRegCnt);
                clocks[1].findPhase = true;
                WriteRegister(0x000A, 0x0200);

            }
        }
        status = lime::fpga::SetPllFrequency(this, 0, rxInterfaceClk, clocks, 4);
    }
    else
    {
        status = lime::fpga::SetDirectClocking(this, 0, rxInterfaceClk, 90);
        if (status == 0)
            status = lime::fpga::SetDirectClocking(this, 1, rxInterfaceClk, 90);
    }

    if (phaseSearch)
    {
        //Restore registers
        for (int i = 0; i < bakRegCnt; ++i)
            dataWr[i] = (1 << 31) | (uint32_t(spiAddr[i]) << 16) | dataRd[i]; //msbit 1=SPI write
        TransactSPI(addrLMS7002M, dataWr.data(), nullptr, bakRegCnt);
        dataWr[0] = (1 << 31) | (uint32_t(0x0020) << 16) | reg20; //msbit 1=SPI write
        TransactSPI(addrLMS7002M, dataWr.data(), nullptr, 1);
        WriteRegister(0x000A, 0);
        UpdateThreads();
    }
    return status;
}


int Connection_uLimeSDR::ReadRawStreamData(char* buffer, unsigned length, int timeout_ms)
{
    int totalBytesReceived = 0;
    fpga::StopStreaming(this);

    //ResetStreamBuffers();
    WriteRegister(0x0008, 0x0100 | 0x2);
    WriteRegister(0x0007, 1);

    fpga::StartStreaming(this);

    int handle = BeginDataReading(buffer, length);
    if (WaitForReading(handle, timeout_ms))
        totalBytesReceived = FinishDataReading(buffer, length, handle);

    AbortReading();
    fpga::StopStreaming(this);

    return totalBytesReceived;
}

int Connection_uLimeSDR::ResetStreamBuffers()
{
    rxSize = 0;
    txSize = 0;
#ifndef __unix__
    if (FT_AbortPipe(mFTHandle, mStreamRdEndPtAddr)!=FT_OK)
        return -1;
    if (FT_AbortPipe(mFTHandle, mStreamWrEndPtAddr)!=FT_OK)
        return -1;
    if (FT_FlushPipe(mFTHandle, mStreamRdEndPtAddr)!=FT_OK)
        return -1;
#else
    return FT_FlushPipe(mStreamRdEndPtAddr);
#endif
    return 0;
}

/** @brief Function dedicated for receiving data samples from board
    @param rxFIFO FIFO to store received data
    @param terminate periodically pooled flag to terminate thread
    @param dataRate_Bps (optional) if not NULL periodically returns data rate in bytes per second
*/
void Connection_uLimeSDR::ReceivePacketsLoop(const Connection_uLimeSDR::ThreadData args)
{
    //auto dataPort = args.dataPort;
    auto terminate = args.terminate;
    auto dataRate_Bps = args.dataRate_Bps;
    auto generateData = args.generateData;
    auto safeToConfigInterface = args.safeToConfigInterface;

    //at this point FPGA has to be already configured to output samples
    const uint8_t chCount = args.channels.size();
    const auto link = args.channels[0]->config.linkFormat;
    const uint32_t samplesInPacket = (link == StreamConfig::STREAM_12_BIT_COMPRESSED ? 1360 : 1020)/chCount;

    double latency=0;
    for (int i = 0; i < chCount; i++)
    {
        latency += args.channels[i]->config.performanceLatency/chCount;
    }
    const unsigned tmp_cnt = (latency * 4)+0.5;

    const uint8_t packetsToBatch = (1<<tmp_cnt);
    const uint32_t bufferSize = packetsToBatch*sizeof(FPGA_DataPacket);
    const uint8_t buffersCount = 16; // must be power of 2
    vector<int> handles(buffersCount, 0);
    vector<char>buffers(buffersCount*bufferSize, 0);
    vector<StreamChannel::Frame> chFrames;
    try
    {
        chFrames.resize(chCount);
    }
    catch (const std::bad_alloc &ex)
    {
        ReportError("Error allocating Rx buffers, not enough memory");
        return;
    }

    uint8_t activeTransfers = 0;
    for (int i = 0; i<buffersCount; ++i)
    {
        handles[i] = this->BeginDataReading(&buffers[i*bufferSize], bufferSize);
        ++activeTransfers;
    }

    int bi = 0;
    unsigned long totalBytesReceived = 0; //for data rate calculation
    int m_bufferFailures = 0;
    int32_t droppedSamples = 0;
    int32_t packetLoss = 0;

    vector<uint32_t> samplesCollected(chCount, 0);
    vector<uint32_t> samplesReceived(chCount, 0);

    auto t1 = chrono::high_resolution_clock::now();
    auto t2 = chrono::high_resolution_clock::now();

    std::mutex txFlagsLock;
    condition_variable resetTxFlags;
    //worker thread for reseting late Tx packet flags
    std::thread txReset([](ILimeSDRStreaming* port,
                        atomic<bool> *terminate,
                        mutex *spiLock,
                        condition_variable *doWork)
    {
        uint32_t reg9;
        port->ReadRegister(0x0009, reg9);
        const uint32_t addr[] = {0x0009, 0x0009};
        const uint32_t data[] = {reg9 | (1 << 1), reg9 & ~(1 << 1)};
        while (not terminate->load())
        {
            std::unique_lock<std::mutex> lck(*spiLock);
            doWork->wait(lck);
            port->WriteRegisters(addr, data, 2);
        }
    }, this, terminate, &txFlagsLock, &resetTxFlags);

    int resetFlagsDelay = 128;
    uint64_t prevTs = 0;
    while (terminate->load() == false)
    {
        if(generateData->load())
        {
            if(activeTransfers == 0) //stop FPGA when last transfer completes
                fpga::StopStreaming(this);
            safeToConfigInterface->notify_all(); //notify that it's safe to change chip config
            const int batchSize = (this->mExpectedSampleRate/chFrames[0].samplesCount)/10;
            IStreamChannel::Metadata meta;
            for(int i=0; i<batchSize; ++i)
            {
                for(int ch=0; ch<chCount; ++ch)
                {
                    meta.timestamp = chFrames[ch].timestamp;
                    for(int j=0; j<chFrames[ch].samplesCount; ++j)
                    {
                        chFrames[ch].samples[j].i = 0;
                        chFrames[ch].samples[j].q = 0;
                    }
                    uint32_t samplesPushed = args.channels[ch]->Write((const void*)chFrames[ch].samples, chFrames[ch].samplesCount, &meta);
                    samplesReceived[ch] += chFrames[ch].samplesCount;
                    if(samplesPushed != chFrames[ch].samplesCount)
                        printf("Rx samples pushed %i/%i\n", samplesPushed, chFrames[ch].samplesCount);
                }
            }
            this_thread::sleep_for(chrono::milliseconds(100));
        }
        int32_t bytesReceived = 0;
        if(handles[bi] >= 0)
        {
            if (this->WaitForReading(handles[bi], 1000) == false)
                ++m_bufferFailures;
            bytesReceived = this->FinishDataReading(&buffers[bi*bufferSize], bufferSize, handles[bi]);
            --activeTransfers;
            totalBytesReceived += bytesReceived;
            if (bytesReceived != int32_t(bufferSize)) //data should come in full sized packets
                ++m_bufferFailures;
        }
        bool txLate=false;
        for (uint8_t pktIndex = 0; pktIndex < bytesReceived / sizeof(FPGA_DataPacket); ++pktIndex)
        {
            const FPGA_DataPacket* pkt = (FPGA_DataPacket*)&buffers[bi*bufferSize];
            const uint8_t byte0 = pkt[pktIndex].reserved[0];
            if ((byte0 & (1 << 3)) != 0 && !txLate) //report only once per batch
            {
                txLate = true;
                if(resetFlagsDelay > 0)
                    --resetFlagsDelay;
                else
                {
                    printf("L");
                    resetTxFlags.notify_one();
                    resetFlagsDelay = packetsToBatch*buffersCount;
                    if (args.reportLateTx) args.reportLateTx(pkt[pktIndex].counter);
                }
            }
            uint8_t* pktStart = (uint8_t*)pkt[pktIndex].data;
            if(pkt[pktIndex].counter - prevTs != samplesInPacket && pkt[pktIndex].counter != prevTs)
            {
#ifndef NDEBUG
                printf("\tRx pktLoss ts diff %lli\n", (long long)pkt[pktIndex].counter - prevTs);
#endif
                packetLoss += (pkt[pktIndex].counter - prevTs)/samplesInPacket;
            }
            prevTs = pkt[pktIndex].counter;
            if(args.lastTimestamp)
                args.lastTimestamp->store(pkt[pktIndex].counter);
            //parse samples
            vector<complex16_t*> dest(chCount);
            for(uint8_t c=0; c<chCount; ++c)
                dest[c] = (chFrames[c].samples);
            size_t samplesCount = 0;
            fpga::FPGAPacketPayload2Samples(pktStart, 4080, chCount, link, dest.data(), &samplesCount);

            for(int ch=0; ch<chCount; ++ch)
            {
                IStreamChannel::Metadata meta;
                meta.timestamp = pkt[pktIndex].counter;
                meta.flags = RingFIFO::OVERWRITE_OLD;
                uint32_t samplesPushed = args.channels[ch]->Write((const void*)chFrames[ch].samples, samplesCount, &meta, 100);
                if(samplesPushed != samplesCount)
                    droppedSamples += samplesCount-samplesPushed;
            }
        }
        // Re-submit this request to keep the queue full
        if(not generateData->load())
        {
            if(activeTransfers == 0) //reactivate FPGA and USB transfers
                fpga::StartStreaming(this);
            for(int i=0; i<buffersCount-activeTransfers; ++i)
            {
                handles[bi] = this->BeginDataReading(&buffers[bi*bufferSize], bufferSize);
                bi = (bi + 1) & (buffersCount-1);
                ++activeTransfers;
            }
        }
        else
        {
            handles[bi] = -1;
            bi = (bi + 1) & (buffersCount-1);
        }
        t2 = chrono::high_resolution_clock::now();
        auto timePeriod = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
        if (timePeriod >= 1000)
        {
            t1 = t2;
            //total number of bytes sent per second
            double dataRate = 1000.0*totalBytesReceived / timePeriod;
#ifndef NDEBUG
            //each channel sample rate
            float samplingRate = 1000.0*samplesReceived[0] / timePeriod;
            printf("Rx: %.3f MB/s, Fs: %.3f MHz, overrun: %i, loss: %i \n", dataRate / 1000000.0, samplingRate / 1000000.0, droppedSamples, packetLoss);
#endif
            samplesReceived[0] = 0;
            totalBytesReceived = 0;
            m_bufferFailures = 0;
            droppedSamples = 0;
            packetLoss = 0;

            if (dataRate_Bps)
                dataRate_Bps->store((uint32_t)dataRate);
        }
    }
    this->AbortReading();
    for (int j = 0; j<buffersCount; j++)
    {
        if(handles[bi] >= 0)
        {
            this->WaitForReading(handles[bi], 1000);
            this->FinishDataReading(&buffers[bi*bufferSize], bufferSize, handles[bi]);
        }
        bi = (bi + 1) & (buffersCount-1);
    }
    resetTxFlags.notify_one();
    txReset.join();
    if (dataRate_Bps)
        dataRate_Bps->store(0);
}

/** @brief Functions dedicated for transmitting packets to board
    @param txFIFO data source FIFO
    @param terminate periodically pooled flag to terminate thread
    @param dataRate_Bps (optional) if not NULL periodically returns data rate in bytes per second
*/
void Connection_uLimeSDR::TransmitPacketsLoop(const Connection_uLimeSDR::ThreadData args)
{
    //auto dataPort = args.dataPort;
    auto terminate = args.terminate;
    auto dataRate_Bps = args.dataRate_Bps;

    //at this point FPGA has to be already configured to output samples
    const uint8_t maxChannelCount = 2;
    const uint8_t chCount = args.channels.size();
    const auto link = args.channels[0]->config.linkFormat;

    double latency=0;
    for (int i = 0; i < chCount; i++)
    {
        latency += args.channels[i]->config.performanceLatency/chCount;
    }
    const unsigned tmp_cnt = (latency * 4)+0.5;

    const uint8_t buffersCount = 16; // must be power of 2
    assert(buffersCount % 2 == 0);
    const uint8_t packetsToBatch = (1<<tmp_cnt); //packets in single USB transfer
    const uint32_t bufferSize = packetsToBatch*4096;
    const uint32_t popTimeout_ms = 100;

    const int maxSamplesBatch = (link==StreamConfig::STREAM_12_BIT_COMPRESSED?1360:1020)/chCount;
    vector<int> handles(buffersCount, 0);
    vector<bool> bufferUsed(buffersCount, 0);
    vector<uint32_t> bytesToSend(buffersCount, 0);
    vector<complex16_t> samples[maxChannelCount];
    vector<char> buffers;
    try
    {
        for(int i=0; i<chCount; ++i)
            samples[i].resize(maxSamplesBatch);
        buffers.resize(buffersCount*bufferSize, 0);
    }
    catch (const std::bad_alloc& ex) //not enough memory for buffers
    {
        printf("Error allocating Tx buffers, not enough memory\n");
        return;
    }

    int m_bufferFailures = 0;
    long totalBytesSent = 0;

    uint32_t samplesSent = 0;

    auto t1 = chrono::high_resolution_clock::now();
    auto t2 = chrono::high_resolution_clock::now();

    uint8_t bi = 0; //buffer index
    while (terminate->load() != true)
    {
        if (bufferUsed[bi])
        {
            if (this->WaitForSending(handles[bi], 1000) == false)
                ++m_bufferFailures;
            uint32_t bytesSent = this->FinishDataSending(&buffers[bi*bufferSize], bytesToSend[bi], handles[bi]);
            totalBytesSent += bytesSent;
            if (bytesSent != bytesToSend[bi])
                ++m_bufferFailures;
            bufferUsed[bi] = false;
        }
        int i=0;

        while(i<packetsToBatch && terminate->load() != true)
        {
            IStreamChannel::Metadata meta;
            FPGA_DataPacket* pkt = reinterpret_cast<FPGA_DataPacket*>(&buffers[bi*bufferSize]);
            for(int ch=0; ch<chCount; ++ch)
            {
                int samplesPopped = args.channels[ch]->Read(samples[ch].data(), maxSamplesBatch, &meta, popTimeout_ms);
                if (samplesPopped != maxSamplesBatch)
                {
                #ifndef NDEBUG
                    printf("Warning popping from TX, samples popped %i/%i\n", samplesPopped, maxSamplesBatch);
                #endif
                }

            }
            if(terminate->load() == true) //early termination
                break;
            pkt[i].counter = meta.timestamp;
            pkt[i].reserved[0] = 0;
            //by default ignore timestamps
            const int ignoreTimestamp = !(meta.flags & IStreamChannel::Metadata::SYNC_TIMESTAMP);
            pkt[i].reserved[0] |= ((int)ignoreTimestamp << 4); //ignore timestamp

            vector<complex16_t*> src(chCount);
            for(uint8_t c=0; c<chCount; ++c)
                src[c] = (samples[c].data());
            uint8_t* const dataStart = (uint8_t*)pkt[i].data;
            fpga::Samples2FPGAPacketPayload(src.data(), maxSamplesBatch, chCount, link, dataStart, nullptr);
            samplesSent += maxSamplesBatch;
            ++i;
        }

        bytesToSend[bi] = bufferSize;
        handles[bi] = this->BeginDataSending(&buffers[bi*bufferSize], bytesToSend[bi]);
        bufferUsed[bi] = true;

        t2 = chrono::high_resolution_clock::now();
        auto timePeriod = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
        if (timePeriod >= 1000)
        {
            //total number of bytes sent per second
            float dataRate = 1000.0*totalBytesSent / timePeriod;
            if(dataRate_Bps)
                dataRate_Bps->store(dataRate);
            m_bufferFailures = 0;
            samplesSent = 0;
            totalBytesSent = 0;
            t1 = t2;
#ifndef NDEBUG
            //total number of samples from all channels per second
            float sampleRate = 1000.0*samplesSent / timePeriod;
            printf("Tx: %.3f MB/s, Fs: %.3f MHz, failures: %i\n", dataRate / 1000000.0, sampleRate / 1000000.0, m_bufferFailures);
#endif
        }
        bi = (bi + 1) & (buffersCount-1);
    }

    // Wait for all the queued requests to be cancelled
    this->AbortSending();
    for (int j = 0; j<buffersCount; j++)
    {
        if (bufferUsed[bi])
        {
            this->WaitForSending(handles[bi], 1000);
            this->FinishDataSending(&buffers[bi*bufferSize], bufferSize, handles[bi]);
        }
        bi = (bi + 1) & (buffersCount-1);
    }
    if (dataRate_Bps)
        dataRate_Bps->store(0);
}
