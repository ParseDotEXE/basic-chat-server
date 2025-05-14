#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_CLIENTS 4
FILE *CLIENTS[MAX_CLIENTS] = {0};
// ^ Each connected client is represented as a FILE*
// If that client is not connected, its FILE* will be NULL

/** Sends the message `buf` to all connected clients except
 * the one at `sender_index`.
 * - Use `fprintf()` to send the message to a client
 * - Use `fflush()` after printing to the client to force sending out the message.
 * - If fprintf or fflush fail, close the client and reset that slot's FILE*
 *   to NULL.
 * */
void redistribute_message(int sender_index, char *buf) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (i == sender_index || CLIENTS[i] == NULL) {
            continue; // Skip the sender and empty slots
        }

        if (fprintf(CLIENTS[i], "%s", buf) < 0 || fflush(CLIENTS[i]) == EOF) {
            perror("Error writing to client");
            fclose(CLIENTS[i]); // Close the connection on error
            CLIENTS[i] = NULL; // Clear the slot
            printf("Client %d disconnected during message redistribution.\n", i);
        }
    }
}


int poll_message(char *buf, size_t len, int client_index) {
    if (CLIENTS[client_index] == NULL) return 0;

    errno = 0; // Reset errno before calling fgets
    if (fgets(buf, len, CLIENTS[client_index]) == NULL) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0; // No data available, nonblocking behavior
        }

        if (feof(CLIENTS[client_index])) {
            printf("Client %d reached EOF (client closed connection).\n", client_index);
        } else {
            perror("Error reading from client");
        }

        fclose(CLIENTS[client_index]); // Close the connection on EOF or error
        CLIENTS[client_index] = NULL; // Clear the slot
        printf("Client %d disconnected.\n", client_index);
        return 0;
    }

    return 1; // A message was successfully read
}




void try_add_client(int server_fd) {
    struct sockaddr_in client_address;
    socklen_t client_len = sizeof(client_address);
    int client_fd = accept(server_fd, (struct sockaddr *)&client_address, &client_len);

    if (client_fd < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return; // No pending connections
        }
        perror("accept");
        exit(1); // Critical error
    }

    // Set client socket to nonblocking
    if (fcntl(client_fd, F_SETFL, O_NONBLOCK) < 0) {
        perror("fcntl client_fd NONBLOCK");
        close(client_fd);
        return;
    }

    FILE *client_file = fdopen(client_fd, "r+");
    if (!client_file) {
        perror("fdopen");
        close(client_fd);
        return;
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (CLIENTS[i] == NULL) {
            CLIENTS[i] = client_file;
            printf("New client connected at index %d.\n", i);
            return;
        }
    }

    // Room is full
    fprintf(client_file, "Room is full. Try again later.\n");
    fflush(client_file);
    fclose(client_file);
    printf("Rejected a new client - room is full.\n");
}



int main_loop(int server_fd)
{
    char buf[1024];

    while (1) {
        // check each client to see if there are messages to redistribute
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (NULL == CLIENTS[i]) continue;
            if (!poll_message(buf, sizeof(buf), i)) continue;
            printf("Received message from client %i: %s", i, buf);
            redistribute_message(i, buf);
        }

        // see if there's a new client to add
        try_add_client(server_fd);

        usleep(100 * 1000); // wait 100ms before checking again
    }
}

int main(int argc, char* argv[])
{
    struct sockaddr_in address;
    int server_fd;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1 aka localhost
    // ^ this is a kind of security: the server will only listen
    // for incoming connections that come from 127.0.0.1 i.e. the same
    // computer that the server process is running on.
    address.sin_port = htons(7878);

    if (-1 == bind(server_fd, (struct sockaddr *)&address, sizeof(address))) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (-1 == listen(server_fd, 5)) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    // set the server file descriptor to nonblocking mode
    // so that `accept` returns immediately if there are no pending
    // incoming connections
    if (-1 == fcntl(server_fd, F_SETFL, O_NONBLOCK)) {
        perror("fcntl server_fd NONBLOCK");
        close(server_fd);
        return 1;
    }

    int status = main_loop(server_fd);
    close(server_fd);
    return status;
}
