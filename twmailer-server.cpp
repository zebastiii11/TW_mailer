#include "socket_utils.h"
#include <algorithm>
#include <arpa/inet.h>
#include <climits>
#include <csignal>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>
#include <vector>


namespace fs = std::filesystem;
const std::size_t FILE_NUMBER_DIGITS = 10;

// selbige function wie im client
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

bool isValidUsername(const std::string &username)
{
    if (username.empty() || username.size() > 8)
    {
        return false;
    }
 
    for (char character : username)
    {
        if (!((character >= 'a' && character <= 'z') ||
              (character >= '0' && character <= '9')))
        {
            return false;
        }
    }
 
    return true;
}

// Später hier LDAP Authentifizierung hinzufügen
// Für jetzt einfach leer gelassen
bool checkCredentials(const std::string &username, const std::string &password)
{
    //unbenutzt on purpose, damit compiler keine warning schmeisst
    std::string user = username;
    std::string pw = password;

    return true;
}

fs::path getInbox(const std::string &spoolDirectory, const std::string &username)
{
    return fs::path(spoolDirectory) / username;
}

bool readSubject(const fs::path &messageFile, std::string &subject)
{
    std::ifstream file(messageFile);
    std::string sender;
    std::string receiver;
 
    return static_cast<bool>(std::getline(file, sender) && std::getline(file, receiver) && std::getline(file, subject));
}



std::vector<fs::path> getMessageFiles(const fs::path &inbox)
{
    std::vector<fs::path> files;
    std::error_code error;
 
    // Unbekannter Benutzer: Postfach existiert nicht, Liste bleibt leer.
    if (!fs::is_directory(inbox, error))
    {
        return files;
    }
 
    for (const fs::directory_entry &entry : fs::directory_iterator(inbox, error))
    {
        const fs::path &path = entry.path();
        std::string stem = path.stem().string();
 
        // Nur nachrichten mit .msg beachten
        if (path.extension() != ".msg" || stem.size() != FILE_NUMBER_DIGITS ||
            !std::all_of(stem.begin(), stem.end(),
                         [](char c) { return c >= '0' && c <= '9'; }))
        {
            continue;
        }
 
        files.push_back(path);
    }
 
    // Gleich lange Namen mit fuehrenden Nullen: alphabetisch = numerisch.
    std::sort(files.begin(), files.end());
    return files;
}

// Ermittelt den Dateinamen fuer die naechste Nachricht: hoechste Nummer + 1.
bool getNextMessagePath(const fs::path &inbox, fs::path &nextPath)
{
    std::vector<fs::path> files = getMessageFiles(inbox);
    int nextNumber = 1;
 
    if (!files.empty())
    {
        int lastNumber;
 
        if (!parseNumber(files.back().stem().string(), 1, INT_MAX - 1, lastNumber))
        {
            return false;
        }
 
        nextNumber = lastNumber + 1;
    }
 
    // Nummer mit fuehrenden Nullen auf 10 Stellen auffuellen.
    std::string name = std::to_string(nextNumber);
    name.insert(0, FILE_NUMBER_DIGITS - name.size(), '0');
 
    nextPath = inbox / (name + ".msg");
    return true;
}

bool handleList(int clientFd, const std::string &spoolDirectory, const std::string &loggedInUser)
{
    if (loggedInUser.empty())
    {
        return sendAll(clientFd, "ERR\n");
    }
 
    std::vector<fs::path> files = getMessageFiles(getInbox(spoolDirectory, loggedInUser));
 

    std::string response = std::to_string(files.size()) + "\n";
 
    for (const fs::path &file : files)
    {
        std::string subject;
 
       
        if (!readSubject(file, subject))
        {
            std::cerr << "Betreff nicht lesbar: " << file << '\n';
            subject.clear();
        }
 
        response += subject + "\n";
    }
 
    return sendAll(clientFd, response);
}

bool handleRead(int clientFd, const std::string &spoolDirectory,
                const std::string &loggedInUser)
{
    std::string numberText;
 
    if (!receiveLine(clientFd, numberText))
    {
        return false;
    }
 
    if (loggedInUser.empty())
    {
        return sendAll(clientFd, "ERR\n");
    }
 
    std::vector<fs::path> files =
        getMessageFiles(getInbox(spoolDirectory, loggedInUser));
 
    // Nummer muss zwischen 1 und der Anzahl der Nachrichten liegen.
    int number;
 
    if (!parseNumber(numberText, 1, INT_MAX, number) ||
        static_cast<std::size_t>(number) > files.size())
    {
        return sendAll(clientFd, "ERR\n");
    }

    std::ifstream file(files[number - 1]);
 
    if (!file)
    {
        std::cerr << "Nachricht nicht lesbar: " << files[number - 1] << '\n';
        return sendAll(clientFd, "ERR\n");
    }
 
    if (!sendAll(clientFd, "OK\n"))
    {
        return false;
    }

    std::string line;
 
    while (std::getline(file, line))
    {
        if (!sendAll(clientFd, line + "\n"))
        {
            return false;
        }
    }
 

    return sendAll(clientFd, ".\n");
}

bool handleLogin(int clientFd, std::string &loggedInUser)
{
    std::string username;
    std::string password;
 
    // Vollständigen Befehl lesen zuerst
    if (!receiveLine(clientFd, username) || !receiveLine(clientFd, password))
    {
        return false;
    }
 
    // Nur ein login pro session pro client
    if (!loggedInUser.empty())
    {
        return sendAll(clientFd, "ERR\n");
    }
    
    //Prüfe ob username valid und authentifizierung
    if (!isValidUsername(username) || !checkCredentials(username, password))
    {
        return sendAll(clientFd, "ERR\n");
    }
 
    loggedInUser = username;
    std::cout << "Login: " << loggedInUser << std::endl;
    return sendAll(clientFd, "OK\n");
}



bool handleSend(int clientFd, const std::string &spoolDirectory, const std::string &loggedInUser)
{
    std::string receiver;
    std::string subject;
    std::string line;
 
    if (!receiveLine(clientFd, receiver) || !receiveLine(clientFd, subject))
    {
        return false;
    }

    bool valid = !loggedInUser.empty() && isValidUsername(receiver) && subject.size() <= 80;
 
    fs::path finalPath;
    fs::path tempPath;
    std::ofstream file;
 
    if (valid)
    {
        fs::path inbox = fs::path(spoolDirectory) / receiver;
        std::error_code error;
        fs::create_directories(inbox, error);
 
        if (error || !getNextMessagePath(inbox, finalPath))
        {
            std::cerr << "Postfach von " << receiver << " nicht verwendbar.\n";
            valid = false;
        }
        else
        {
            // Zuerst in eine .tmp-Datei schreiben

            tempPath = finalPath;
            tempPath.replace_extension(".tmp");
            file.open(tempPath);
 
            if (!file)
            {
                std::cerr << "Datei konnte nicht angelegt werden: " << tempPath << '\n';
                valid = false;
            }
            else
            {
                file << loggedInUser << '\n' << receiver << '\n' << subject << '\n';
            }
        }
    }
 
    // Nachrichtentext zeilenweise lesen und direkt in die Datei
    // schreiben, so bleibt auch eine sehr grosse Nachricht nie
    // komplett im Speicher.
    bool connectionOk = true;
 
    while (true)
    {
        if (!receiveLine(clientFd, line))
        {
            connectionOk = false;
            valid = false;
            break;
        }
 
        if (line == ".")
        {
            break;
        }
 
        if (valid)
        {
            file << line << '\n';
        }
    }
 
    // Überprüfung ob File richtig geschrieben wrude
    if (file.is_open())
    {
        file.close();
 
        if (!file)
        {
            std::cerr << "Fehler beim Schreiben von " << tempPath << '\n';
            valid = false;
        }
    }
 
    // Fertige Nachricht unter ihrem Namen ablegen
    if (valid)
    {
        std::error_code error;
        fs::rename(tempPath, finalPath, error);
 
        if (error)
        {
            std::cerr << "Nachricht konnte nicht gespeichert werden: "
                      << error.message() << '\n';
            valid = false;
        }
    }
 
    // Bei einem Fehler die angefangene Datei wieder entfernen.
    if (!valid && !tempPath.empty())
    {
        std::error_code error;
        fs::remove(tempPath, error);
    }
 
    if (!connectionOk)
    {
        return false;
    }
 
    if (valid)
    {
        std::cout << "SEND: " << loggedInUser << " -> " << receiver << std::endl;
    }
 
    return sendAll(clientFd, valid ? "OK\n" : "ERR\n");
}

bool handleDel(int clientFd, const std::string &spoolDirectory,
               const std::string &loggedInUser)
{
    std::string numberText;
 
    if (!receiveLine(clientFd, numberText))
    {
        return false;
    }
 
    if (loggedInUser.empty())
    {
        return sendAll(clientFd, "ERR\n");
    }
 
    std::vector<fs::path> files =
        getMessageFiles(getInbox(spoolDirectory, loggedInUser));
 
    // Gleiche Pruefung wie bei READ: 1 bis Anzahl der Nachrichten.
    int number;
 
    if (!parseNumber(numberText, 1, INT_MAX, number) ||
        static_cast<std::size_t>(number) > files.size())
    {
        return sendAll(clientFd, "ERR\n");
    }

    std::error_code error;
 
    if (!fs::remove(files[number - 1], error) || error)
    {
        std::cerr << "Nachricht konnte nicht geloescht werden: "
                  << files[number - 1] << '\n';
        return sendAll(clientFd, "ERR\n");
    }
 
    std::cout << "DEL: " << loggedInUser << " Nachricht " << number << std::endl;
    return sendAll(clientFd, "OK\n");
}

// bearbeitet befehle des clients
void handleClient(int clientFd, const std::string &spoolDirectory)
{
    std::string loggedInUser;
    std::string command;

    while (receiveLine(clientFd, command))
    {
        bool success;
 
        if (command == "QUIT")
        {
            return;
        } 
        else if (command == "LOGIN")
        {
            success = handleLogin(clientFd, loggedInUser);
        }
        else if (command == "SEND")
        {
            success = handleSend(clientFd, spoolDirectory, loggedInUser);
        }
        else if (command == "LIST")
        {
            success = handleList(clientFd, spoolDirectory, loggedInUser);
        }
        else if (command == "READ")
        {
            success = handleRead(clientFd, spoolDirectory, loggedInUser);
        }
        else if (command == "DEL")
        {
            success = handleDel(clientFd, spoolDirectory, loggedInUser);
        }
        else
        {
           
            success = sendAll(clientFd, "ERR\n");
        }
 
        if (!success)
        {
            return;
        }
    }

    
}

int main(int argc, char *argv[])
{
    // Port und Mail-Spool-Verzeichnis aus dem Programmaufruf einlesen.
    if (argc != 3)
    {
        std::cerr << "Aufruf: " << argv[0] << " <port> <mail-spool-verzeichnis>\n";
        return 1;
    }

    int port;

    if (!parseNumber(argv[1], 1, 65535, port))
    {
        std::cerr << "Ungueltiger Port: erlaubt sind 1 bis 65535.\n";
        return 1;
    }

    std::string spoolDirectory = argv[2];
    std::error_code error;
    std::filesystem::create_directories(spoolDirectory, error);

    if (error)
    {
        std::cerr << "Mail-Spool-Verzeichnis konnte nicht angelegt werden: "
                  << error.message() << '\n';
        return 1;
    }

    // abgebrochener client beendet nicht den server
    std::signal(SIGPIPE, SIG_IGN);

    // TCP-Socket erstellen.
    int listenFd = socket(AF_INET, SOCK_STREAM, 0);

    if (listenFd == -1)
    {
        std::perror("socket");
        return 1;
    }

    // Port wiederverwendent
    int reuse = 1;

    if (setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR,
                   &reuse, sizeof(reuse)) == -1)
    {
        std::perror("setsockopt");
        close(listenFd);
        return 1;
    }

    // Auf allen Netzwerkschnittstellen am angegebenen Port lauschen.
    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddress.sin_port = htons(static_cast<unsigned short>(port));

    if (bind(listenFd, reinterpret_cast<sockaddr *>(&serverAddress), sizeof(serverAddress)) == -1)
{
    std::perror("bind");
    close(listenFd);
    return 1;
}
    if (listen(listenFd, 5) == -1)
    {
        std::perror("listen");
        close(listenFd);
        return 1;
    }

    std::cout << "Server laeuft auf Port " << port << "." << std::endl;

    while (true)
    {
        sockaddr_in clientAddress{};
        socklen_t addressLength = sizeof(clientAddress);

        int clientFd = accept(listenFd, reinterpret_cast<sockaddr *>(&clientAddress), &addressLength);

        if (clientFd == -1)
        {
            //Fehler beim accepten
            std::perror("accept");
            continue;
        }

        // IP-Adresse von client herausfinden
        char clientIp[INET_ADDRSTRLEN] = "unbekannt";
        inet_ntop(AF_INET, &clientAddress.sin_addr, clientIp, sizeof(clientIp));

        std::cout << "Client verbunden: " << clientIp << std::endl;

        handleClient(clientFd, spoolDirectory);

        close(clientFd);
        std::cout << "Client getrennt: " << clientIp << std::endl;
    }

    close(listenFd);
    return 0;
}