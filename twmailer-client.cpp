#include "socket_utils.h"
#include <arpa/inet.h>
#include <climits>
#include <cstdio>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

// Prueft eine ganze Zahl innerhalb des erlaubten Bereichs.
bool parseNumber(const std::string &text, int minimum, int maximum, int &number)
{
    if (text.empty())
    {
        return false;
    }

    int result = 0;

    for (char character : text)
    {
        if (character < '0' || character > '9')
        {
            return false;
        }

        int digit = character - '0';

        if (result > (maximum - digit) / 10)
        {
            return false;
        }

        result = result * 10 + digit;
    }

    if (result < minimum)
    {
        return false;
    }

    number = result;
    return true;
}

// Gibt eine Aufforderung aus und liest eine Eingabezeile.
bool readInput(const std::string &prompt, std::string &text)
{
    std::cout << prompt << std::flush;
    return static_cast<bool>(std::getline(std::cin, text));
}

// Benutzernamen: 1 bis 8 Zeichen, nur a-z und 0-9.
bool readUsername(const std::string &prompt, std::string &username)
{
    while (readInput(prompt, username))
    {
        bool valid = !username.empty() && username.size() <= 8;

        for (char character : username)
        {
            if (!((character >= 'a' && character <= 'z') ||
                  (character >= '0' && character <= '9')))
            {
                valid = false;
            }
        }

        if (valid)
        {
            return true;
        }

        std::cout << "Bitte 1 bis 8 Zeichen verwenden: a-z und 0-9.\n";
    }

    return false;
}

// Liest OK oder ERR. Auch nach ERR bleibt die Sitzung offen.
bool receiveStatus(int socketFd)
{
    std::string response;

    if (!receiveLine(socketFd, response))
    {
        return false;
    }

    if (response != "OK" && response != "ERR")
    {
        std::cerr << "Unerwartete Serverantwort: " << response << '\n';
        return false;
    }

    std::cout << response << '\n';
    return true;
}

// Erfasst eine Nachricht und sendet sie an den Server.
bool sendMessage(int socketFd)
{
    std::string sender;
    std::string receiver;
    std::string subject;
    std::string line;

    if (!readUsername("Absender: ", sender) ||
        !readUsername("Empfaenger: ", receiver))
    {
        return false;
    }

    while (true)
    {
        if (!readInput("Betreff (max. 80 Zeichen): ", subject))
        {
            return false;
        }

        if (subject.size() <= 80)
        {
            break;
        }

        std::cout << "Der Betreff ist zu lang.\n";
    }

    std::string request =
        "SEND\n" + sender + "\n" + receiver + "\n" + subject + "\n";

    std::cout << "Nachricht eingeben. Ein einzelner Punkt beendet die Eingabe.\n";

    while (true)
    {
        if (!std::getline(std::cin, line))
        {
            return false;
        }

        request += line + "\n";

        if (line == ".")
        {
            break;
        }
    }

    return sendAll(socketFd, request) && receiveStatus(socketFd);
}

// Fordert die Betreffzeilen aller Nachrichten eines Benutzers an.
bool listMessages(int socketFd)
{
    std::string username;
    std::string line;

    if (!readUsername("Benutzername: ", username) ||
        !sendAll(socketFd, "LIST\n" + username + "\n") ||
        !receiveLine(socketFd, line))
    {
        return false;
    }

    int count;

    if (!parseNumber(line, 0, INT_MAX, count))
    {
        std::cerr << "Ungueltige Nachrichtenanzahl vom Server.\n";
        return false;
    }

    std::cout << count << " Nachricht(en)\n";

    for (int index = 0; index < count; ++index)
    {
        if (!receiveLine(socketFd, line))
        {
            return false;
        }

        std::cout << index + 1 << ": " << line << '\n';
    }

    return true;
}

// READ und DEL benoetigen Benutzername und Nachrichtennummer.
bool readOrDeleteMessage(int socketFd, const std::string &command)
{
    std::string username;
    std::string numberText;
    std::string line;

    if (!readUsername("Benutzername: ", username))
    {
        return false;
    }

    int number;

    while (true)
    {
        if (!readInput("Nachrichtennummer (ab 1): ", numberText))
        {
            return false;
        }

        if (parseNumber(numberText, 1, INT_MAX, number))
        {
            break;
        }

        std::cout << "Bitte eine gueltige positive Zahl eingeben.\n";
    }

    std::string request =
        command + "\n" + username + "\n" + std::to_string(number) + "\n";

    if (!sendAll(socketFd, request))
    {
        return false;
    }

    if (command == "DEL")
    {
        return receiveStatus(socketFd);
    }

    // Ab hier wird die Antwort auf READ verarbeitet.
    if (!receiveLine(socketFd, line))
    {
        return false;
    }

    if (line == "ERR")
    {
        std::cout << "ERR\n";
        return true;
    }

    if (line != "OK")
    {
        std::cerr << "Unerwartete Serverantwort: " << line << '\n';
        return false;
    }

    std::cout << "OK\n";

    // Zuerst Absender, Empfaenger und Betreff empfangen.
    for (int field = 0; field < 3; ++field)
    {
        if (!receiveLine(socketFd, line))
        {
            return false;
        }

        std::cout << line << '\n';
    }

    // Danach den Nachrichtentext bis zur Punktzeile empfangen.
    while (receiveLine(socketFd, line))
    {
        if (line == ".")
        {
            return true;
        }

        std::cout << line << '\n';
    }

    return false;
}

int main(int argc, char *argv[])
{
    // IP-Adresse und Port aus dem Programmaufruf einlesen.
    if (argc != 3)
    {
        std::cerr << "Aufruf: " << argv[0] << " <ip> <port>\n";
        return 1;
    }

    int port;

    if (!parseNumber(argv[2], 1, 65535, port))
    {
        std::cerr << "Ungueltiger Port: erlaubt sind 1 bis 65535.\n";
        return 1;
    }

    // Serveradresse vorbereiten.
    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(static_cast<unsigned short>(port));

    if (inet_pton(AF_INET, argv[1], &serverAddress.sin_addr) != 1)
    {
        std::cerr << "Ungueltige IPv4-Adresse.\n";
        return 1;
    }

    // TCP-Socket erstellen.
    int socketFd = socket(AF_INET, SOCK_STREAM, 0);

    if (socketFd == -1)
    {
        std::perror("socket");
        return 1;
    }

    // Verbindung zum Server herstellen.
    if (connect(socketFd,
                reinterpret_cast<sockaddr *>(&serverAddress),
                sizeof(serverAddress)) == -1)
    {
        std::perror("connect");
        close(socketFd);
        return 1;
    }

    std::cout << "Verbunden. Befehle: SEND, LIST, READ, DEL, QUIT\n";

    std::string command;
    int exitCode = 0;

    // Befehle einlesen und die passende Funktion aufrufen.
    while (true)
    {
        if (!readInput("Befehl: ", command) || command == "QUIT")
        {
            if (!sendAll(socketFd, "QUIT\n"))
            {
                std::cerr << "QUIT konnte nicht gesendet werden.\n";
                exitCode = 1;
            }

            break;
        }

        bool success;

        if (command == "SEND")
        {
            success = sendMessage(socketFd);
        }
        else if (command == "LIST")
        {
            success = listMessages(socketFd);
        }
        else if (command == "READ" || command == "DEL")
        {
            success = readOrDeleteMessage(socketFd, command);
        }
        else
        {
            std::cout << "Unbekannter Befehl. Erlaubt: "
                      << "SEND, LIST, READ, DEL, QUIT\n";
            continue;
        }

        if (!success)
        {
            // Bei Eingabeende nach Moeglichkeit sauber abmelden.
            if (std::cin.eof())
            {
                if (!sendAll(socketFd, "QUIT\n"))
                {
                    std::cerr << "QUIT konnte nicht gesendet werden.\n";
                    exitCode = 1;
                }
            }
            else
            {
                std::cerr << "Fehler bei Eingabe, Verbindung "
                          << "oder Serverantwort.\n";
                exitCode = 1;
            }

            break;
        }
    }

    // Verbindung schliessen und Programm beenden.
    close(socketFd);
    return exitCode;
}