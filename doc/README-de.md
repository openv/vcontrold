***********************************************************
Aktuelle Informationen und Dokus finden sich im Wiki:
https://github.com/openv/vcontrold
***********************************************************
Die Software wird unter der GPL veroeffentlicht.
***********************************************************


vcontrold ist ein in C geschriebener Daemon, der die Kommunikation mit 
der Vito-Steuerung übernimmt.

Die Konfiguration erfolgt über XML-Dateien.
Der Daemon bietet eine ASCII-Socketschnittstelle, die mit telnet oder 
dem vclient Programm angespochen werden kann.

Der Quelltext kann im git Repository heruntergeladen werden.

	git clone https://github.com/openv/vcontrold.git vcontrold-code
    cd vcontrold-code
	mkdir ./build && cd ./build
    cmake ..
	make && make install


Die Konfiguration des Programms wird in zwei XML-Dateien vorgenommen:
vcontrold.xml : Programmspezifische Definitionen
vito.xml: Definition der Kommandos und Devices

