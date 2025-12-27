<h1>Aaronia Spectran V6 and V6 Eco using SDK plugin</h1>

<h2>Introduction</h2>

This plugin makes it possible to use an Aaronia Spectran V6 or V6 Eco connecting directly to the PC via the USB cables. It uses the SDK developed by Aaronia that you will need to download and install in your system to be able to compile and use this plugin. As there is not distribution of this plugin you will have to compile it from source.

This plugin is marked as a "MISO" meaning it supports multiple inputs i.e. receivers (Spectran V6) and single output i.e. transmitter (both models) in the same plugin. When connecting a V6 Eco the second receiver is simply disabled. There are also a few other restrictions with the V6 Eco vs the V6 that will be covered in the relevant sections of the interface description next.

<h2>Compilation</h2>

<h2>Interface</h2>

![Spectran MISO plugin](../../../doc/img/SpectranMISOPlugin.png)

The top and bottom bars of the device window are described [here](../../../sdrgui/device/readme.md)

<h3>1: Start/Stop</h3>

Device start / stop button.

  - Blue triangle icon: device is ready and can be started
  - Green square icon: device is running and can be stopped

<h3>2: Baseband sample rate</h3>

This is the sample rate and thus bandwidth available in the baseband after any processing.

<h3>3: Processing modes</h3>

Selects the processing mode. With the Aaronia SDK there are two essential processing modes IQ and raw.

The IQ mode is downsampling or upsampling a chunk of the bandwidth allowed by the main clock setting (8) at the sample rate provided in (10). It acts similarly to the local sink or local source channel plugins in SDRangel but via the SDK. It supports the single receiver and transmitter modes.

The Raw mode directly dumps the full bandwidth provided by the main clock and works in receive mode only and can also support dual receivers.

Below are the options in the combo box:

  - **Rx IQ**: Single receiver in IQ mode
  - **Tx IQ**: Transmitter only
  - **Rx+Tx IQ**: Transceiver is not supported yet
  - **Rx Raw**: Single receiver in Raw mode
  - **Rx 1+2 I**: Dual receivers in Raw mode with interleaved samples
  - **Rx 1+2 Raw**: Dual receivers in Raw mode with non-interleaved samples

<h3>4. Select receiver</h3>

For dual receiver devices and single receiver mode this control allows selection between receivers

<h3>5. Select spectrum to display</h3>

This control allows to select the source of the spectrum display:

  - **Rx1**: Shows spectrum from the first receiver
  - **Rx2**: Shows spectrum from the second receiver
  - **Tx**: Shows spectrum from the transmitter

<h3>6. Rx frequency</h3>

Sets the frequency of the receiver or receivers. When in dual receive modes the frequency of both receivers is the same as they are coherent.

<h3>7. Tx frequency</h3>

Sets the frequency of the transmitter. It is independent of the Rx frequency within the limit of the common frequency span between receiver and transmitter sides which is fixed at 50 MHz. Practically it means that the Tx frequency cannot be further away than 25 MHz from the receiver frequency on any side also limited by the transmitter bandwidth so that it is actually the far side of the bandwidth that can be further away than 25 MHz from the receiver center frequency.

Another limitation is that upon restart the Tx frequency is reset to the Rx frequency.

<h3>8. Clock rate</h3>

This is the device main clock rate that is in effect in raw mode. For the Eco mode the clock is fixed at 61.44 MHz therefore this control is disabled. In IQ mode it limits the actual sample rate (10)

For the V6 available clock rates are 92, 122 and 245 MHz. Other frequencies may be model dependant and are not supported. Yes it means you can see up to 245 MHz of spectrum.

<h3>9. Decimation in raw mode</h3>

This is available only in raw mode. The full bandwidth conditioned by the main clock rate (8) can be decimated within the SDK down to a factor of 512 by powers of two.

<h3>10. Sample rate in IQ mode</h3>

This is the sample rate in IQ mode for the receivers or the transmitter. For the transmitter it is limited from 48 kS/s to 20 MS/s.

<h3>11. Tx drive factor</h3>

This is the Tx drive factor in dB referenced to the full power (0 dB) and goes down to -50dB in 1dB steps.
