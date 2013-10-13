LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/opt/usr/lib
export LD_LIBRARY_PATH

/opt/usr/bin/rrdtool graph /tmp/vito.png \
--start end-1d  --end now --width 800 --height 300 \
--title 'Vito-Messwerte' \
DEF:tempA=/extern/rrdb/vito.rrd:tempA:AVERAGE \
DEF:tempWWist=/extern/rrdb/vito.rrd:tempWWist:AVERAGE \
DEF:tempKist=/extern/rrdb/vito.rrd:tempKist:AVERAGE \
DEF:tempKsoll=/extern/rrdb/vito.rrd:tempKsoll:AVERAGE \
DEF:PumpeSollM1=/extern/rrdb/vito.rrd:PumpeSollM1:AVERAGE \
DEF:PumpeStatusM1=/extern/rrdb/vito.rrd:PumpeStatusM1:AVERAGE \
DEF:BetriebsArt=/extern/rrdb/vito.rrd:BetriebsArt:AVERAGE \
DEF:ExtBA=/extern/rrdb/vito.rrd:ExtBA:AVERAGE \
DEF:BrennerStatus=/extern/rrdb/vito.rrd:BrennerStatus:AVERAGE \
DEF:PumpeStatusZirku=/extern/rrdb/vito.rrd:PumpeStatusZirku:AVERAGE \
DEF:VentilStatus=/extern/rrdb/vito.rrd:getVentilStatus:AVERAGE \
CDEF:PSM1=PumpeStatusM1,30,* \
CDEF:BA=BetriebsArt,10,* \
CDEF:EBA=ExtBA,25,* \
CDEF:BS=BrennerStatus,40,* \
CDEF:VS=VentilStatus,60,* \
CDEF:PS=PumpeStatusZirku,70,* \
LINE1:tempA#0000FF:tempA \
LINE1:tempWWist#FF00FF:tempWWist \
LINE1:tempKist#00FFFF:tempKist \
LINE1:tempKsoll#CCCCCC:tempKsoll \
LINE1:BA#555555:Betriebsart \
LINE1:EBA#779922:Ext.Umschaltung \
LINE1:BS#11ABCD:Brennerstatus \
LINE1:VS#23AA77:Ventilstatus \
LINE1:PS#880044:Zirku-Pumpe \
VDEF:tempAmax=tempA,MAXIMUM \
VDEF:tempAmin=tempA,MINIMUM \
VDEF:tempWWmax=tempWWist,MAXIMUM \
VDEF:tempWWmin=tempWWist,MINIMUM \
VDEF:BSmax=BS,MAXIMUM \
VDEF:VSmax=VS,MAXIMUM \
COMMENT:"\l" \
COMMENT:"Aussentemp." \
GPRINT:tempAmin:"    Min. %02.1lf Grad C" \
GPRINT:tempAmax:"Max. %02.1lf Grad C" \
COMMENT:"\l" \
COMMENT:"Warmwassertemp." \
GPRINT:tempWWmin:"Min. %02.1lf Grad C" \
GPRINT:tempWWmax:"Max. %02.1lf Grad C" \
COMMENT:"\l" \
GPRINT:BSmax:"Brenner %02lf" \
GPRINT:VSmax:"Venitl %02lf" 

