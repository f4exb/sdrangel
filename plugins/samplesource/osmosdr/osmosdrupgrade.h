#ifndef INCLUDE_OSMOSDRUPGRADE_H
#define INCLUDE_OSMOSDRUPGRADE_H

#include <QDialog>
#include <QTimer>
#include "util/miniz.h"

typedef struct libusb_context libusb_context;
typedef struct libusb_device_handle libusb_device_handle;

namespace Ui {
	class OsmoSDRUpgrade;
}

class OsmoSDRUpgrade : public QDialog {
	Q_OBJECT

public:
	explicit OsmoSDRUpgrade(QWidget* parent = NULL);
	~OsmoSDRUpgrade();

private slots:
	void on_browse_clicked();
	void on_dfu_toggled(bool checked);
	void on_radio_toggled(bool checked);
	void on_fpga_toggled(bool checked);
	void on_start_clicked();
	void searchDeviceTick();

private:
	Ui::OsmoSDRUpgrade* ui;

	libusb_context* m_usb;

	QByteArray m_dfuApp;
	QByteArray m_radioApp;
	QByteArray m_fpgaBin;

	int m_searchTries;
	QTimer m_searchDeviceTimer;

	void updateFlashButton();

	void switchToDFU();
	int dfuGetState(libusb_device_handle* device, quint8* state);
	int dfuGetStatus(libusb_device_handle* device);
	int dfuClrStatus(libusb_device_handle* device);
	int dfuDownloadBlock(libusb_device_handle* device, quint16 block, const quint8* data, quint16 len);
	int dfuDetach(libusb_device_handle* device);

	void fail(const QString& msg);
	void finish();
	void log(const QString& msg);

	void reject();

	static size_t zipHelper(void* pOpaque, mz_uint64 file_ofs, const void* pBuf, size_t n);
};

#endif // INCLUDE_OSMOSDRUPGRADE_H
