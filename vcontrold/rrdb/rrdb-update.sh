if  [ "x" != x ]; then
	echo -n `date`  >>/tmp/vc-err.txt
	echo "es ist ein Fehler aufgetreten: getDevType:" >>/tmp/vc-err.txt
	exit 1;
fi
if  [ "x" != x ]; then
	echo -n `date`  >>/tmp/vc-err.txt
	echo "es ist ein Fehler aufgetreten: getTempA:" >>/tmp/vc-err.txt
	exit 1;
fi
if  [ "x" != x ]; then
	echo -n `date`  >>/tmp/vc-err.txt
	echo "es ist ein Fehler aufgetreten: getTempWWist:" >>/tmp/vc-err.txt
	exit 1;
fi
if  [ "x" != x ]; then
	echo -n `date`  >>/tmp/vc-err.txt
	echo "es ist ein Fehler aufgetreten: getTempWWsoll:" >>/tmp/vc-err.txt
	exit 1;
fi
if  [ "x" != x ]; then
	echo -n `date`  >>/tmp/vc-err.txt
	echo "es ist ein Fehler aufgetreten: getTempKist:" >>/tmp/vc-err.txt
	exit 1;
fi
if  [ "x" != x ]; then
	echo -n `date`  >>/tmp/vc-err.txt
	echo "es ist ein Fehler aufgetreten: getTempKsoll:" >>/tmp/vc-err.txt
	exit 1;
fi
if  [ "x" != x ]; then
	echo -n `date`  >>/tmp/vc-err.txt
	echo "es ist ein Fehler aufgetreten: unit off:" >>/tmp/vc-err.txt
	exit 1;
fi
if  [ "x" != x ]; then
	echo -n `date`  >>/tmp/vc-err.txt
	echo "es ist ein Fehler aufgetreten: getBetriebArtM1:" >>/tmp/vc-err.txt
	exit 1;
fi
if  [ "x" != x ]; then
	echo -n `date`  >>/tmp/vc-err.txt
	echo "es ist ein Fehler aufgetreten: unit on:" >>/tmp/vc-err.txt
	exit 1;
fi
if  [ "x" != x ]; then
	echo -n `date`  >>/tmp/vc-err.txt
	echo "es ist ein Fehler aufgetreten: getPumpeSollM1:" >>/tmp/vc-err.txt
	exit 1;
fi
BA=`echo 03 |cut -d ' ' -f 1` 
ST=`echo 21639.000000 h|cut -d '.' -f 1` 

LD_LIBRARY_PATH=':/opt/lib:/opt/usr/lib:/opt/usr/lib:/opt/usr/lib'
export LD_LIBRARY_PATH


rrdtool update /home/marcus/rrdb/vito.rrd N:15.500000:56.500000:50.000000:33.500000:39.000000:$BA:33.000000:0:0:$ST
