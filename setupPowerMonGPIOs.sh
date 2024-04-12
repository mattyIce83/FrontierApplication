#!/bin/bash
# P8_12 GPIO_44
# WARN_OUT_PULSE
#cd /sys/class/gpio
#echo 44 > export
cd /sys/class/gpio/gpio44
echo in > direction
echo rising > edge
