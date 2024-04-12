#!/bin/bash
# P9_17 GPIO0_5
# /MUX_CS
cd /sys/class/gpio/gpio5
echo out > direction
echo 1 > value

# P9_42 GPIO0_7
# /PWR_MON_CS on schematic, but this is spidev1.1 CS, not in the device tree
# Make it an input
cd /sys/class/gpio/gpio7
echo in > direction

# P9_28 GPIO_113
# N/C on schematic, this is spidev1.0 CS which is in the device tree, use it
cd /sys/class/gpio/gpio113
echo out > direction
echo 1 > value
