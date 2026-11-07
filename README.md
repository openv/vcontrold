# :fire: vcontrold

_vcontrold_ is a software daemon written in C for communication with the "Optolink" interface of Viessmann Vito heating controllers.

For building and installation instructions see `doc/INSTALL.md`.

[Binary packages](https://github.com/openv/vcontrold/releases) are available for different platforms.

Please visit the [OpenV Wiki](https://github.com/openv/openv/wiki/) for in-depth info and examples.

## Security model

`vcontrold` is designed for a trusted local network. Its TCP protocol does
not authenticate clients and does not encrypt traffic. Every client that can
reach the listener can execute the commands exposed by the active XML
configuration. This includes write commands when `readonly` is disabled and
the raw mode when it is enabled in the daemon.

Bind the listener only to an appropriate local address and use host or network
firewall rules when untrusted clients can reach the machine. Enable
`readonly` unless writes are explicitly required. Treat XML command
definitions as privileged configuration: an incorrect address, length, unit
or conversion can change controller state.

Configuration reloads are transactional for device, protocol, command and
unit data: an invalid candidate keeps the previous configuration active.
Listener address, TCP port, daemon user/group, PID file and logging
destination are process-level settings and require a service restart to take
effect reliably.

## DE

_vcontrold_ ist ein in C geschriebener Software-Daemon zur Kommunikation mit der „Optolink“-Schnittstelle von Viessmann-Vito-Heizungssteuerungen.

Der Build-Prozess und die Installation sind kurz unter `doc/INSTALL.md` beschrieben.

Für einige Plattformen sind kompilierte [Installations-Pakete](https://github.com/openv/vcontrold/releases) verfügbar.

Infos zur Einrichtung und Benutzung sind im [OpenV-Wiki](https://github.com/openv/openv/wiki/) beschrieben.

## Sicherheitsmodell

`vcontrold` ist für ein vertrauenswürdiges lokales Netz vorgesehen. Das
TCP-Protokoll besitzt weder Client-Authentifizierung noch Verschlüsselung.
Jeder erreichbare Client kann die Kommandos der aktiven XML-Konfiguration
aufrufen. Bei deaktiviertem `readonly` betrifft dies auch Set-Kommandos; ein
freigeschalteter Raw-Modus erlaubt unmittelbaren Protokollzugriff.

Der Listener sollte nur an eine geeignete lokale Adresse gebunden und bei
Bedarf durch Host- oder Netz-Firewallregeln geschützt werden. `readonly`
sollte aktiv bleiben, solange Schreibzugriffe nicht ausdrücklich benötigt
werden. XML-Kommandodefinitionen sind privilegierte Konfiguration: Falsche
Adressen, Längen, Units oder Umrechnungen können den Anlagenzustand ändern.

Reloads übernehmen Geräte-, Protokoll-, Kommando- und Unit-Daten
transaktional; eine ungültige neue Konfiguration lässt die bisherige aktiv.
Listener-Adresse, TCP-Port, Daemon-Benutzer/-Gruppe, PID-Datei und Logziel
sind Prozessparameter und benötigen für eine zuverlässige Änderung einen
Dienstneustart.
