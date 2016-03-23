doc
===


Suggested data types
---------------------
Space or control chars is not allowed in any types.

Float           (F)
Integer Dec     (Id)
Integer HEX     (Ih)
Boolean 0 or 1  (B)
String          (S)


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
