<h1>Rigctrl plugin</h1>

<h2>Introduction</h2>

The rigctrl plugin allows SDRangel to be controlled via [Hamlib](http://hamlib.sourceforge.net/manuals/hamlib.html)'s rigctrld protocol. This allows other software that implements the rigctrld protocol, such at the satelite tracking software GPredict, to control SDRangel, to adjust for doppler or to automatically switch between different satellite frequencies and modes.

<h2>Interface</h2>

<h3>Enable rigctrl server</h3>

Checking this option will enable the rigctrl server in SDRangel. The default is disabled.

<h3>API Address</h3>

The rigctrl plugin using the SDRangel REST API to control SDRangel. Please specify the API address of the SDRangel instance to control. The default is http://127.0.0.1:8091.

<h3>Port</h3>

The rigctrl plugin opens a TCP port to receive commands from a rigctrl client on. Please specify a free TCP port number. The default rigctrld port is 4532.

<h3>Max Frequency Offset</h3>

The maximum frequency offset controls whether the center frequency or frequency offset is adjusted when a new frequency is received by a rigctrl command.
If the difference between the new frequency and the current center frequency is less than this value, the input offset (in the demodulator) will be adjusted.
If the difference is greater than this value, the center frequency will be set to the received frequency.
To only ever set the center frequency, set this value to 0. The default value is 10000.

<h3>Device Index</h3>

The device index specifies the SDRangel device set that will be controlled by received rigctrl commands. Defaults to 0.

<h3>Channel Index</h3>

The channel index specifies the SDRangel channel that will be controlled by received rigctrl commands. Defaults to 0.

<h2>Supported rigctrl Commands</h2>

The following rigctrl commands are supported:

<ul>
<li>F / set_freq
<li>f / get_freq
<li>M / set_mode
<li>get_powerstat
<li>set_powerstat
</ul>

<h2>Example rigctrl Session</h2>

Run SDRangel and from the Preferences menu select rigctrl. Check "Enable rigctrl server" and press OK.

In a terminal window, run:

<pre>
telnet localhost 4532
set_mode AM, 1000
set_freq 100000000
set_powerstat 1
</pre>
