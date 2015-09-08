sensd - A WSN Internet gateway, daemon and hub
==============================================

Authors
--------
Robert Olsson <robert@radio-sensors.com>
Jens Laas <jens.laas@uadm.uu.se>

Abstract
--------
We've outlined, designed and implemented and very simple concept for WSN data
reports, including collection, storage and retrieval using standard text tools.
The sensor data can be sent over Internet to active listeners. The concept also
includes a mapping to URI (Unified Resource Identifier) to form a WSN caching
server similar to CoAP using http-proxy.

sensd also includes a proxy functionality primary to forward WSN data to a
public IP. This is in case the GW is behind a NAT. sensd can be used both to
forward data but it can also work as the "proxy" server receiving WSN data
and allowing multiple TCP listeners.

All programs are written C, Java-script and bash. And designed for small
footprint and with minimal dependencies. sensd runs on Raspberry Pi and Openwrt.

Copyright
---------

Open-Source via GPL

Introduction
------------

This is collection of software to implement data monitoring and data collection
from WSN Wireless Sensor Networks. The goal is to have a very simple,
straight-forward and robust framework.

The scenario: One or several mots is connected to USB or serial port to gather
received information from connected WSN motes. Data can be visualized in
several ways.

*  Sensor data report can be transmitted and propagated throughout the
   Internet. sensd acts as server and sends incoming report to active
   listeners.

*  Data is kept in ASCII with tagging and ID information. Data is conveniently
   handled, copied and viewed with standard text utilities of your OS.

*  Last mote report is cached into the file system suitable for URI use. The
   Format is SID/TAG. Typical tags are EUI64 and unique serial numbers. The
   different TAGS are left for mote user to define. Although the TAGS used in
   our example setup are included in this draft for example purposes.

Both formats can easily be stored or linked directly in web tree to form a
URI to format WSN logging/datafile or caching service.

Major components
--------------------

### sensd

A daemon that reads WSN mote reports from USB/serial and stores data in a ASCII
data file. Default is located at _/var/log/sensors.dat_

### js

A set of Java-scripts can plot, print and visualize sensor data from sensd
directly in your web-browser.

### seltag

seltag filters and formats "tagged data" on stdin and outputs on stdout
in column format. A typical seltag input comes from sensd (sensors.dat).
The resulting data is normally used for plotting and further analysis.
In most cases seltag is used together unix commands like grep, tail etc.

Syntax:

	seltag 2 ID=%s T=%s V_MCU=%s < infile > outfile

Example:

Raw report

	tail /var/log/sensors.dat | grep 28e13cce020000c6

 	2013-12-30 23:58:24 TZ=CET UT=1388444304 GWGPS_LON=17.36869 GWGPS_LAT=59.51052 &: ID=28e13cce020000c6 PS=1 T=19.00  V_MCU=3.60 UP=18BEC40 V_IN=0.29  V_A1=0.00  V_A2=0.00  [ADDR=225.198 SEQ=248 RSSI=16 LQI=255 DRP=1.00]

Running the seltag to filter out tag ID and T

	tail /var/log/sensors.dat | grep 28e13cce020000c6 | seltag 2 ID=%s T=%s

	2013-12-30 23:58:24 28e13cce020000c6 19.00

Note the special syntax "=%s".

### doc

Documentation and sample files

Expose WSN data alternatives:


Data types
----------
Space or control chars is not allowed in any types.

Float           (F)
Integer Dec     (Id)
Integer HEX     (Ih)
Boolean 0 or 1  (B)
String          (S)

Datafile logging
----------------

Below is and example of the anatomy of a sensors.dat file we are currently using in our WSN
data collection networks.

	2012-05-22 14:07:46 UT=1337688466 ID=283c0cdd030000d7 PS=0 T=30.56  T_MCU=34.6  V_MCU=3.08 UP=2C15C V_IN=4.66

	2012-05-22 14:11:41 UT=1337688701 ID=28a9d5dc030000af PS=0 T=36.00  V_MCU=2.92 UP=12C8A0 RH=42.0 V_IN=4.13  V_A1=3.43  [ADDR=0.175 SEQ=33 RSSI=21 LQI=255 DRP=1.00]

Each line is a mote report. They start with date and time and are followed by a set of
tags. The tags is different for different motes. In other words they can
send different data. Essential is the ID which should be unique for each mote.

The information with brackets is information generated by the receiving mote
and is not a part the motes data. Typically RSSI (Receiver Signal Strength
Indicator) and LQI (Link Quality Indicator)

S = String
Id = Integer Decimal
Ih = Integer HEX
B = Boolean

Example of tags used:

*   UT= Unix time                                (Id)      Provided by sensd
*   TZ= Time Zone                                (String)  Provided by sensd
*   GWGPS_LON=                                   (float)   Provided by sensd
*   GWGPS_LAT=                                   (float)   Provided by sensd
*   TXT= Node text field, site, name etc         (s)
*   ID= Unique 64 bit ID                         (S)
*   E64= EUI-64 Unique 64 bit ID                 (S)
*   T= temp in C (Celcius)                       (F)
*   PS= Power Save Indicator                     (B)
*   P= Pressure                                  (F)
*   V_MCU= Microcontroller voltage               (F)
*   UP= Uptime                                  (Ih)
*   RH= Relative Humidity in %                   (F)
*   V_IN= Voltage Input                          (F)
*   V_A1= Voltage Analog 1 (A1)                  (F)
*   V_A2= Voltage Analog 2 (A2)                  (F)
*   V_A3= Voltage Analog 1 (A3)                  (F)
*   V_A4= Voltage Analog 1 (A4)                  (F)
*   V_AD1= Voltage ADC 1 (AD1)                   (F)
*   V_AD2= Voltage ADC 2 (AD2)                   (F)
*   V_AD3= Voltage ADC 3 (AD3)                   (F)
*   V_AD4= Voltage ADC 4 (AD4)                   (F)
*   P0= Number of pulses (Input 0)              (Ih)
*   P0_T= Pulses per Sec  (Input 0)              (F)
*   P1= Number of pulses (Input 1)              (Ih)
*   P1_T= Pulses per Sec  (Input 1)              (F)
*   RSSI= Reeiver Signal Strengh Indicator      (Id)
*   TTL= Time-To-Live for multi-hop relay       (Id)
*   LQI= Link Quality Indicator                 (Id)
*   SEQ= Sequental Number (packet)              (Id)
*   DRP= Drop Probability (Contiki)              (F)
*   ADDR=                                        (S)
*   SRC= Source IP addr. When received by proxy  (S)

Datafile metadata
------------------

Meta-data and additional information, descriptions and comments can be stored in
data file to keep everything in on context. Of course pre-cautions must be taken
so this auxiliary data doesn't conflict with the data. For example to restrict or escape
the '=' sign. It is suggested that data are retrieved with seltag or a similar technique.

Special characters
-----------------

&: is used in the report message. The idea is to distinguish between data
report and responses to commands. This not yet implemented.

[] The information between brackets [] is supplied by the receiver or sink node
and not by the sending one. For example Receiver Signal Strength Indicator (RSSI)
and Link Quality Indicator (LQI) originates from the receiving node.

Internet sensor data
--------------------

Start sensd with the `-report` option. This enables reports to be transmitted
over IP to remote listeners. Default TCP port 1234.

Server side example:

	sensd -report -p 1234  -D /dev/ttyUSB0

Client side. Example using netcat:

	nc server-ip 1234

URI format
----------

URI (Unified Resource Identifier) displays the node ID and the tags in a file tree.
It is easy to export this into a web tree to form a URI similar to a CoAP gateway.

Example: In our case we have a unique sensor ID followed by the different data
fields represented by "tags".

	/tmp/WSN1-GW1/281a98d20200004a:
	DRP  ID  LQI  PS  RH  RSSI  SEQ  T  V_IN  V_MCU  ADDR

	/tmp/WSN1-GW1/28be51ce02000031:
	DRP  ID  LQI  PS  RH  RSSI  SEQ  T  UP  V_IN  V_MCU  ADDR

Read Temp from a sensor:

	cat /tmp/WSN1-GW1/281a98d20200004a/T
	19.44

And it's very easy to link this tree into a web-server.

GPS support
-----------

Positioning support has been added via GPS device connected to serial
or USB port. Tags added when enabled GWGPS_LON & GWGPS_LAT.
GPS code from. https://github.com/herjulf/gps_simple

Getting the source and building
-------------------------------

Code is stored in github. Typically procedure below is the very straight-
forward unix way:

	git clone http://github.com/herjulf/sensd
	cd sensd
	make

Put your binaries after your preference:

Pre-built binary versions
--------------------------

For x86:
Sensd and friends are available in Bifrost/Linux packages. Those packages are
statically linked and can be used on most x86 Linuxes. 32-bit compiled.

http://ftp.sunet.se/pub/Linux/distributions/bifrost/download/opt/opt-sensd-2.3-1.tar.gz


Use
---

The WSN data logging and caching concept is in actual use with Contiki, RIME
broadcast application.

Tips
----

One can use netcat to listen to reports:

Example:

	nc radio-sensors.com 1235

To save in file use nohup:

	nohup nc radio-sensors.com 1235 > /var/log/sensors.dat
