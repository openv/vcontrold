Legacy-Beispiele
================

Die Dateien in diesem Verzeichnis dokumentieren historische Installationen
und veranschaulichen unter anderem das Loggen von Betriebsdaten in eine RRD.
Sie sind keine direkt ausführbaren Produktionsvorlagen.

Mehrere Skripte enthalten absichtlich alte, anlagenspezifische Pfade unter
``/opt``, feste Datenbanknamen oder veraltete Init-System-Annahmen. Vor einer
Verwendung müssen Pfade, Benutzer, Berechtigungen, Kommandonamen und
Abhängigkeiten an das Zielsystem angepasst werden. Für neue Installationen
sind die Dateien unter ``xml/300`` beziehungsweise ``xml/kw`` sowie ein
systemd-Service mit dem tatsächlich gewählten Installationspräfix der
Ausgangspunkt.

Aktuelle Informationen finden sich im OpenV-Wiki:
https://github.com/openv/openv/wiki

Die Software wird unter der GPL veröffentlicht.
