<h1>LimeRFE USB controller</h1>

<h2>Introduction</h2>

This plugin supports the [LimeRFE](https://github.com/myriadrf/LimeRFE) board. This board hosts a hardware power amplifier (PA) module with appropriate filtering and support circuitry to augment primarily but not only the LimeSDR, LimeSDR Mini, and LimeNET Micro platforms, providing a complete solution that addresses real life applications ranging from HAM radio to standards-compliant cellular network implementations.

As mentioned above it can be connected to a wide variety of SDR receivers and transmitters and this feature supports the synchronization with any receiver or transmitter device sets.

This plugin depends on [LimeSuite](https://github.com/myriadrf/LimeSuite) that should be available in your system in order to be compiled.

<h2>Interface</h2>

![LimeRFE controller GUI](../../../doc/img/LimeRFE_plugin.png)

When starting you need first to open the LimeRFE device with button (2). You have to select the appropriate serial device from (1). Note that all serial USB based devices are listed. You need to identify which one corresponds to the LimeRFE board you want to target.

Once opened successfully (check status message in 6) you will apply the settings using the Apply button (5). Whenever the settings are changed this button lits in green showing that you may press it to update the board. Conversely the "to GUI" button (4) reads the settings from the board and updates the GUI.

<h3>1. USB serial devices list</h3>

This combo lists all USB serial devices list available in the system regardless if they are LimeRFE or other devices. You must specify the device corresponding to a LimeRFE device to be able to open it successfully with (2)

<h3>2. Open device</h3>

Click on this button to open the serial device selected by (1). You need to open the device successfully prior to any operation. The open status is displayed in the status window (5).

<h3>3. Close device</h3>

If you have more than one LimeRFE connected to your system you have to close one before opening another by using this button.

<h3>4. Pull device configuration to GUI</h3>

Use this button to retrieve the LimeRFE device current configuration and populate the GUI with its data.

<h3>5. Apply changes</h3>

Use this button to apply configuration changes. You must press this button to make any of your changes active. Whenever a change requires the LimeRFE configuration to be changed to become effective this button becomes green to suggest it should be activated.

<h3>6. Status window</h3

<h2>A. Rx channel control</h2>

![LimeRFE Rx section](../../../doc/img/LimeRFE_plugin_rx.png)

<h3>A.1. Rx channel group</h3>

Select the channel group using this combo. Groups are:

  - **Wideband**: covers all frequency ranges in two sub-bands
  - **HAM**: covers amateur radio bands
  - **Cellular**: covers cellular phone bands

<h3>A.2 Rx band selection</h3>

Depending on the channel group selected in (A.1) use this combo to select the band.

<h4>Wideband channels</h4>

  - **1-1000MHz**: 1 MHz to 1 GHz
  - **1-4GHz**: 1 GHz to 4 GHz

<h4>HAM channels</h4>

  - **<30MHz**: HF bands i.e. low-pass up to 30 MHz
  - **50-70MHz**: covers the 6m and 4m bands
  - **144-146MHz**: covers the 2m band
  - **220-225MHz**: covers the 1.25m band (region 2)
  - **430-440MHz**: covers the 70cm band
  - **902-928MHz**: covers the 33cm band (region 2)
  - **1240-1325MHz**: covers the 23cm band
  - **2300-2450MHz**: covers the 13cm band
  - **3300-3500MHz**: covers the 9cm band (not available in all countries)

  <h4>Cellular channels</h4>

  - **Band 1**: 2110-2170 MHz
  - **Band 2**: 1930-2000 MHz
  - **Band 7**: 2620-2700 MHz
  - **Band 38**: 1800-1880 MHz

<h3>A.3 Rx port</h3>

Select which port to connect the Rx to:

  - **Tx/Rx (J3)**: this is the J3 port combining Rx and Tx. When cellular bands are selected this is connected to a duplexer internally
  - **Tx/Rx 30M (J5)**: HF port only available when wide band channels are selected

<h3>A.4 Attenuator</h3>

Select attenuator value from 0 to 14 dB in 2 dB steps.

<h3>A.5 AM/FM notch</h3>

Toggle AM/FM broadcast bands notch filter.

<h2>B. Tx channel control</h2>

![LimeRFE Tx section](../../../doc/img/LimeRFE_plugin_tx.png)

<h3>B.1 Copy Rx band settings</h3>

When selected the Rx channel group (A.1) and Rx band (A.2) is copied to the Tx channel group (B.2) and Tx band (B.3) respectively.

<h3>B.2 Tx channel group</h3>

Select the channel group using this combo. This is identical to the Rx side (A.1).

<h3>B.3 Tx band selection</h3>

Use this combo to select the band. This is identical to the Rx side (A.2).

<h3>A.3 Tx port</h3>

Select which port to connect the Rx to:

  - **Tx/Rx (J3)**: this is the J3 port combining Rx and Tx. When cellular bands are selected this is connected to a duplexer internally
  - **Tx (J4)**: Tx connected port only. Can be used to split Rx and Tx to drive a higher power P.A. for example


This is where status messages are displayed.

<h2>C. Power and SWR</h2>

![LimeRFE power section](../../../doc/img/LimeRFE_plugin_power.png)

<h3>C.1 Activate power measurement</h3>

Check this box to enable power measurements.

<h3>C.2 Refresh measurements</h3>

Use this button to refresh the power measurements.

<h3>C.3 Power measurement source</h3>

Use this combo to select the power measurement source:

  - **EXT**: External: select this when a dual directional coupler is connected to `Ref` (J17) and `Fwd` (J18) ports
  - **CEL**: Cellular: select this to use the internal coupler when cellular bands are engaged

<h3>C.4 Monitor power continuously</h3>

Use this switch to activate the continuous monitoring. A measurement will be taken every 500ms.

<h3>C.5 Power correction</h3>

Use a power meter and apply this correction to obtain the real delivered power in dBm. There is one correction factor by band. The values are saved in the persistent settings.

<h3>C.6 Coupler relative power measurements</h3>

  - **Fwd**: This is the relative forward direction power in dB.
  - **Ref**: This is the relative reverse direction power in dB.
  - **RL**: This is the return loss in dB and is exactly Fwd minus Ref

<h3>C.7 Voltage Standing Wave Ratio</h3>

This is the VSWR computed from the return loss RL in (C.6)

<h3>C.8 Corrected power in dBm</h3>

This is the corrected power in dBm and is exactly Fwd in (C.6) plus correction in (C.8).

<h3>C.9 Corrected power in Watts</h3>

This is the corrected power in Watts.

<h3>C.10 Corrected power averaging</h3>

Use this switch to activate the averaging of corrected power. This is a moving average over 10 measurements thus over a 5s period.

<h2>D. Control</h2>

![LimeRFE control section](../../../doc/img/LimeRFE_plugin_control.png)

<h3>D.1 Rx mode</h3>

Use this button to switch on Rx. This has no effect when Cellular channels are engaged.

<h3>D.2 Tx mode</h3>

Use this button to switch on Tx. This has no effect when Cellular channels are engaged.

<h3>D.3 Rx/Tx toggle</h3>

Use this switch to activate Rx/Tx toggle. When Rx is switched on Tx is switched off automatically and vice versa.

<h3>D.4 Rx/Tx device synchronization</h3>

When switched on this connects the Rx (D.1) and Tx (D.2) switches to a Rx and Tx device set selected by (D.5) and (D.6) respectively in order to start or stop devices accordingly.

<h3>D.5 Rx device set index</h3>

Select the Rx device set index with which you want to synchronize the Rx switch (D.1).

<h3>D.6 Tx device set index</h3>

Select the Tx device set index with which you want to synchronize the Tx switch (D.2).

<h3>D.7 Refresh device sets indexes</h3>

When the configuration of device sets changes you can use this button to refresh the device set indexes in (D.5) and (D.6).
