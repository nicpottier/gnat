# Decent GNAT (GNAT is Not A Tablet)

GNAT is an esp32 based mini-dashboard for Decent DE1 espresso machines. It pairs with the machine and a scale and manages putting the machine to sleep, taring scale when pouring a shot and stopping at a preset weight. Also shows pretty graphs. 

# Install

You can find out more about GNAT and install it from your Chrome browser by visiting [https://nicpottier.github.io/gnat/](https://nicpottier.github.io/gnat/)

# Quickstart

This is a PlatformIO Project. To build and install you'll need to follow these steps:

1. Install [Visual Studio Code](https://code.visualstudio.com/)

2. Install the [PlatformIO](https://platformio.org/) Extension

3. Clone this repository and point PlatformIO to open the root of this repository as your project folder.

4. Once PlatformIO opens the GNAT project you should be able to build the project by clicking on the checkmark in the lower toolbar and install the firmware to a connected device by clicking the right arrlow icon in the same place. You can change the target by selecting the approproate environment in the lower toolbar, ie: `env:ttgo` 
