#include <QFileDialog>
#include <QMessageBox>
#include <QCursor>
#include <QThread>
#include <libusb.h>
#include "osmosdrupgrade.h"
#include "ui_osmosdrupgrade.h"

#define OSMOSDR_USB_VID 0x16c0
#define OSMOSDR_USB_PID 0x0763

// DFU commands
#define DFU_DETACH              0x00
#define DFU_DNLOAD              0x01
#define DFU_UPLOAD              0x02
#define DFU_GETSTATUS           0x03
#define DFU_CLRSTATUS           0x04
#define DFU_GETSTATE            0x05
#define DFU_ABORT               0x06

// DFU states
#define ST_appIDLE              0
#define ST_appDETACH            1
#define ST_dfuIDLE              2
#define ST_dfuDNLOAD_SYNC       3
#define ST_dfuDNBUSY            4
#define ST_dfuDNLOAD_IDLE       5
#define ST_dfuMANIFEST_SYNC     6
#define ST_dfuMANIFEST          7
#define ST_dfuMANIFEST_WAIT_RST 8
#define ST_dfuUPLOAD_IDLE       9
#define ST_dfuERROR             10

#define DFU_PACKETSIZE 512

#pragma pack(push, 1)
struct dfu_getstatus {
	quint8 bStatus;
	quint8 bwPollTimeout[3];
	quint8 bState;
	quint8 iString;
};
#pragma pack(pop)


OsmoSDRUpgrade::OsmoSDRUpgrade(QWidget* parent) :
	QDialog(parent),
	ui(new Ui::OsmoSDRUpgrade),
	m_usb(NULL)
{
	ui->setupUi(this);
	connect(&m_searchDeviceTimer, SIGNAL(timeout()), this, SLOT(searchDeviceTick()));
}

OsmoSDRUpgrade::~OsmoSDRUpgrade()
{
	delete ui;
}

void OsmoSDRUpgrade::on_browse_clicked()
{
	QString filename = QFileDialog::getOpenFileName(this, tr("OsmoSDR Firmware File"), QString(), tr("OsmoSDR Firmware (*.ofw);;All Files (*)"));
	if(filename.isEmpty())
		return;

	QFile file(filename);
	if(file.size() > 10 * 1024 * 1024) {
		QMessageBox::critical(this, tr("File Open Error"), tr("File is too big to be a firmware update for OsmoSDR."));
		return;
	}

	QByteArray dfuApp;
	QByteArray radioApp;
	QByteArray fpgaBin;
	mz_zip_archive zip;
	memset(&zip, 0x00, sizeof(zip));

	if(!mz_zip_reader_init_file(&zip, qPrintable(filename), 0)) {
		mz_zip_reader_end(&zip);
		QMessageBox::critical(this, tr("File Open Error"), tr("File is not a valid OsmoSDR update."));
		return;
	}

	int dfuAppIndex = mz_zip_reader_locate_file(&zip, "dfuapp.bin", NULL, 0);
	int radioAppIndex = mz_zip_reader_locate_file(&zip, "radioapp.bin", NULL, 0);
	int fpgaIndex = mz_zip_reader_locate_file(&zip, "fpga.bin", NULL, 0);

	if((dfuAppIndex < 0) && (radioAppIndex < 0) && (fpgaIndex < 0)) {
		mz_zip_reader_end(&zip);
		QMessageBox::critical(this, tr("File Open Error"), tr("File is not a valid OsmoSDR update."));
		return;
	}

	if(!mz_zip_reader_extract_to_callback(&zip, dfuAppIndex, zipHelper, &dfuApp, 0)) {
		mz_zip_reader_end(&zip);
		QMessageBox::critical(this, tr("File Open Error"), tr("File is not a valid OsmoSDR update."));
		return;
	}

	if(!mz_zip_reader_extract_to_callback(&zip, radioAppIndex, zipHelper, &radioApp, 0)) {
		mz_zip_reader_end(&zip);
		QMessageBox::critical(this, tr("File Open Error"), tr("File is not a valid OsmoSDR update."));
		return;
	}

	if(!mz_zip_reader_extract_to_callback(&zip, fpgaIndex, zipHelper, &fpgaBin, 0)) {
		mz_zip_reader_end(&zip);
		QMessageBox::critical(this, tr("File Open Error"), tr("File is not a valid OsmoSDR update."));
		return;
	}

	mz_zip_reader_end(&zip);
	m_dfuApp = dfuApp;
	m_radioApp = radioApp;
	m_fpgaBin = fpgaBin;

	if(dfuAppIndex >= 0) {
		ui->dfu->setEnabled(true);
		ui->dfuSize->setText(tr("%1").arg(m_dfuApp.size()));
	} else {
		ui->dfu->setEnabled(false);
		ui->dfuSize->setText(tr("-"));
	}
	if(radioAppIndex >= 0) {
		ui->radio->setEnabled(true);
		ui->radioSize->setText(tr("%1").arg(m_radioApp.size()));
	} else {
		ui->radio->setEnabled(false);
		ui->dfuSize->setText(tr("-"));
	}
	if(fpgaIndex >= 0) {
		ui->fpga->setEnabled(true);
		ui->fpgaSize->setText(tr("%1").arg(m_fpgaBin.size()));
	} else {
		ui->fpga->setEnabled(false);
		ui->fpgaSize->setText(tr("-"));
	}


	if((dfuAppIndex >= 0) && (radioAppIndex < 0) && (fpgaIndex < 0)) {
		ui->dfu->setChecked(true);
		ui->radio->setChecked(false);
		ui->fpga->setChecked(false);
	} else {
		ui->dfu->setChecked(false);
		ui->radio->setChecked(radioAppIndex >= 0);
		ui->fpga->setChecked(fpgaIndex >= 0);
	}

	ui->fwBox->setEnabled(true);
	ui->progressBox->setEnabled(true);

	QFileInfo fi(file.fileName());
	ui->filename->setText(fi.fileName());
}

void OsmoSDRUpgrade::on_dfu_toggled(bool checked)
{
	updateFlashButton();
}

void OsmoSDRUpgrade::on_radio_toggled(bool checked)
{
	updateFlashButton();
}

void OsmoSDRUpgrade::on_fpga_toggled(bool checked)
{
	updateFlashButton();
}

void OsmoSDRUpgrade::updateFlashButton()
{
	if(ui->dfu->isChecked() || ui->radio->isChecked() || ui->fpga->isChecked())
		ui->progressBox->setEnabled(true);
	else ui->progressBox->setEnabled(false);
}

void OsmoSDRUpgrade::on_start_clicked()
{
	ui->close->setEnabled(false);
	ui->start->setEnabled(false);
	QApplication::setOverrideCursor(Qt::WaitCursor);

	ui->log->clear();
	log(tr("Starting flash operation..."));

	if(libusb_init(&m_usb) < 0) {
		fail(tr("Could not initialize libusb."));
		return;
	}

	switchToDFU();

	m_searchTries = 0;
	m_searchDeviceTimer.start(250);
}

void OsmoSDRUpgrade::searchDeviceTick()
{
	m_searchTries++;
	log(tr("Searching device (try %1)").arg(m_searchTries));

	libusb_device_handle* device = libusb_open_device_with_vid_pid(m_usb, OSMOSDR_USB_VID, OSMOSDR_USB_PID);
	if(device == NULL) {
		if(m_searchTries >= 10) {
			m_searchDeviceTimer.stop();
			finish();
		}
		return;
	}

	m_searchDeviceTimer.stop();

	libusb_device_descriptor deviceDescriptor;

	if(libusb_get_descriptor(device, LIBUSB_DT_DEVICE, 0, (unsigned char*)&deviceDescriptor, sizeof(deviceDescriptor)) < 0) {
		libusb_close(device);
		fail(tr("Could not read device descriptor."));
		return;
	}

	char sn[64];
	memset(sn, 0x00, sizeof(sn));
	libusb_get_string_descriptor_ascii(device, deviceDescriptor.iSerialNumber, (unsigned char*)sn, sizeof(sn));
	sn[sizeof(sn) - 1] = '\0';

	log(tr("OsmoSDR found (SN %1)").arg(sn));

	quint8 buffer[255];
	if(libusb_get_descriptor(device, LIBUSB_DT_CONFIG, 0, buffer, sizeof(buffer)) < 16) {
		libusb_close(device);
		fail(tr("Could not get a configuration descriptor"));
		return;
	}

	if(buffer[15] != 1) {
		libusb_close(device);
		fail(tr("Device not in DFU mode as it should be"));
		return;
	}

	quint8 state;

	if(dfuGetState(device, &state) < 0) {
		libusb_reset_device(device);
		if(dfuGetState(device, &state) < 0) {
			libusb_close(device);
			fail(tr("Cannot get device DFU state"));
			return;
		}
	}
	if((state != ST_dfuIDLE) && (state != ST_dfuDNLOAD_IDLE)) {
		dfuGetStatus(device);
		libusb_close(device);
		fail(tr("Device is not in dfuIDLE or dfuDNLOAD_IDLE state"));
		return;
	}

	if(libusb_claim_interface(device, 0) < 0) {
		libusb_close(device);
		fail(tr("Could not claim interface"));
		return;
	}

	QByteArray* image;
	int altSetting;

	while(true) {
		if(ui->dfu->isChecked()) {
			log(tr("Starting download of DFU application"));
			image = &m_dfuApp;
			altSetting = 1;
		} else if(ui->radio->isChecked()) {
			log(tr("Starting download of radio application"));
			image = &m_radioApp;
			altSetting = 0;
		} else if(ui->fpga->isChecked()) {
			log(tr("Starting download of FPGA image"));
			image = &m_fpgaBin;
			altSetting = 2;
		} else {
			log(tr("Switching back to radio application mode"));
			dfuDetach(device);
			break;
		}

		if(libusb_set_interface_alt_setting(device, 0, altSetting) < 0) {
			libusb_close(device);
			fail(tr("Could not set alternate interface setting to %1").arg(altSetting));
			return;
		}

		ui->progress->setMaximum((image->size() / DFU_PACKETSIZE) + ((image->size() % DFU_PACKETSIZE) ? 1 : 0));

		int blocknum = 0;
		while((state == ST_dfuIDLE) || (state == ST_dfuDNLOAD_IDLE)) {
			int offset = blocknum * DFU_PACKETSIZE;
			int blocklen = (int)image->size() - offset;
			if(blocklen < 0)
				blocklen = 0;
			if(blocklen > DFU_PACKETSIZE)
				blocklen = DFU_PACKETSIZE;
			if(dfuDownloadBlock(device, blocknum, (const quint8*)(image->data() + offset), blocklen) < 0) {
				if(dfuGetState(device, &state) < 0) {
					if(ui->dfu->isChecked())
						break;
					libusb_close(device);
					fail(tr("Cannot get device DFU state"));
					return;
				}
				dfuGetStatus(device);
				libusb_close(device);
				fail(tr("Error downloading block %1").arg(blocknum));
				return;
			}
			dfuGetStatus(device);
			if(blocklen == 0)
				break;
			blocknum++;
			ui->progress->setValue(blocknum);
			QApplication::processEvents();
		}
		log(tr("Download complete (state %1)").arg(state));

		do {
			if(dfuGetState(device, &state) < 0) {
				if(ui->dfu->isChecked()) {
					break;
				} else {
					libusb_close(device);
					fail(tr("Cannot get device DFU state"));
					return;
				}
			}
			dfuGetStatus(device);
		} while(state == ST_dfuMANIFEST);

		if(ui->dfu->isChecked()) {
			ui->dfu->setChecked(false);
			m_searchTries = 0;
			m_searchDeviceTimer.start(250);
			libusb_close(device);
			return;
		} else if(ui->radio->isChecked()) {
			ui->radio->setChecked(false);
		} else if(ui->fpga->isChecked()) {
			ui->fpga->setChecked(false);
		}

		QApplication::processEvents();
	}

	log(tr("Upgrade finished successfully"));
	libusb_close(device);
	finish();
}

void OsmoSDRUpgrade::switchToDFU()
{
	libusb_device_handle* device = libusb_open_device_with_vid_pid(m_usb, OSMOSDR_USB_VID, OSMOSDR_USB_PID);
	if(device == NULL) {
		log(tr("No OsmoSDR VID:PID %1:%2 found").arg(OSMOSDR_USB_VID, 4, 16, QChar('0')).arg(OSMOSDR_USB_PID, 4, 16, QChar('0')));
		return;
	}

	quint8 buffer[255];
	if(libusb_get_descriptor(device, LIBUSB_DT_CONFIG, 0, buffer, sizeof(buffer)) < 16) {
		log(tr("Could not get a valid configuration descriptor"));
		libusb_close(device);
		return;
	}

	if(buffer[15] != 1) {
		if(libusb_claim_interface(device, 0) < 0) {
			log(tr("Could not claim interface"));
			libusb_close(device);
			return;
		}

		log(tr("Switching device to DFU mode"));
		libusb_control_transfer(device, LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_OUT, 0x07, 0x0003, 0, NULL, 0, 50);
		libusb_release_interface(device, 0);
	} else {
		log(tr("Device is already in DFU mode"));
	}

	libusb_close(device);
}

int OsmoSDRUpgrade::dfuGetState(libusb_device_handle* device, quint8* state)
{
	int res;

	res = libusb_control_transfer(
		device,
		0xa1,
		DFU_GETSTATE,
		0,
		0,
		(unsigned char*)state,
		1,
		1000);

	return res;
}

int OsmoSDRUpgrade::dfuGetStatus(libusb_device_handle* device)
{
	int res;
	struct dfu_getstatus getstatus;
/*
	static const char* statedesc[] = {
		"appIDLE",
		"appDETACH",
		"dfuIDLE",
		"dfuDNLOAD-SYNC",
		"dfuDNBUSY",
		"dfuDNLOAD-IDLE",
		"dfuMANIFEST-SYNC",
		"dfuMANIFEST",
		"dfuMANIFEST-WAIT-RESET",
		"dfuUPLOAD-IDLE",
		"dfuERROR"
	};
*/

	res = libusb_control_transfer(
		device,
		0xa1,
		DFU_GETSTATUS,
		0,
		0,
		(unsigned char*)&getstatus,
		sizeof(getstatus),
		1000);
/*
	if(res >= 0)
		log(tr("OsmoSDR bStatus: %1, bState: %2 (%3)").arg(getstatus.bStatus).arg(statedesc[getstatus.bState]).arg(getstatus.bState));
*/
	return res;
}

int OsmoSDRUpgrade::dfuClrStatus(libusb_device_handle* device)
{
	return libusb_control_transfer(
		device,
		0x21,
		DFU_CLRSTATUS,
		0,
		0,
		NULL,
		0,
		1000);
}

int OsmoSDRUpgrade::dfuDownloadBlock(libusb_device_handle* device, quint16 block, const quint8* data, quint16 len)
{
	return libusb_control_transfer(
		device,
		0x21,
		DFU_DNLOAD,
		block,
		0,
		(unsigned char*)data,
		len,
		10000);
}

int OsmoSDRUpgrade::dfuDetach(libusb_device_handle* device)
{
	return libusb_control_transfer(
		device,
		0x21,
		DFU_DETACH,
		0,
		0,
		NULL,
		0,
		1000);
}

void OsmoSDRUpgrade::fail(const QString& msg)
{
	log(tr("Fatal error: %1").arg(msg));
	QMessageBox::critical(this, tr("OsmoSDR Upgrade Failed"), msg);
	finish();
}

void OsmoSDRUpgrade::finish()
{
	if(m_usb != NULL) {
		libusb_exit(m_usb);
		m_usb = NULL;
	}
	QApplication::restoreOverrideCursor();
	ui->start->setEnabled(true);
	ui->close->setEnabled(true);
}

void OsmoSDRUpgrade::log(const QString& msg)
{
	ui->log->appendPlainText(msg);
	ui->log->moveCursor(QTextCursor::End);
}

void OsmoSDRUpgrade::reject()
{
	if(ui->close->isEnabled())
		return QDialog::reject();
}

size_t OsmoSDRUpgrade::zipHelper(void* pOpaque, mz_uint64 file_ofs, const void* pBuf, size_t n)
{
	QByteArray* bytes = (QByteArray*)pOpaque;
	bytes->append((const char*)pBuf, n);
	return n;
}
