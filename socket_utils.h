#ifndef SOCKET_UTILS_H
#define SOCKET_UTILS_H

#include <string>

// Sendet den gesamten Text, ohne selbst einen Zeilenumbruch anzuhängen.
// true = erfolgreich, false = Fehler
bool sendAll(int socketFd, const std::string &text);

// Empfängt eine Zeile und entfernt das abschließende '\n'.
// true = vollständige Zeile, false = Verbindungsende oder Fehler
bool receiveLine(int socketFd, std::string &line);

#endif