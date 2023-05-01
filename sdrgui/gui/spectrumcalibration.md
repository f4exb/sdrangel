<h1>Spectrum calibration management</h1>

The spectrum calibration is controlled by this dialog. It manages the list of calibration points (calibration chart), general options and the import/export from/to a .csv file.

This calibration is an artifact of the spectrum display it does not make any change in the actual levels in the DSP processing. It assumes nothing about the receiving or transmitting chains it is up to the user to run the calibration procedure to make the face power levels match the desired levels. If anything is changed in the receiving or transmitting parameters then the calibration procedure may have to be re-run again. Also it assumes nothing about the units of the calibrated power. Normally one would like to make dBm (or mW) measurements but it is up to the user to assume the actual units. One may want to use dBW or any other custom units therefore the displayed calibrated values will remain unit-less (dB).

For frequencies between calibration points the shift from relative value to displayed calibration value is interpolated using the next closest calibration points. If only one calibration point is provided then it applies regardless of frequency. The retained frequency for calculation is the center frequency of the spectrum and the shift applied is constant over the spectrum span. When no calibration points are provided turning on calibration correction has no effect.

<h2>Interface description</h2>

![Spectrum calibration dialog](../doc/img/Spectrum_Calibration_dialog.png)

<h3>1. Select point</h3>

Use the rotating button to select the point to edit

<h3>2. Add/Delete point</h3>

Click on the "+" button to add a new point and "-" to remove the current point.

<h3>3. Relative power</h3>

Enter the relative power in dB. This is the power displayed without calibration at this frequency point.

<h3>4. Calibrated power</h3>

Enter the calibrated power in dB. This is the power displayed with calibration at this frequency point.

<h3>5. Point frequency</h3>

This is the frequency of the calibration point

<h3>6. Point controls</h3>

![Spectrum calibration buttons](../doc/img/Spectrum_Calibration_buttons.png)

<h4>6.1. Sort points</h4>

Sort points by increasing frequency

<h4>6.2. Duplicate point</h4>

Append a new point to the list as a copy of the current point. The editor is automatically transferred to the new point.

<h4>6.3. Import from spectrum marker</h4>

Import data (frequency and relative power) from the first histogram marker (marker 0) if it exists. If there are no histogram markers this just does nothing.

<h4>6.4. Set frequency to center</h4>

Sets the point frequency to the center frequency of the spectrum

<h3>7. Interpolation method</h3>

The calibration shift between calibration points is interpolated based on the relative distance in frequency between points. The applied calibration shift is interpolated from the shift of the neighboring points. The interpolation can be:

  - **Linear**: interpolation is linear in terms of power
  - **Log**: interpolation is logarithmic in terms of power or linear if the power is expressed in dB

<h3>8. Apply correction or set value</h3>

This button lets you choose how the constant power next (9) is applied to all points

  - **Cor**: applies a correction i.e. the value in (9) is added (in dB) to all points
  - **Set**: sets power of all point to the value in (9)

The correction allows to take roughly into account the reception or transmission gain changes. It is like applying a constant gain over all frequencies.

You may apply correction to either the relative powers or the calibrated powers. The choice depends on whether you are in a reception or transmission chain (source or sink device set respectively).

In reception you would apply it to the relative powers as the input power that you measure for calibration remains the same.

In transmission you would apply it to the calibrated powers as the output power that you measure for calibration actually changes.

<h3>9. Constant power value in dB</h3>

This value is used to set all points to or to be applied as a correction depending on (8). It is used for relative powers (10) or calibrated powers (11)

<h3>10. Set or apply correction to relative powers</h3>

All relative power values are set to value in (9) if "Set" is selected in (8).

The value in (9) is added (in dB) to all relative power values if "Cor" is selected in (8)

<h3>11. Set or apply correction to calibrated powers</h3>

All calibrated power values are set to value in (9) if "Set" is selected in (8).

The value in (9) is added (in dB) to all calibrated power values if "Cor" is selected in (8)

<h3>12. Export calibrated points to .csv</h3>

Export the calibrated points to a .csv file. Columns are:

  - **Frequency**: frequency in Hz of the point
  - **Relative**: relative power in dB
  - **Calibrated**: calibrated power in dB

<h3>13. Import calibrated points from .csv file</h3>

Import the calibrated points from a .csv file in the format described above.
