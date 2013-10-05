/opt/usr/bin/rrdtool create /extern/rrdb/vito.rrd --step 300 \
DS:tempA:GAUGE:700:-50:100 \
DS:tempWWist:GAUGE:700:0:100 \
DS:tempWWsoll:GAUGE:700:0:100 \
DS:tempKist:GAUGE:700:0:100 \
DS:tempKsoll:GAUGE:700:0:100 \
DS:BetriebsArt:GAUGE:700:0:5 \
DS:PumpeSollM1:GAUGE:700:0:100 \
DS:ExtBA:GAUGE:700:0:1 \
DS:PumpeStatusM1:GAUGE:700:0:1 \
DS:BrennerStunden1:COUNTER:700:0:100000 \
DS:TempRaumNorSollM1:GAUGE:700:0:100 \
DS:TempRaumRedSollM1:GAUGE:700:0:100 \
DS:BrennerStatus:GAUGE:700:0:1 \
DS:PumpeStatusZirku:GAUGE:700:0:1 \
DS:getVentilStatus:GAUGE:700:0:1 \
RRA:AVERAGE:0.5:1:2020 \
RRA:MIN:0.5:12:2400 \
RRA:MAX:0.5:12:2400 \
RRA:AVERAGE:0.5:12:2400
