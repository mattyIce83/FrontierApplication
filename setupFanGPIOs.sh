#!/bin/bash
# P8_7 GPIO2_2
# FAN_1_TACH
#cd /sys/class/gpio
#echo 66 > export
cd /sys/class/gpio/gpio66
echo in > direction

# P8_9 GPIO2_5
# FAN_2_TACH
#cd /sys/class/gpio
#echo 69 > export
cd /sys/class/gpio/gpio69
echo in > direction

# P8_41 GPIO2_10
# FAN_2_ON_OFF
cd /sys/class/gpio/gpio74
echo out > direction
echo 0 > value

# P8_43 GPIO2_8
# FAN_1_ON_OFF
cd /sys/class/gpio/gpio72
echo out > direction
echo 0 > value

# P8_11 GPIO1_13
# FAN_1_ILIM
#cd /sys/class/gpio
#echo 45 > export
cd /sys/class/gpio/gpio45
echo in > direction

# P8_14 GPIO0_26
# FAN_2_ILIM
#cd /sys/class/gpio
#echo 26 > export
cd /sys/class/gpio/gpio26
echo in > direction


