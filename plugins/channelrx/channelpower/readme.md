<h1>Channel Power Plugin</h1>

<h2>Introduction</h2>

This plugin can be used to calculate channel power. Channel power is measured as average power, maximum peak power, minimum peak power as well as pulse average power (i.e. average power above a threshold) over a specified bandwidth.

<h2>Interface</h2>

The top and bottom bars of the channel window are described [here](../../../sdrgui/channel/readme.md)

![Channel power plugin GUI](../../../doc/img/ChannelPower_plugin_settings.png)

<h3>1: Frequency shift from center frequency of reception</h3>

Use the wheels to adjust the channel center frequency as a shift in Hz from the center frequency of reception. Left click on a digit sets the cursor position at this digit. Right click on a digit sets all digits on the right to zero. This effectively floors value at the digit position. Wheels are moved with the mousewheel while pointing at the wheel or by selecting the wheel with the left mouse click and using the keyboard arrows. Pressing shift simultaneously moves digit by 5 and pressing control moves it by 2.

<h3>2: BW - Channel Bandwidth</h3>

Bandwidth in Hz of the channel for which power is to be measured.

<h3>3: Tavg - Average Time</h3>

Time period overwhich the channel power is averaged. Values range from 10us to 10s in powers of 10. The available values depend upon the sample rate.

<h3>4: THp - Pulse Threshold</h3>

The pulse threshold sets the power in dB for which the channel power needs to exceed, in order to be included in the pulse average power measurement.

<h3>5: Avg - Average Power</h3>

Displays the most recent average power measurement in dB.

<h3>6: Max - Max Peak Power</h3>

Displays the maximum instantaneous peak power measurement in dB.

<h3>7: Min - Min Peak Power</h3>

Displays the minimum instantaneous peak power measurement in dB.

<h3>8: Pulse - Pulse Average Power</h3>

Displays the most recent pulse average power measurement in dB.

<h3>9: Clear Data</h3>

Clears current measurements (min and max values are reset).
