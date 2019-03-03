#!/bin/bash

sudo service ssh start
sudo service dbus start
sudo service avahi-daemon start
/opt/install/sdrangel/bin/sdrangel
