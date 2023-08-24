
MPW-sensor
#################

Overview
********

An application that detects movement and sends Bluetooth advertisements accordingly.

If using the custom board, replace the code in \ncs\v2.4.0\zephyr\drivers\sensor\lis2dh\lis2dh.c
with the newer code in new_lis2dh.c
The newer code enables the use of a power pin instead of vdd, but paths are not updated in the code.