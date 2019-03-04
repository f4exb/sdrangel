#!/bin/bash
sudo systemctl start ssh
sudo systemctl start dbus
sudo systemctl start avahi-daemon
/bin/bash
#/opt/install/sdrangel/bin/sdrangelsrv # not ready yet