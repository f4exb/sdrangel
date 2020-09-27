<h1>Simple PTT plugin</h1>

<h2>Introduction</h2>

This plugin controls switchover between a Rx (Device source) and Tx (Device sink). It has no other controls than an adjustable delay from Rx to Tx and back to Rx. Because of its simplicity it can also serve as a model to build other feature plugins.

<h2>Interface</h2>

![File source channel plugin GUI](../../../doc/img/SimplePTT_plugin.png)

<h3>1: Start/Stop plugin</h3>

This button starts or stops the plugin

<h3>2: PTT button</h3>

Click to switch from Rx to Tx and again to switch back to Rx. When in Tx mode the button is lit.

<h3>3: Status indicator</h3>

This LED type display shows the current PTT status:

  - **Green**: Rx runs
  - **Red**: Tx runs
  - **Grey**: None active (transient)

<h3>4: Refresh list of devices</h3>

Use this button to refresh the list of devices (5) and (6)

<h3>5: Select Rx device set</h3>

Use this combo to select which Rx device is controlled

<h3>6: Select Tx device set</h3>

Use this combo to select which Tx device is controlled

<h3>7: Transistion delay from Rx to Tx</h3>

Value in milliseconds between Rx stop and Tx start

<h3>8: Transistion delay from Tx to Rx</h3>

Value in milliseconds between Tx stop and Rx start
