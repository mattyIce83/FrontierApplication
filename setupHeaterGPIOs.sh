#!/bin/bash
# P8_19 EHRPWM2B
# 220VAC_RELAY
# must be on for heaters to work!
#cd /sys/class/gpio
#echo 22 > export
cd /sys/class/gpio/gpio22
echo out > direction
echo 1 > value

# P8_15 GPIO_47
# HEATER_1_SW
#cd /sys/class/gpio
#echo 47 > export
cd /sys/class/gpio/gpio47
echo out > direction
echo 0 > value

# P8_17 GPIO_27
# HEATER_2_SW
#cd /sys/class/gpio
#echo 27 > export
cd /sys/class/gpio/gpio27
echo out > direction
echo 0 > value

# P8_26 GPIO_61
# HEATER_3_SW
#cd /sys/class/gpio
#echo 61 > export
cd /sys/class/gpio/gpio61
echo out > direction
echo 0 > value

# P8_28 GPIO_88
# HEATER_4_SW
#cd /sys/class/gpio
#echo 88 > export
cd /sys/class/gpio/gpio88
echo out > direction
echo 0 > value

# P8_30 GPIO_89
# HEATER_5_SW
#cd /sys/class/gpio
#echo 89 > export
cd /sys/class/gpio/gpio89
echo out > direction
echo 0 > value

# P8_34 GPIO_81
# HEATER_6_SW
#cd /sys/class/gpio
#echo 81 > export
cd /sys/class/gpio/gpio81
echo out > direction
echo 0 > value

# P8_36 GPIO_80
# HEATER_7_SW
#cd /sys/class/gpio
#echo 80 > export
cd /sys/class/gpio/gpio80
echo out > direction
echo 0 > value

# P8_38 GPIO_79
# HEATER_8_SW
#cd /sys/class/gpio
#echo 79 > export
cd /sys/class/gpio/gpio79
echo out > direction
echo 0 > value

# P8_40 GPIO_77
# HEATER_9_SW
#cd /sys/class/gpio
#echo 77 > export
cd /sys/class/gpio/gpio77
echo out > direction
echo 0 > value

# P8_42 GPIO_75
# HEATER_10_SW
#cd /sys/class/gpio
#echo 75 > export
cd /sys/class/gpio/gpio75
echo out > direction
echo 0 > value

# P8_45 GPIO_70
# HEATER_11_SW
#cd /sys/class/gpio
#echo 70 > export
cd /sys/class/gpio/gpio70
echo out > direction
echo 0 > value

# P8_46 GPIO_71
# HEATER_12_SW
#cd /sys/class/gpio
#echo 71 > export
cd /sys/class/gpio/gpio71
echo out > direction
echo 0 > value

