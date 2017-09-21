<h1>Devices common resources</h1>

This folder contains classes and methods that can be used by different plugins that work with a common physical device or via network. Thus this can be one of the following devices:

  - BladeRF: one Rx and one Tx full duplex. Plugins are:
    - bladerfinput
    - bladerfoutput
    
  - HackRF: one Rx and one Tx half duplex. Plugins are:
    - hackrfinput
    - hackrfoutput
  
  - LimeSDR: 2 Rx and 2 Tx full duplex. Plugins are
    - limesdrinput
    - limesdroutput
    
  - PlutoSDR: one Rx and one Tx full duplex. Plugins are
    - plutosdrinput
    - plutosdroutput

  - SDRdaemon: sends or receive samples to/from device remotely through the network. Used on the Tx plugin only
    - sdrdaemonsink 