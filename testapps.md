# Test Apps

## gpioread

    gpioread [<signal>[ <signal> ...]
    where <signal> is one of the names
        CF1_PULSE_OUT
        CF2_PULSE_OUT
        WARN_OUT_PULSE
        ZERO_CROSS
        IRQ
        POWER_GOOD
        ID0
        ID1
        ID2
        FAN_1_TACH
        FAN_2_TACH
    With no arguments, prints all the signals.

    Output format is 0 or 1 for each signal specified, separated by spaces.
    E.g., 0 1 0 0 1

## gpioset

    gpioset <signal> <state>[ <signal> <state> ...]
    where <signal> is one of the names
        PWR_MON_CS
        MUX_CS
        220VAC_RELAY
        MUX_DIS
        FAN_1_ON_OFF
        FAN_2_ON_OFF
        HEATER_1_SW
        HEATER_2_SW
        HEATER_3_SW
        HEATER_4_SW
        HEATER_5_SW
        HEATER_6_SW
        HEATER_7_SW
        HEATER_8_SW
        HEATER_9_SW
        HEATER_10_SW
        HEATER_11_SW
        HEATER_12_SW
    and <state> is either 0 or 1

## adcread

    adcread [-t <gpio>] <channel>
    where <channel> is the ADC channel to read (0 or 1)
    and <gpio> is the gpio number on which to indicate start and stop of the ADC conversion.
    This can be any unused GPIO pin
    (currently the list is 20, 26, 27, 34, 36, 38, 45, 47, 48, 49, 60, 62, 63, 70, 115, 117)
    If the -t argument is ommitted, no GPIO pin is used to signal the ADC conversion start/stop.

## rtdmux

    rtdmux [<mux> <channel>]
    closes <channel> (0 - 9) on <mux> (0 or 1)
    sets the MUX_DIS signal low and leaves it low to enable the muxes
    with no arguments, opens all channels on both muxes then sets MUX_DIS high
    mux 0 is connected to ADC channel 0, mux 1 is connected to ADC channel 1

## pwrmon

    pwrmon [-x] <reg>
    where <reg> is either I (reads the Irms register) or V (reads the Urms register)
    -x causes the value to be printed in hexadecimal (the default is decimal)
