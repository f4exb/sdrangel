<h1>ATV modulator plugin</h1>

<h2>Introduction</h2>

This plugin can be used to generate an analog TV signal mostly used in amateur radio. It is limited to black and white images as only the luminance (256 levels) is supported. 

There is no sound either. You coud imagine using any of the plugins supporting audio to create a mixed signal. This is not working well however for various reasons. It is better to use two physical transmitters and two physical receivers.

In practice 4 MS/s is the lowest sample rate that produces a standard image quality. Lower sample rates down to 1 MS/s produce low quality images that may still be acceptable for experiments.

<h2>Interface</h2>

![ATV Modulator plugin GUI](../../../doc/img/ATVMod_plugin.png)

<h3>1: Frequency shift from center frequency of reception direction</h3>

The "+/-" button on the left side of the dial toggles between positive and negative shift.

<h3>2: Frequency shift from center frequency of reception value</h3>

Use the wheels to adjust the frequency shift in Hz from the center frequency of reception. Left click on a digit sets the cursor position at this digit. Right click on a digit sets all digits on the right to zero. This effectively floors value at the digit position.

<h3>3: Channel power</h3>

Average total power in dB relative to a &#177;1.0 amplitude signal generated in the pass band.

<h3>4: Channel mute</h3>

Use this button to toggle mute for this channel. The radio waves on the icon are toggled on (active) and off (muted) accordingly. Default is channel active.

<h3>5: Modulation type</h3>

The video signal can modulate the carrier in the following modes:

  - AM: Amplitude modulation. Modulation index is 90%.
  - FM: Frequency modulation. Excursion is half the bandwidth available given the channel sample rate. e.g. for 4 MS/s sample rate this is &#177;1 MHz (2 MHz total) 
  - USB: SSB upper side band: video signal is transposed only in positive frequencies including DC component
  - LSB: SSB lower side band: video signal is transposed only in megative frequencies excluding DC component
  
<h3>6: RF bandwidth</h3>

This is the bandwidth in MHz of the channel filter when the interpolator is engaged. If the sink sample rate is an integer multiple of the base rate that depends on the standard then there is no interpolation else the channel rate is set to the closest integer multiple of the base rate below the sink sample rate. 

The base rate depending on the standard is:

  - PAL625L: 1 MS/s
  - PAL525L: 1.008 MS/s

The modulating video signal sample rate is the same as the channel sample rate hence for example with a channel sample rate of 4 MS/s a video sample will last 1/4 = 0.25 &#956;s. In PAL625L a complete horizontal line lasts 64 &#956;s hence it will be composed of 64 &#215; 4 = 256 samples while the useful image line that lasts about 52 &#956;s will be composed of 52 &#215; 4 = 204 samples or points. 

When no interpolation and hence no soft RF filtering in the channel takes place then filtering relies on the hardware. The hardware filter bandwidth can be set in the sink plugin.

<h3>7: Video signal level meter</h3>

This is the level meter fed with the video signal. Units are the percentage of the 0.0 to 1.0 modulating video signal.

<h3>8: TV Standard</h3>

This is the analog TV standard in use. Use the checkbox to choose between:

  - PAL625L: this is the PAL 625 lines standard with 25 FPS. Since only black and white (luminance) is supported this corresponds to any of the B,G,I or L PAL standards
  - PAL525L: this is the PAL 525 lines standard with 30 FPS. This corresponds to the PAL M standard.
  
<h3>9: Input source</h3>

This combo box lets you choose between various inputs for the video signal:

  - Uniform: the image has a uniform luminance level given by the luminance level adjusted with the button next (10)
  - H Bars: horizontal bars from black level on the left to white level on the right
  - V Bars: vertical bars from black level on the top to white level on the bottom
  - Chess: chessboard pattern alternating black squares and squares with the luminance level adjusted with button (10)
  - H Grad: horizontal gradient from black level on the left to white level on the right
  - V Grad: vertical gradient from black level on the top to white level on the bottom
  - Image: still image read from the file selected with button (13). If no image is selected an uniform image is sent with the luminance adjusted with button (10)
  - Video: video file read from the file selected with button (14). If no image is selected an uniform image is sent with the luminance adjusted with button (10).  Buttons (15) and (16) control the play.
  - Camera: video signal from a webcam or supported video source connected to the system. If no source is selected an uniform image is sent with the luminance adjusted with button (10). Button (21) selects the camera source. Button (20) plays or stops the camera on a still image.
  
<h3>10. Luminance level</h3> 

This button lets you adjust the luminance level of the "blank" screens displays, the lighter squares on the chessboard and the overlay text.

<h3>11. Show overlay text toggle</h3>

This button toggles the display of an overlay text on a still image, or a video signal from a file or a camera. Note that for still images you will have to toggle off/on this button to take any change of text or text brightness into account.

<h3>12. Overlay text</h3>

In this textbox you can type the text that will be displayed as overlay when button (11) is engaged. The brightness level of the text is selected with the luminance adjust button (10). The number of characters is limited to 12.

<h3>13. Still picture file select</h3>

Clicking on this button will open a file dialog to let you choose an image file for still image display. When the dialog is closed and the choice is validated the name of the file will appear on the space at the right of the button. 

<h3>14. Video file select</h3>

Clicking on this button will open a file dialog to let you choose a video file for video play. When the dialog is closed and the choice is validated the name of the file will appear on the space at the right of the button. 

<h3>15. Play loop video</h3>

Use this button to toggle on/off playing the video file in a loop

<h3>16. Play/Pause video</h3>

Use this button to toggle on/off the video file play. When play stops the current image is displayed as a still image. When video is stopped the button is dark and a play (&#9658;) icon is displayed on the button. When video runs the button is lit and a pause (&#9611;&#9611;) icon is displayed on the button. 

<h3>17. Current video position</h3>

This is the current video file play position in time units relative to the start

<h3>18. Video file length</h3>

This is the video file play length in time units

<h3>19. Video file position slider</h3>

This slider can be used to randomly set the currennt position in the file when file play is in pause state (button 16). When video plays the slider moves according to the current position.

<h3>20. Play/Pause camera</h3>

Use this button to toggle on/off the camera play. When play stops the current image is displayed as a still image. When camera is stopped the button is dark and a play (&#9658;) icon is displayed on the button. When camera runs the button is lit and a pause (&#9611;&#9611;) icon is displayed on the button. 

On Linux systems when the play button is engaged for the first time the FPS of the camera is scanned which can take some time (100 frames are read). A message box appears while the operation is running.

<h3>21. Camera select</h3>

Use this combo to select the camera source when more than one is available. the number corresponds to the index of the camera during the camera scan at the startup of the plugin instance. A maximum of 4 cameras are scanned in whichever order presented by the system.  

<h3>22. Camera device number</h3>

This is the device number used by OpenCV which on Linux systems correspond to the /dev/video device number. i.e. "#0" for /dev/video0.

<h3>23. Camera FPS</h3>

This is the camera FPS. On Windows there is no dynamic FPS check and a fixed 5 FPS is set for each camera. On Linux 90% of the calculated FPS is divided by the number of scanned cameras to give the final FPS. This is an attempt to avoid congestion when multiple cameras are available however this was only tested with two cameras.

<h3>24. Camera image size</h3>

This is the width x height camera iamge size in pixels