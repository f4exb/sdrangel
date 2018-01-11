#!/bin/sh

cp 52-airspy.rules /etc/udev/rules.d/
cp 52-airspyhf.rules /etc/udev/rules.d/
cp 88-nuand.rules /etc/udev/rules.d/
cp fcd.rules /etc/udev/rules.d/
cp 53-hackrf.rules /etc/udev/rules.d/
cp 64-limesuite.rules /etc/udev/rules.d/
cp 53-adi-plutosdr-usb.rules /etc/udev/rules.d/
cp rtl-sdr.rules /etc/udev/rules.d/
cp mirisdr.rules /etc/udev/rules.d/

udevadm control --reload-rules
udevadm trigger
