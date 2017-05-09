/**
@file Connection_uLimeSDR.h
@author Lime Microsystems
@brief Implementation of STREAM board connection.
*/

#pragma once
#include <ConnectionRegistry.h>
#include <IConnection.h>
#include <ILimeSDRStreaming.h>
#include <vector>
#include <string>
#include <atomic>
#include <memory>
#include <thread>
#include "fifo.h"

#define __unix__
#include "windows.h"

#ifndef __unix__
#include "windows.h"
#include "FTD3XXLibrary/FTD3XX.h"
#else
#include <libusb-1.0/libusb.h>
#include <mutex>
#include <condition_variable>
#include <chrono>
#endif

namespace lime{

#define USB_MAX_CONTEXTS 64 //maximum number of contexts for asynchronous transfers

class Connection_uLimeSDR : public ILimeSDRStreaming
{
public:
    /** @brief Wrapper class for holding USB asynchronous transfers contexts
    */
    class USBTransferContext
    {
    public:
        USBTransferContext() : used(false)
        {
            id = idCounter++;
#ifndef __unix__
            context = NULL;
#else
            transfer = libusb_alloc_transfer(0);
            bytesXfered = 0;
            bytesExpected = 0;
            done = 0;
#endif
        }
        ~USBTransferContext()
        {
#ifdef __unix__
            libusb_free_transfer(transfer);
#endif
        }
        bool reset()
        {
            if(used)
                return false;
            return true;
        }
        bool used;
        int id;
        static int idCounter;
#ifndef __unix__
        PUCHAR context;
        OVERLAPPED inOvLap;
#else
        libusb_transfer* transfer;
        long bytesXfered;
        long bytesExpected;
        std::atomic<bool> done;
        std::mutex transferLock;
        std::condition_variable cv;
#endif
    };

    Connection_uLimeSDR(void *arg);
    Connection_uLimeSDR(void *ctx, const unsigned index, const int vid = -1, const int pid = -1);

    virtual ~Connection_uLimeSDR(void);

    int Open(const unsigned index, const int vid, const int pid);
    void Close();
    bool IsOpen();
    int GetOpenedIndex();

    virtual int Write(const unsigned char *buffer, int length, int timeout_ms = 100) override;
    virtual int Read(unsigned char *buffer, int length, int timeout_ms = 100) override;

    //hooks to update FPGA plls when baseband interface data rate is changed
    virtual int UpdateExternalDataRate(const size_t channel, const double txRate, const double rxRate, const double txPhase, const double rxPhase)override;
    virtual int UpdateExternalDataRate(const size_t channel, const double txRate, const double rxRate) override;
    int ReadRawStreamData(char* buffer, unsigned length, int timeout_ms = 100)override;
protected:
    virtual void ReceivePacketsLoop(const ThreadData args) override;
    virtual void TransmitPacketsLoop(const ThreadData args) override;

    virtual int BeginDataReading(char* buffer, uint32_t length);
    virtual int WaitForReading(int contextHandle, unsigned int timeout_ms);
    virtual int FinishDataReading(char* buffer, uint32_t length, int contextHandle);
    virtual void AbortReading();

    virtual int BeginDataSending(const char* buffer, uint32_t length);
    virtual int WaitForSending(int contextHandle, uint32_t timeout_ms);
    virtual int FinishDataSending(const char* buffer, uint32_t length, int contextHandle);
    virtual void AbortSending();

    int ResetStreamBuffers() override;

    eConnectionType GetType(void) {return USB_PORT;}

    USBTransferContext contexts[USB_MAX_CONTEXTS];
    USBTransferContext contextsToSend[USB_MAX_CONTEXTS];

    bool isConnected;

    int mCtrlWrEndPtAddr;
    int mCtrlRdEndPtAddr;
    int mStreamWrEndPtAddr;
    int mStreamRdEndPtAddr;

    uint32_t txSize;
    uint32_t rxSize;
#ifndef __unix__
    FT_HANDLE mFTHandle;
    int ReinitPipe(unsigned char ep);
#else
    int FT_SetStreamPipe(unsigned char ep, size_t size);
    int FT_FlushPipe(unsigned char ep);
    uint32_t mUsbCounter;
    libusb_device **devs; //pointer to pointer of device, used to retrieve a list of devices
    libusb_device_handle *dev_handle; //a device handle
    libusb_context *ctx; //a libusb session
#endif

    std::mutex mExtraUsbMutex;
};



class Connection_uLimeSDREntry : public ConnectionRegistryEntry
{
public:
    Connection_uLimeSDREntry(void);
    ~Connection_uLimeSDREntry(void);
    std::vector<ConnectionHandle> enumerate(const ConnectionHandle &hint);
    IConnection *make(const ConnectionHandle &handle);
private:
#ifndef __unix__
    FT_HANDLE* mFTHandle;
#else
    libusb_context *ctx; //a libusb session
    std::thread mUSBProcessingThread;
    void handle_libusb_events();
    std::atomic<bool> mProcessUSBEvents;
#endif
};

}
