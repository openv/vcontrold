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
	sh ./auto-build.sh
	sh ./configure
	make && make install

Es exisitiert ein Makefile für x86 und den NSLU2 unter openwrt (Cross Compiling).
Benötigt wird die libxml2.

Die Konfiguration des Programms wird in zwei XML-Dateien vorgenommen:
vcontrold.xml : Programmspezifische Definitionen
vito.xml: Definition der Kommandos und Devices (hier im SVN)

Aufruf:
vcontrold --help
vcontrold	[-x|--xmlfile xml-file] [-d|--device <device>]
			[-l|--logfile <logfile>] [-p|--port port] [-s|--syslog]
			[-n|--nodaemon] [-i|--vsim] [-g|--debug]
			[-4|--inet4] [-6|--inet6]

	-x Pfad zur XML Konfigurationsdatei
	-d Device, entweder serielles Device oder IP-Adresse:Port bei Zugriff über ser2net
	-l Log-Datei
	-p TCP-Port, auf dem das CLI-Interface lauscht
	-s Logging in syslog
	-n kein fork, für Testzwecke
	-g erzeugt mehr debug Ausgaben
	-4 Nutze IPv4 (Voreingestellt für Rückwärtskompatibilität)
	-6 Nutze IPv6

CLI-Kommandos:
device: Name und ID des konfigurierten Devices
protocol: Name des Protokolls
commands: Für das device in der XML-Datei definierte Kommandos
detail <command>: detailinformationen zum Kommando
<command>: Führe Kommando aus
close: schließe Kommunikationskanal zur Analage (wird nach <comman> offen gehalten)
debug on|off:Zeigt Debug Meldungen (Kommunikation mit der Anlage) an und wieder ab.
unit on|off: Schaltet implizite Umrechnung in die definierte Einheit aus und wieder an.

-------------------------------------------------------------------------------------

vclient- Client Programm zu Kommunikation mit dem vcontrold.

Aufruf:

vclient --help
usage: vclient -h <ip:port> [-c <command1,command2,..>] [-f <commandfile>] [-s <csv-Datei>] [-t <Template-Datei>] [-o <outpout Datei> [-x <exec-Datei>] [-k] [-m] [-v]
or
usage: vclient --host <ip> --port <port> [--command <command1,command2,..>] [--commandfile <commandfile>] [--cvsfile <csv-Datei>] [--template <Template-Datei>] [--output <outpout Datei>] [--execute <exec-Datei>] [--cacti] [--munin] [--verbose] [command3 [command4] ...]

	-h|--host		<IPv4>:<Port> oder <IPv6> des vcontrold
	-p|--port		<Port> des vcontrold bei IPv6
	-c|--command	Liste von auszufuehrenden Kommandos, durch Komma getrennt
	-f|--commandfile	Optional Datei mit Kommandos, pro Zeile ein Kommando
	-s|--csvfile	Ausgabe des Ergebnisses im CSV Format zur Weiterverarbeitung
	-t|--template	Template, Variablen werden mit zurueckgelieferten Werten ersetzt.
	-o|--output		Output, der stdout Output wird in die angegebene Datei geschrieben
	-x|--execute	Das umgewandelte Template (-t) wird in die angegebene Datei geschrieben und anschliessend ausgefuehrt.
	-m|--munin		Munin Datalogger kompatibles Format; Einheiten und Details zu Fehler gehen verloren.
	-k|--cacti		Cacti Datalogger kompatibles Format; Einheiten und Details zu Fehler gehen verloren.
	-v|--verbose	Verbose Modus zum testen
	-4|--inet4		IPv4 wird bevorzugt
	-6|--inet6		IPv6 wird bevorzugt. Wird keine dieser Optionen angegben werden die OS default Einstellungen verwendet
	--help			Gibt diese Benutzungshinweise aus.


Template Modus
Im Template Modus (-t) wird ein Template eingelesen und die dort enthaltenen 
Variablen ersetzt. Die Ausgabe erfolgt dann auf stdout.
Damt ist es einfach möglich, die Messwerte der Heizung auszulesen und direkt 
in eine Datenbank zu schreiben.

Variable Funktion
$1..$n    Rueckgabewert gewandelt in Gleitkommazahl
$R1..$Rn  Rueckgabewert ungewandelt (Text)
$C1..$Cn  aufgerufenes Kommando
$E1..$En  Fehlermeldung pro Kommando

Einfaches Beispiel:
Datei sql.tmpl

cat sql.tmpl
INSERT INTO messwerte values (CURRENT_DATE,$1,$2);

Aufruf:

./vclient -h 127.0.0.1:1234 -t sql.tmpl -c gettempA,gettempWW
INSERT INTO messwerte values (CURRENT_DATE,-2.600000,54.299999);


Diese Werte können nun direkt über ein DB-cli in die Datenbank geschrieben werden.
Die Ausgabe von stderr sollte dafür umgeleitet werden:

./vclient -h 127.0.0.1:1234 -t sql.tmpl -c gettempA,gettempWW 2>/dev/null  |mysql -D vito

Wir der Schalter -o <datei> angegeben, so wird das Ergebnis in die angegebene 
Datei geschrieben.
Wird der Schalter -x <datei> verwendet, wird das Ergebnis in die Datei 
geschrieben und diese direkt ausgefuehrt.

Beispiel:
$ cat sh-example.tmpl
#!/bin/sh
echo "Das ist ein Beispiel eines ausfuerhbaren Scriptes"
echo "Kommando 1: $C1 Kommando 2: $C2"
echo "rrdb Update machen wir mit: update $db N:$1:$2"

nun setzen wir noch $db in der Shell:
export db='VITODB'

./vclient -h 127.0.0.1:1234 -c getTempA,getTempWW -t sh-example.tmpl -x sh-example.sh
Das ist ein Beispiel eines ausfuerhbaren Scriptes
Kommando 1: getTempA Kommando 2: getTempWW
rrdb Update machen wir mit: update VITODB N:-2.600000:54.299999


-------------------------------------------------------------------------------------

XML Konfig:
Die Konfiguration des Daemons erfolgt mit XML-Dateien.
Programmspezifische Konfigurationen sind in der datei vcontrold.xml angelegt, 
kommandospezifische Konfigurationen finden sich in der Datei vito.xml.
vcontrold.xml inkludiert die Datei vito.xml mittels XInclude.

Die Datei gliedert sich in drei Hauptabschnitte:
<unix>: Maschinennahe Konfigurationen
<units>: Einheiten zur implizierten Umrechnung, hierauf wird in der vito.xml mit 
dem <unit> Tag des Kommandos verwiesen.
<protocols>: Protokoll-spezifische Definition der protocmd-Kommados (in vito.xml). Hier werden die verschiedenen Definitionen für die Kommandos getaddr (und später setaddr) definiert).
Abschnitt units:
abbrev: Abkürzung der unit, auf dieses Abkürzung wird mit dem <unit> Tag in <comand> der vito.xml referenziert.
calc: Hier können arithmetische Ausdrücke zur Umrechnung angegeben werden. Zur Zeit sind + - * / ( ) in beliebiger Schachtelung möglich. Als Variablen stehen zur Verfügung:

    * B0...B9 -> zurückgelieferte Bytes der Abfrage
    * V -> Value unter Berücksichtigung des Types <type>

type: Typ der zurückgelieferten Bytes. Zur zeit sind implementiert:

    * char -> 1 Byte (hier ist V und B0 gleich)
    * short -> 2 Bytes, intern signed short
    * int -> 4 Bytes, intern signed int
    * später: date für 8 byte Datum
    * ...

entity:
Zusatztext, der hinter dem Ergebnis ausgegeben wird
Es können beliebige Units definiert werden.
Abschnitt protocols:

macro: Definition von zu sendenden Befehlen, die unter dem Makro Name in <send> des Kommandos in diesem Abschnitt angegeben werden können.
commands: Defintion von Protokoll-Kommandos, die zur direkten Kommunikation mit der Anlage genutzt werden
send: Zu sendende Byte-Kommandos. Hier können die Makro Defintionen des Protokolls genutzt werden.
Folgende Kommandos stehen zur Verfügung:

    * SEND <BYTES in Hex>
    * WAIT <BYTES in Hex>
    * RECV <Anzahl Bytes> <optional Unit aus <unit><abbrev>>
    * PAUSE <ms>>

Als Variablen aus dem command Abschnitt der vito.xml stehen zur Verfügung:

    * $addr -> vito.xml-><command><addr>
    * $len ->vito.xml-><command><len>
    * $unit >vito.xml-><command><unit>

