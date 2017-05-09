/**
    @file ConnectionSTREAM.h
    @author Lime Microsystems
    @brief Implementation of STREAM board connection.
*/

#pragma once
#include <ConnectionRegistry.h>
#include <IConnection.h>
#include <ILimeSDRStreaming.h>
#include <vector>
#include <set>
#include <string>
#include <atomic>
#include <memory>
#include <thread>
#include "fifo.h"
#include <ciso646>

#define __unix__
#include "windows.h"

#ifndef __unix__
#include "windows.h"
#include "CyAPI.h"
#else
#include <libusb-1.0/libusb.h>
#include <mutex>
#include <condition_variable>
#include <chrono>
#endif

namespace lime
{

#define USB_MAX_CONTEXTS 64 //maximum number of contexts for asynchronous transfers

/** @brief Wrapper class for holding USB asynchronous transfers contexts
*/
class USBTransferContext
{
public:
    USBTransferContext() : used(false)
    {
        id = idCounter++;
#ifndef __unix__
        inOvLap = new OVERLAPPED;
        memset(inOvLap, 0, sizeof(OVERLAPPED));
        inOvLap->hEvent = CreateEvent(NULL, false, false, NULL);
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
#ifndef __unix__
        CloseHandle(inOvLap->hEvent);
        delete inOvLap;
#else
        libusb_free_transfer(transfer);
#endif
    }
    bool reset()
    {
        if(used)
            return false;
#ifndef __unix__
        CloseHandle(inOvLap->hEvent);
        memset(inOvLap, 0, sizeof(OVERLAPPED));
        inOvLap->hEvent = CreateEvent(NULL, false, false, NULL);
#endif
        return true;
    }
    bool used;
    int id;
    static int idCounter;
#ifndef __unix__
    PUCHAR context;
    OVERLAPPED* inOvLap;
#else
    libusb_transfer* transfer;
    long bytesXfered;
    long bytesExpected;
    std::atomic<bool> done;
    std::mutex transferLock;
    std::condition_variable cv;
#endif
};

class ConnectionSTREAM : public ILimeSDRStreaming
{
public:
    ConnectionSTREAM(void* arg, const std::string &vidpid, const std::string &serial, const unsigned index);
    ~ConnectionSTREAM(void);
    void VersionCheck(void);

    int Open(const std::string &vidpid, const std::string &serial, const unsigned index);
    void Close();
    bool IsOpen();
    int GetOpenedIndex();

    virtual int Write(const unsigned char* buffer, int length, int timeout_ms = 100) override;
    virtual int Read(unsigned char* buffer, int length, int timeout_ms = 100) override;

    virtual int UploadWFM(const void* const* samples, uint8_t chCount, size_t sample_count, StreamConfig::StreamDataFormat format) override;

    //hooks to update FPGA plls when baseband interface data rate is changed
    virtual int UpdateExternalDataRate(const size_t channel, const double txRate, const double rxRate) override;
    virtual int UpdateExternalDataRate(const size_t channel, const double txRate, const double rxRate, const double txPhase, const double rxPhase) override;
    virtual int ProgramWrite(const char *buffer, const size_t length, const int programmingMode, const int device, ProgrammingCallback callback) override;
    int ProgramUpdate(const bool download, ProgrammingCallback callback);
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

    double DetectRefClk(void);

    USBTransferContext contexts[USB_MAX_CONTEXTS];
    USBTransferContext contextsToSend[USB_MAX_CONTEXTS];

    bool isConnected;

#ifndef __unix__
    CCyFX3Device* USBDevicePrimary;
    //control endpoints
    CCyControlEndPoint* InCtrlEndPt3;
    CCyControlEndPoint* OutCtrlEndPt3;

    //end points for samples reading and writing
    CCyUSBEndPoint* InEndPt;
    CCyUSBEndPoint* OutEndPt;

    CCyUSBEndPoint* InCtrlBulkEndPt;
    CCyUSBEndPoint* OutCtrlBulkEndPt;
#else
    libusb_device_handle* dev_handle; //a device handle
    libusb_context* ctx; //a libusb session
    int read_firmware_image(unsigned char *buf, int len);
    int fx3_usbboot_download(unsigned char *buf, int len);
    int ram_write(unsigned char *buf, unsigned int ramAddress, int len);
#endif
    static const uint8_t streamBulkOutAddr;
    static const uint8_t streamBulkInAddr;
    static const uint8_t ctrlBulkOutAddr;
    static const uint8_t ctrlBulkInAddr;
    static const std::set<uint8_t> commandsToBulkCtrlHw1;
    static const std::set<uint8_t> commandsToBulkCtrlHw2;
    std::set<uint8_t> commandsToBulkCtrl;
    bool bulkCtrlInProgress;
    bool bulkCtrlAvailable;
    std::mutex mExtraUsbMutex;
};

class ConnectionSTREAMEntry : public ConnectionRegistryEntry
{
public:
    ConnectionSTREAMEntry(void);
    ConnectionSTREAMEntry(const std::string entryName);
    virtual ~ConnectionSTREAMEntry(void);
    virtual std::vector<ConnectionHandle> enumerate(const ConnectionHandle& hint);
    virtual IConnection* make(const ConnectionHandle& handle);
protected:
#ifndef __unix__
    std::string DeviceName(unsigned int index);
    void *ctx; //not used, just for mirroring unix
#else
    libusb_context* ctx; //a libusb session
    std::thread mUSBProcessingThread;
    void handle_libusb_events();
    std::atomic<bool> mProcessUSBEvents;
#endif
};

}
