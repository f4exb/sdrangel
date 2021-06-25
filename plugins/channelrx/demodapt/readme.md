<h1>APT Demodulator Plugin</h1>

<h2>Introduction</h2>

This plugin can be used to demodulate APT (Automatic Picture Transmission) signals transmitted by NOAA weather satellites. These images are at a 4km/pixel resolution in either the visible, near-IR, mid-IR or thermal-IR bands.

![APT Demodulator plugin GUI](../../../doc/img/APTDemod_plugin.png)

* NOAA 15 transmits on 137.620 MHz.
* NOAA 18 transmits on 137.912 MHz.
* NOAA 19 transmits on 137.100 MHz.

<h2>Interface</h2>

![APT Demodulator plugin GUI](../../../doc/img/APTDemod_plugin_settings.png)

<h3>1: Frequency shift from center frequency of reception</h3>

Use the wheels to adjust the frequency shift in Hz from the center frequency of reception. Left click on a digit sets the cursor position at this digit. Right click on a digit sets all digits on the right to zero. This effectively floors value at the digit position. Wheels are moved with the mousewheel while pointing at the wheel or by selecting the wheel with the left mouse click and using the keyboard arrows. Pressing shift simultaneously moves digit by 5 and pressing control moves it by 2.

<h3>2: Channel power</h3>

Average total power in dB relative to a +/- 1.0 amplitude signal received in the pass band.

<h3>3: Level meter in dB</h3>

  - top bar (green): average value
  - bottom bar (blue green): instantaneous peak value
  - tip vertical bar (bright green): peak hold value

<h3>4: RF Bandwidth</h3>

This specifies the bandwidth of a LPF that is applied to the input signal to limit the RF bandwidth. APT signals are nominally 34kHz wide, however, this defaults to 40kHz to allow for some Doppler shift.

<h3>5: Frequency deviation</h3>

Adjusts the expected frequency deviation in 0.1 kHz steps from 10 to 25 kHz. The typical value for APT is 17 kHz.

<h3>6: Start/stop decoding</h3>

Starts or stops decoding. A maximum of 3000 scanlines can be decoded, after which, the Reset Decoder (7) button needs to be pressed, to start a new image.

<h3>7: Show settings dialog</h3>

When clicked, shows additional APT Demodulator settings.

![APT Demodulator settings dialog](../../../doc/img/APTDemod_plugin_settingsdialog.png)

This includes:

   - Whether the APT demodulator can be controlled by the Satellite Tracker feature. When checked, the image decoder will be enabled and reset on AOS and the satellite pass direction will be used to control image rotation. The decoder will be stopped on LOS.
   - Which satellites the APT demodulator will respond to AOS and LOS indications from the Satellite Tracker. This can be used to simulataneously decode images from multiple satellites, by having multiple instances of the APT Demodulator and setting a unique satellite name for each demodulator.
   - Whether to automatically save the image on LOS.
   - Path to save automatically saved images in.
   - The minimum number of scanlines required to be in an image, after noise cropping, for it to be automatically saved.

<h3>8: Reset decoder</h3>

Clears the current image and restarts the decoder. The decoder must be reset between passes of different satellites.

<h3>9: Save image to disk</h3>

Saves the current image to disk. Images can be saved in PNG, JPEG, BMP, PPM, XBM or XPM formats.

<h3>10: Zoom in</h3>

Zooms in to the image. You can also zoom in with the scroll wheel.

<h3>11: Zoom out</h3>

Zooms out from the image. You can also zoom out with the scroll wheel.

<h3>12: Zoom image to fit</h3>

Zooms so that the image fits in to the available space.

<h3>13: Channel selection</h3>

Selects whether:

   - both channels are displayed
   - only channel A is displayed
   - only channel B is displayed

<h3>14: Crop noise</h3>

When checked, noise is cropped from the top and bottom of the image. This is noise that is typically the result of the satellite being at a low elevation.

<h3>15: Apply denoise filter</h3>

When checked, a denoise filter is applied to the received image.

<h3>16: Apply linear equalisation</h3>

When checked, linear equalisation is performed, which can enhance the contrast. The equalisation is performed separately on each channel.

<h3>17: Apply histogram equalisation</h3>

When checked, histogram equalisation is performed, which can enhance the contrast. The equalisation is performed separately on each channel.

<h3>18: Overlay precipitation</h3>

When checked, precipitation is detected from the IR channel and overlayed on both channels using a colour palette.

This option will not work if linear or histogram equalisation has been applied.

<h3>19: Pass direction</h3>

The pass direction check button should be set to match the direction of the satellite pass.
i.e. select down arrow for satellite passing from the North to the South and the up arrow for the satellite passing from the South to the North.
This will ensure the image has the Northern latitudes at the top of the image.
This can be set automatically by the Satellite Tracker feature.

<h2>Attribution</h2>

This plugin uses libapt, part of Aptdec by Thierry Leconte and Xerbo, to perform image decoding and processing: https://github.com/Xerbo/aptdec

Icons are by Freepik from Flaticon https://www.flaticon.com/

Icons are by Hare Krishna from the Noun Project Noun Project: https://thenounproject.com/
