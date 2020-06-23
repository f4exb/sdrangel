<h1>Transverter dialog</h1>

This button opens a dialog to set the transverter mode frequency translation options:

![Transverter dialog](../../doc/img/TransverterDialog.png)

Note that if you mouse over the button a tooltip appears that displays the translating frequency and if translation is enabled or disabled. When the frequency translation is enabled the button is lit.

<h3>1: Translating frequency</h3>

You can set the translating frequency in Hz with this dial. Use the wheels to adjust the sample rate. Left click on a digit sets the cursor position at this digit. Right click on a digit sets all digits on the right to zero. This effectively floors value at the digit position. Wheels are moved with the mousewheel while pointing at the wheel or by selecting the wheel with the left mouse click and using the keyboard arrows. Pressing shift simultaneously moves digit by 5 and pressing control moves it by 2.

The frequency set in the device is the frequency on the main dial (1) minus this frequency. Thus it is positive for down converters and negative for up converters.

For example with the DX Patrol that has a mixer at 120 MHz for HF operation you would set the value to -120,000,000 Hz so that if the main dial frequency is set at 7,130 kHz the RTLSDR of the DX Patrol will be set to 127.130 MHz.

If you use a down converter to receive the 6 cm band narrowband center frequency of 5670 MHz at 432 MHz you would set the translating frequency to 5760 - 432 = 5328 MHz thus dial +5,328,000,000 Hz.

For bands even higher in the frequency spectrum the GHz digits are not really significant so you can have them set at 1 GHz. Thus to receive the 10368 MHz frequency at 432 MHz you would set the translating frequency to 1368 - 432 = 936 MHz. Note that in this case the frequency of the LO used in the mixer of the transverter is set at 9936 MHz.

The Hz precision allows a fine tuning of the transverter LO offset

<h3>2: Translating frequency enable/disable</h3>

Use this toggle button to activate or deactivate the frequency translation

<h3>3: I/Q samples order</h3>

Use this button to swap the order of I and Q samples. Normally in the I/Q interleaved stream coming from the device the I (real) component of the complex sample comes first. When I and Q samples are swapped the spectrum is reversed. This is useful when the transverter has a high side LO.

<h3>4 Confirmation buttons</h3>

Use these buttons to confirm ("OK") or dismiss ("Cancel") your changes.