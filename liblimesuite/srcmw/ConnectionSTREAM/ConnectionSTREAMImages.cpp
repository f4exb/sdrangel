/**
    @file ConnectionSTREAMImages.cpp
    @author Lime Microsystems
    @brief Image updating and version checking
*/

#include "ConnectionSTREAM.h"
#include "ErrorReporting.h"
#include "SystemResources.h"
#include "LMS64CProtocol.h"
#include "LMSBoards.h"
#include "Logger.h"
#include <fstream>
#include <ciso646>

using namespace lime;

/*!
 * The entry structure that describes a board revision and its fw/gw images
 */
struct ConnectionSTREAMImageEntry
{
    eLMS_DEV dev;
    int hw_rev;

    int fw_ver;
    const char *fw_img;

    int gw_ver;
    int gw_rev;
    const char *gw_rbf;
};

/*!
 * Lookup the board information given hardware type and revision.
 * Edit each entry for supported hardware and image updates.
 */
static const ConnectionSTREAMImageEntry &lookupImageEntry(const LMS64CProtocol::LMSinfo &info)
{
    static const std::vector<ConnectionSTREAMImageEntry> imageEntries = {
        ConnectionSTREAMImageEntry({LMS_DEV_UNKNOWN, -1, -1, "Unknown-USB.img", -1, -1, "Unknown-USB.rbf"}),
        ConnectionSTREAMImageEntry({LMS_DEV_LIMESDR, 4, 3, "LimeSDR-USB_HW_1.3_r3.0.img", 2, 8,  "LimeSDR-USB_HW_1.4_r2.8.rbf"}),
        ConnectionSTREAMImageEntry({LMS_DEV_LIMESDR, 3, 3, "LimeSDR-USB_HW_1.3_r3.0.img", 1, 20, "LimeSDR-USB_HW_1.1_r1.20.rbf"}),
        ConnectionSTREAMImageEntry({LMS_DEV_LIMESDR, 2, 3, "LimeSDR-USB_HW_1.2_r3.0.img", 1, 20, "LimeSDR-USB_HW_1.1_r1.20.rbf"}),
        ConnectionSTREAMImageEntry({LMS_DEV_LIMESDR, 1, 7, "LimeSDR-USB_HW_1.1_r7.0.img", 1, 20, "LimeSDR-USB_HW_1.1_r1.20.rbf"}),
        ConnectionSTREAMImageEntry({LMS_DEV_STREAM,  3, 8, "STREAM-USB_HW_1.1_r8.0.img",  1, 2,  "STREAM-USB_HW_1.3_r1.2.rbf"})};

    for(const auto &iter : imageEntries)
    {
        if (info.device == iter.dev and info.hardware == iter.hw_rev)
        {
            return iter;
        }
    }

    return imageEntries.front(); //the -1 unknown entry
}

void ConnectionSTREAM::VersionCheck(void)
{
    const auto info = this->GetInfo();
    const auto &entry = lookupImageEntry(info);

    //an entry match was not found
    if (entry.dev == LMS_DEV_UNKNOWN)
    {
        lime::error("Unsupported hardware connected: %s[HW=%d]", GetDeviceName(info.device), info.hardware);
        return;
    }

    //check and warn about firmware mismatch problems
    if (info.firmware != entry.fw_ver) lime::warning(
        "Firmware version mismatch!\n"
        "  Expected firmware version %d, but found version %d\n"
        "  Follow the FW and FPGA upgrade instructions:\n"
        "  http://wiki.myriadrf.org/Lime_Suite#Flashing_images\n"
        "  Or run update on the command line: LimeUtil --update\n",
        entry.fw_ver, info.firmware);

    //check and warn about gateware mismatch problems
    const auto fpgaInfo = this->GetFPGAInfo();
    if (fpgaInfo.gatewareVersion != entry.gw_ver
        || fpgaInfo.gatewareRevision != entry.gw_rev) lime::warning(
        "Gateware version mismatch!\n"
        "  Expected gateware version %d, revision %d\n"
        "  But found version %d, revision %d\n"
        "  Follow the FW and FPGA upgrade instructions:\n"
        "  http://wiki.myriadrf.org/Lime_Suite#Flashing_images\n"
        "  Or run update on the command line: LimeUtil --update\n",
        entry.gw_ver, entry.gw_rev, fpgaInfo.gatewareVersion, fpgaInfo.gatewareRevision);
}

static bool programmingCallbackStream(
    int bsent, int btotal, const char* progressMsg,
    const std::string &image,
    IConnection::ProgrammingCallback callback)
{
    const auto msg = std::string(progressMsg) + " (" + image + ")";
    return callback(bsent, btotal, msg.c_str());
}

int ConnectionSTREAM::ProgramUpdate(const bool download, IConnection::ProgrammingCallback callback)
{
    const auto info = this->GetInfo();
    const auto &entry = lookupImageEntry(info);

    //an entry match was not found
    if (entry.dev == LMS_DEV_UNKNOWN)
    {
        return lime::ReportError("Unsupported hardware connected: %s[HW=%d]", GetDeviceName(info.device), info.hardware);
    }

    //download images when missing
    if (download)
    {
        const std::vector<std::string> images = {entry.fw_img, entry.gw_rbf};
        for (const auto &image : images)
        {
            if (not lime::locateImageResource(image).empty()) continue;
            const std::string msg("Downloading: " + image);
            if (callback) callback(0, 1, msg.c_str());
            int ret = lime::downloadImageResource(image);
            if (ret != 0) return ret; //error set by download call
            if (callback) callback(1, 1, "Done!");
        }
    }

    //load firmware into flash
    {
        //open file
        std::ifstream file;
        const auto path = lime::locateImageResource(entry.fw_img);
        file.open(path.c_str(), std::ios::in | std::ios::binary);
        if (not file.good()) return lime::ReportError("Error opening %s", path.c_str());

        //read file
        std::streampos fileSize;
        file.seekg(0, std::ios::end);
        fileSize = file.tellg();
        file.seekg(0, std::ios::beg);
        std::vector<char> progData(fileSize, 0);
        file.read(progData.data(), fileSize);

        int device = LMS64CProtocol::FX3; //FX3
        int progMode = 2; //Firmware to FLASH
        using namespace std::placeholders;
        const auto cb = std::bind(&programmingCallbackStream, _1, _2, _3, path, callback);
        auto status = this->ProgramWrite(progData.data(), progData.size(), progMode, device, cb);
        if (status != 0) return status;
    }

    //load gateware into flash
    {
        //open file
        std::ifstream file;
        const auto path = lime::locateImageResource(entry.gw_rbf);
        file.open(path.c_str(), std::ios::in | std::ios::binary);
        if (not file.good()) return lime::ReportError("Error opening %s", path.c_str());

        //read file
        std::streampos fileSize;
        file.seekg(0, std::ios::end);
        fileSize = file.tellg();
        file.seekg(0, std::ios::beg);
        std::vector<char> progData(fileSize, 0);
        file.read(progData.data(), fileSize);

        int device = LMS64CProtocol::FPGA; //Altera FPGA
        int progMode = 1; //Bitstream to FLASH
        using namespace std::placeholders;
        const auto cb = std::bind(&programmingCallbackStream, _1, _2, _3, path, callback);
        auto status = this->ProgramWrite(progData.data(), progData.size(), progMode, device, cb);
        if (status != 0) return status;
    }

    //Reset FX3, FPGA should be reloaded on boot
    {
        int device = LMS64CProtocol::FX3; //FX3
        auto status = this->ProgramWrite(nullptr, 0, 0, device, nullptr);
        if (status != 0) return status;
    }

    return 0;
}
