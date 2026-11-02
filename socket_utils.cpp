#include "socket_utils.h"

#include <cstddef>
#include <cerrno>
#include <sys/socket.h>

bool sendAll(int socketFd, const std::string &text)
{
    std::size_t totalSent = 0;

    while (totalSent < text.size())
    {
        ssize_t sent = send(
            socketFd,
            text.data() + totalSent,
            text.size() - totalSent,
            MSG_NOSIGNAL
        );

        if (sent == -1 && errno == EINTR)
        {
            continue;
        }

        if (sent <= 0)
        {
            return false;
        }

        totalSent += static_cast<std::size_t>(sent);
    }

    return true;
}