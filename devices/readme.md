<h1>Devices common resources</h1>

This folder contains classes and methods that can be used by different plugins that work with a common physical device or via network. Thus this can be one of the following devices:

  - BladeRF1: one Rx and one Tx full duplex. Plugins are:
    - bladerf1input
    - bladerf1output
    
  - BladeRF2: 2 Rx and 2 Tx full duplex (BladeRF 2.0 micro). Plugins are:
    - bladerf2input
    - bladerf2output
    
  - HackRF: one Rx and one Tx half duplex. Plugins are:
    - hackrfinput
    - hackrfoutput
  
  - LimeSDR: 2 Rx and 2 Tx full duplex (Lime-USB). 1 Rx and 1 Tx full duplex (Lime-Mini). Plugins are
    - limesdrinput
    - limesdroutput
    
  - PlutoSDR: one Rx and one Tx full duplex. Plugins are
    - plutosdrinput
    - plutosdroutput

  - SoapySDR: Soapy SDR virtual device
    - soapysdrinput
    - soapysdroutput

  - USRP: Up to 2 Rx and 2 Tx full duplex. Plugins are
    - usrpinput
    - usrpoutput
