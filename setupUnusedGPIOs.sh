#!/bin/bash
# P8_10 GPIO_68
# CF1_PULSE_OUT
#cd /sys/class/gpio
#echo 68 > export
cd /sys/class/gpio/gpio68
echo in > direction

# P8_8 GPIO_67
# CF2_PULSE_OUT
#cd /sys/class/gpio
#echo 67 > export
cd /sys/class/gpio/gpio67
echo in > direction

# P8_16 GPIO_46
# ZERO_CROSS
#cd /sys/class/gpio
#echo 46 > export
cd /sys/class/gpio/gpio46
echo in > direction

# P8_18 GPIO_65
# IRQ
#cd /sys/class/gpio
#echo 65 > export
cd /sys/class/gpio/gpio65
echo in > direction

