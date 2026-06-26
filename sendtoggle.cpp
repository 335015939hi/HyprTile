#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <unistd.h>
#include <iostream>

int main() {
    int fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0) return 1;

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, "/tmp/launcher.sock");

    const char *msg = "toggle";
    sendto(fd, msg, strlen(msg), 0,
           (struct sockaddr*)&addr, sizeof(addr));

    close(fd);
    return 0;
}
