EESchema Schematic File Version 4
EELAYER 30 0
EELAYER END
$Descr User 7000 4000
encoding utf-8
Sheet 1 2
Title "SenseAir LP8 Module"
Date ""
Rev ""
Comp ""
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
$Sheet
S 1600 1000 1100 600 
U 5FB314E3
F0 "LP8" 50
F1 "co2-lp8.sch" 50
F2 "MEAS_RDY" O R 2700 1300 50 
F3 "LP8_TxD" O R 2700 1200 50 
F4 "LP8_RxD" I R 2700 1100 50 
F5 "EN_MEAS" I R 2700 1400 50 
F6 "VCAP" O L 1600 1500 50 
F7 "3V3" I L 1600 1100 50 
F8 "EN_PWR" I L 1600 1200 50 
F9 "EN_REV_BLOCK" I L 1600 1400 50 
F10 "EN_CHARGE" I L 1600 1300 50 
$EndSheet
Wire Wire Line
	1600 1100 1450 1100
Wire Wire Line
	1600 1200 1450 1200
Wire Wire Line
	1600 1300 1450 1300
Wire Wire Line
	2850 1400 2700 1400
Wire Wire Line
	2850 1300 2700 1300
Wire Wire Line
	2700 1100 2850 1100
Wire Wire Line
	2700 1200 2850 1200
Wire Wire Line
	5400 1150 5550 1150
Wire Wire Line
	5400 1250 5550 1250
Text Label 1450 1100 2    50   ~ 0
3V3
Text Label 1450 1300 2    50   ~ 0
LP8_EN_CHARGE
Text Label 1450 1500 2    50   ~ 0
LP8_VCAP
Text Label 2850 1100 0    50   ~ 0
TxD
Text Label 2850 1200 0    50   ~ 0
RxD
Text Label 2850 1300 0    50   ~ 0
LP8_MEAS_RDY
Text Label 2850 1400 0    50   ~ 0
LP8_EN_MEAS
Text Label 5550 1450 0    50   ~ 0
TxD
Text Label 5550 1550 0    50   ~ 0
RxD
Wire Wire Line
	4600 1550 4450 1550
Wire Wire Line
	4600 1150 4450 1150
Text Label 4450 1150 2    50   ~ 0
LP8_VCAP
Wire Wire Line
	1450 1500 1600 1500
Text Label 1450 1200 2    50   ~ 0
LP8_EN_PWR
Text Label 4450 1550 2    50   ~ 0
LP8_EN_MEAS
Text Label 4450 1450 2    50   ~ 0
LP8_MEAS_RDY
Wire Wire Line
	4600 1450 4450 1450
Text Label 5550 1050 0    50   ~ 0
LP8_EN_CHARGE
Text Label 4450 1250 2    50   ~ 0
LP8_EN_PWR
Wire Wire Line
	4450 1350 4600 1350
Wire Wire Line
	4450 1250 4600 1250
$Comp
L Connector:Conn_01x01_Male J3
U 1 1 5FA00A7B
P 4800 1750
F 0 "J3" H 4850 1700 50  0000 R CNN
F 1 "Conn_01x01_Male" H 4750 1750 50  0001 R CNN
F 2 "Connector_PinSocket_2.54mm:PinSocket_1x01_P2.54mm_Vertical" H 4800 1750 50  0001 C CNN
F 3 "~" H 4800 1750 50  0001 C CNN
	1    4800 1750
	-1   0    0    1   
$EndComp
$Comp
L Connector:Conn_01x01_Male J4
U 1 1 5FA02F41
P 4800 1850
F 0 "J4" H 4850 1800 50  0000 R CNN
F 1 "Conn_01x01_Male" H 4750 1850 50  0001 R CNN
F 2 "Connector_PinSocket_2.54mm:PinSocket_1x01_P2.54mm_Vertical" H 4800 1850 50  0001 C CNN
F 3 "~" H 4800 1850 50  0001 C CNN
	1    4800 1850
	-1   0    0    1   
$EndComp
Wire Wire Line
	4600 1750 4450 1750
Wire Wire Line
	4600 1850 4450 1850
Text Label 4450 1750 2    50   ~ 0
3V3
Wire Wire Line
	4450 1850 4450 1950
$Comp
L power:GND #PWR0101
U 1 1 5FA03B7C
P 4450 1950
F 0 "#PWR0101" H 4450 1700 50  0001 C CNN
F 1 "GND" H 4455 1777 50  0000 C CNN
F 2 "" H 4450 1950 50  0001 C CNN
F 3 "" H 4450 1950 50  0001 C CNN
	1    4450 1950
	1    0    0    -1  
$EndComp
$Comp
L Connector:Conn_01x05_Male J1
U 1 1 5FA3D732
P 4800 1350
F 0 "J1" H 4950 1000 50  0000 R CNN
F 1 "Conn_01x05_Male" V 4700 1650 50  0001 R CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_1x05_P2.54mm_Vertical" H 4800 1350 50  0001 C CNN
F 3 "~" H 4800 1350 50  0001 C CNN
	1    4800 1350
	-1   0    0    1   
$EndComp
Wire Wire Line
	1600 1400 1450 1400
Text Label 1450 1400 2    50   ~ 0
LP8_EN_REV_BLOCK
Wire Wire Line
	5400 1350 5550 1350
Text Label 4450 1350 2    50   ~ 0
LP8_EN_REV_BLOCK
Wire Wire Line
	5400 1550 5550 1550
Wire Wire Line
	5400 1450 5550 1450
Text Label 5550 1350 0    50   ~ 0
SDA
Text Label 5550 1250 0    50   ~ 0
SCL
$Comp
L Connector:Conn_01x06_Male J2
U 1 1 5FAE2FEA
P 5200 1250
F 0 "J2" H 5308 1539 50  0000 C CNN
F 1 "Conn_01x06_Male" H 5308 1540 50  0001 C CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_1x06_P2.54mm_Vertical" H 5200 1250 50  0001 C CNN
F 3 "~" H 5200 1250 50  0001 C CNN
	1    5200 1250
	1    0    0    -1  
$EndComp
Wire Wire Line
	5400 1050 5550 1050
NoConn ~ 5550 1150
$EndSCHEMATC
