<h1>Running SDRangel in a Docker container</h1>

&#9888; This is highly experimental and subject to large changes. Do not expect it to work in all cases.

Because SDRangel uses OpenGL this can make it possibly more difficult to run it properly on some hosts and operating systems. Some particular implementations can be derived fom a base GUI image (see "Folders organization" and "Building images" paragraphs).

<h2>Install Docker</h2>

This is of course the first step. Please check the [Docker related page](https://docs.docker.com/install/) and follow instructions related to your system.

<h3>Windows</h3>

In Windows you have two options:
  - Install with Hyper-V: Hyper-V is a bare-metal type hypervisor where the Windows O/S itself runs in a VM. The catch is that it requires Windows 10 Pro version and a special set up. This is required to install _ Docker Desktop for Windows_ In this configuration a docker instance runs in its own VM
  - Install with Oracle Virtualbox: Virtualbox is a hosted type hypervisor that sits on the top of the Windows O/S so it puts an extra layer on the stack but may be available on more flavors of Windows. In this case you will install Docker in a Linux O/S Virtualbox VM for example Ubuntu 18.04 and therefore you will have to follow Linux instructions.
  
See [this discussion](https://www.nakivo.com/blog/hyper-v-virtualbox-one-choose-infrastructure/) about the difference between Hyper-V and Virtualbox.

<h2>Get familiar with Docker</h2>

The rest of the document assumes you have some understanding on how Docker works and know its most used commands. There are tons of tutorials on the net to get familiar with Docker. Please take time to play with Docker a little bit so that you are proficient enough to know how to build and run images, start containers, etc... Be sure that this is not time wasted just to run this project. Docker is a top notch technology (although based on ancient roots) widely used in the computer industry and at the heart of many IT ecosystems.

<h2>Folders organization</h2>

This is a hierarchy of folders corresponding to an image build hierarchy. At each node building an image may be possible based on the image built previously. A terminating node always builds a final runnable image.

<h3>base</h3>

this creates a `bionic/sdrangel:base` image that is used as a base for all images. It takes an Ubuntu 18.04 base, performs all necessary packages installations then eventually compiles and installs all specific software dependencies

<h3>base/gui</h3>

this creates a `bionic/sdrangel:gui` image from the base image is used as a base for all GUI images. Essentially it compiles and installs SDRangel GUI flavor.

<h3>base/gui/vanilla</h3>

this creates a `bionic/sdrangel:vanilla` runnable image from the GUI base image to be used on systems without any specific hardware requirements.

<h3>base/gui/linux</h3>

Linux specific host implementations    

<h3>base/gui/linux/nvidia</h3>

Creates a `bionic/sdrangel:linux_nvidia` runnable image based on the base GUI image to be used on Linux systems with NVidia proprietary drivers

<h3>base/server</h3>

this creates a `bionic/sdrangel:server` runnable image from the base image to be used to run SDRangel server flavor. Essentially it compiles and installs SRangel server flavor.
  
<h2>Building images</h2>

Each build folder contains a `Dockerfile` to create an image and a script `build.sh` to actually build the image and tag it with its specific tag. To build an image you first `cd` to the appropriate folder.

You will proceed by running the `build.sh` script if it exists at each node in the folder hierarchy path until you reach the final image. The possible use cases corresponding to a particular path are the following:

  - GUI flavor without specific hardware requirements: `base/gui/vanilla`
  - GUI flavor on Linux systems with NVidia proprietary drivers: `base/gui/linux/nvidia`
  - Server flavor: `base/server`
  
Note: for now all images are to be run in a `x86_64` environment.

<h2>Running images</h2>

Each terminal folder in the path hierarchy corresponds to a runnable image and therefore contains a `run.sh` script to be executed to start a container with the corresponding image.