10 'SAVE "N_CLOCK.BAS"
20 _KANJI:_CLS:ON STOP GOSUB 180:STOP ON
30 'Init Nixie tube
40 PS$=CHR$(&HFF):_IOTPUT("device/i2c_a/2A",PS$)
50 'Print KANJI
60   LOCATE 0,6  :GET DATE DA$:PRINT USING "&          &: @";"うれおt";DA$
70  LOCATE 0,6+1:GET TIME TM$:PRINT USING "&          &: @";"ょせゅっ";TM$
80 'Nixie tube
90  PS$=CHR$(&H17)+CHR$(VAL(MID$(TM$,1,1))):_IOTPUT("device/i2c_a/2A",PS$)
100  PS$=CHR$(&H16)+CHR$(VAL(MID$(TM$,2,1))):_IOTPUT("device/i2c_a/2A",PS$)
110  PS$=CHR$(&H35)+CHR$(&H1):_IOTPUT("device/i2c_a/2A",PS$)
120  PS$=CHR$(&H14)+CHR$(VAL(MID$(TM$,4,1))):_IOTPUT("device/i2c_a/2A",PS$)
130  PS$=CHR$(&H13)+CHR$(VAL(MID$(TM$,5,1))):_IOTPUT("device/i2c_a/2A",PS$)
140  PS$=CHR$(&H32)+CHR$(&H1):_IOTPUT("device/i2c_a/2A",PS$)
150  PS$=CHR$(&H11)+CHR$(VAL(MID$(TM$,7,1))):_IOTPUT("device/i2c_a/2A",PS$)
160  PS$=CHR$(&H10)+CHR$(VAL(MID$(TM$,8,1))):_IOTPUT("device/i2c_a/2A",PS$)
170   GOTO 50
180 'Reinitialize console
190   _ANK:_IOTINIT():COLOR 15,4,7
