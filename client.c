#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

int main(int argc, char *argv[])
{
    struct sockaddr_in address;
    int sock_fd;
    char buf[1024];

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        return 1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1 aka localhost
    address.sin_port = htons(7878);
    // ^ to avoid conflicts with others, change the port number
    // to something else. Reflect that change in server.c

    if (-1 == connect(sock_fd, (struct sockaddr *)&address, sizeof(address))) {
        perror("connect");
        return 1;
    }

    // make stdin nonblocking:
if (fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK) < 0) {
    perror("fcntl stdin");
    return 1;
}



    // make the socket nonblocking:
if (fcntl(sock_fd, F_SETFL, fcntl(sock_fd, F_GETFL) | O_NONBLOCK) < 0) {
    perror("fcntl socket");
    return 1;
}




    FILE *server = fdopen(sock_fd, "r+");

    while (1) {
        // Check stdin for input
        if (fgets(buf, sizeof(buf), stdin)) {
            // Send user input to the server
            if (fprintf(server, "%s", buf) < 0 || fflush(server) == EOF) {
                perror("Error writing to the server");
                break;
            }
        }

        // Check the socket for data
        errno = 0; // Reset errno
        if (fgets(buf, sizeof(buf), server)) {
            printf("Server: %s", buf); // Display message from the server
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("Error reading from the server");
            break;
        }

        usleep(100 * 1000); // Wait 100ms before checking again
    }

    fclose(server);
    close(sock_fd);    
    return 0;

}
