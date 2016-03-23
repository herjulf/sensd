seltag
======



seltag filters and formats "tagged data" on stdin and outputs on stdout
in column format. A typical seltag input comes from sensd (sensors.dat).
The resulting data is normally used for plotting and further analysis.
In most cases seltag is used together unix commands like grep, tail etc.

### Syntax:

seltag -sel ID=%s T=%s V_MCU=%s < infile > outfile

Example:

### Raw report

	tail /var/log/sensors.dat | grep 28e13cce020000c6

 	2013-12-30 23:58:24 TZ=CET UT=1388444304 GWGPS_LON=17.36869 GWGPS_LAT=59.51052 &: ID=28e13cce020000c6 PS=1 T=19.00  V_MCU=3.60 UP=18BEC40 V_IN=0.29  V_A1=0.00  V_A2=0.00  [ADDR=225.198 SEQ=248 RSSI=16 LQI=255 DRP=1.00]

tail /var/log/sensors.dat | grep 28e13cce020000c6 | seltag --sel ID=%s T=%s

### Example: Running the seltag to filter out tag ID and T

	tail /var/log/sensors.dat | grep 28e13cce020000c6 | seltag 2 ID=%s T=%s

	2013-12-30 23:58:24 28e13cce020000c6 19.00

Note the special syntax "=%s".


Notes
-----

seltag reads and wites to stdin and sdtout so it can b used togeteher wither
powerful unix utilties. 

Some obsvervation when using with grep. To avoid grep buffering one can use
 
--line-buffered

	netcat -C herjulf.se 1235  | grep --line-buffered  -i -v  Miss | grep --line-buffered  RND | seltag -sel RND=%s


	netcat 129.177.63.214 1234 | grep --line-buffered 10m | grep --line-buffered  SHT2X  | seltag -sel  T_SHT2X=%s RH_SHT2X=%s



