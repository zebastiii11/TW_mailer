# TW-Mailer Basic

## Aufteilung

- Sebastian: Client, gemeinsame Netzwerkfunktionen, Makefile und Testfälle.
- Partner: Server inklusive Nachrichtenspeicherung.
- Gemeinsam: Integrationstests und einseitiges PDF-Protokoll.

## Gemeinsame Netzwerkfunktionen

Im Server einbinden:

#include "socket_utils.h"

Beim Kompilieren wird socket_utils.cpp mitübersetzt.

### sendAll(socketFd, text)

- Sendet den gesamten übergebenen Text.
- Ergänzt keinen Zeilenumbruch: "\n" selbst anhängen.
- true = erfolgreich gesendet, false = Fehler.

### receiveLine(socketFd, line)

- Empfängt eine vollständige Zeile.
- Speichert sie ohne das abschließende "\n" in line.
- Eine leere Zeile ist gültig und liefert true.
- false = Fehler oder Verbindungsende vor einer vollständigen Zeile.

## Erwartetes Serververhalten

- IPv4 und TCP, Zeilenende "\n".
- Keine Begrüßungsnachricht beim Verbindungsaufbau.
- Mehrere Befehle pro Verbindung.
- Ein Client wird nach dem anderen bearbeitet.
- QUIT schließt die Clientverbindung ohne Antwort.
- Danach kann der Server den nächsten Client annehmen.
- Alle Eingaben zusätzlich serverseitig prüfen.

## Antworten und Nummerierung

SEND und DEL:
- Genau "OK\n" oder "ERR\n".

LIST:
- Zuerst die Nachrichtenanzahl.
- Danach genau diese Anzahl Betreffzeilen.
- Keine zusätzliche Abschlusszeile.
- Unbekannter Benutzer oder leeres Postfach: "0\n".

Der Client nummeriert die LIST-Einträge ab 1.
READ und DEL müssen dieselbe aktuelle Reihenfolge verwenden.
Nach dem Löschen kann sich die Nummerierung verschieben.

READ bei Erfolg:
OK
<Absender>
<Empfänger>
<Betreff>
<Nachrichtentext>
.

Keine zusätzliche SEND-Zeile.
Keine zusätzliche Leerzeile nach der abschließenden Punktzeile.
Bei einem Fehler nur "ERR\n".

## Bauen unter Linux

Nur Client:
make twmailer-client

Beide Programme, sobald der Server implementiert ist:
make

Erzeugte Programme entfernen:
make clean

## Starten

Server:
./twmailer-server 6543 ./mailspool

Client:
./twmailer-client 127.0.0.1 6543

Für den ersten Verbindungstest beide Programme im selben Container
in getrennten Terminals starten.