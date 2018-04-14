#define _GNU_SOURCE

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#define EXPECTED_ARGC 3
#define BUF_LEN 1

#define MAX_QUOTES 1000
#define MAX_MSG_LEN 512
/* buffer to hold quotes
 * 512 characters for quotes + terminating null-byte */
char quotes[MAX_QUOTES][MAX_MSG_LEN + 1];
int nquotes = 0;

/* Run a qotd-server listening on given port.
 * Respond to incoming packets with a random quote from quotes buffer.
 */
int run_server(char *port) {
    int status;
    struct addrinfo hints, *res;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;  // use IPv4 or IPv6, whichever
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

    status = getaddrinfo(NULL, port, &hints, &res);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 2;
    }

    int sock;
    sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock == -1) {
        fprintf(stderr, "socket: %s\n", strerror(errno));
        return 3;
    }

    status = bind(sock, res->ai_addr, res->ai_addrlen);
    if (status == -1) {
        fprintf(stderr, "bind: %s\n", strerror(errno));
        return 4;
    }

    status = listen(sock, 5);
    if (status == -1) {
        fprintf(stderr, "bind: %s\n", strerror(errno));
        return 5;
    }

    /* server loop, listen for incoming connects, don't fork */
    while (1) {
        struct sockaddr_storage their_addr;
        socklen_t addr_size;
        int conn_sock;

        addr_size = sizeof their_addr;
        conn_sock = accept(sock, (struct sockaddr *)&their_addr, &addr_size);

        if (conn_sock == -1) {
            fprintf(stderr, "socket: %s\n", strerror(errno));
            return 6;
        }

        int bytes_read;
        char read_buffer[BUF_LEN];
        memset(read_buffer, 0, sizeof read_buffer);
        char *msg;

        /* interpret no bytes read as connection closed otherwiser don't use received bytes */
        while ((bytes_read = recv(conn_sock, read_buffer, sizeof read_buffer, 0))) {
            /* if there is an error while reading, don't shut server down */
            if (bytes_read == -1) {
                fprintf(stderr, "recv: %s\n", strerror(errno));
                break;
            }

            /* generate random integer in the range of available quotes
             * choose message accordingly */
            assert(nquotes <= MAX_QUOTES);
            int r = rand() % nquotes;
            msg = quotes[r];

            int len = strlen(msg);
            assert(len <= MAX_MSG_LEN);

            status = send(conn_sock, msg, len + 1, 0);
            /* dont crash on error while sending */
            if (status == -1) {
                fprintf(stderr, "send: %s\n", strerror(errno));
                break;
            }

            memset(&read_buffer, 0, sizeof read_buffer);
        }
        close(conn_sock);

    }

    close(sock);
    freeaddrinfo(res);
    return 0;
}

/* Read up to MAX_QUOTES quotes into the global quotes buffer from the file
 * given by filename * Quotes are expected to be seperated by newlines.
 * Return nonzero if there an error occured.
 */
int read_file(char *filename) {
    for (int i = 0; i < MAX_QUOTES; i++) {
        memset(quotes[i], 0, sizeof quotes[i]);
    }

    FILE *file;

    file = fopen(filename, "r");
    if (NULL == file) {
        fprintf(stderr, "fopen: %s\n", strerror(errno));
    }

    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    /* read file line by line and write line to quotes buffer */
    while ((read = getline(&line, &len, file)) != -1 && nquotes < MAX_QUOTES) {
        assert(read <= MAX_MSG_LEN);
        strncpy(quotes[nquotes], line, read);
        quotes[nquotes][read] = '\0'; /* always terminate string at end of buffer */
        nquotes++;
    }

    /* if no quotes were read return error */
    if (nquotes == 0) {
        fprintf(stderr, "read_file: no quotes in file %s\n", "filename");
        return 1;
    }

    free(line);
    fclose(file);

    return 0;
}

/* This program starts a qotd-protocol server.
 * It expects a port and an input file as arguments.
 * The server will listen on the given port and print quotes from the file
 */
int main(int argc, char *argv[]) {
    if (argc != EXPECTED_ARGC) {
        fprintf(stderr, "usage: %s <port> <file_name>\n", argv[0]);
        return 1;
    }

    /* read file into global quotes buffer */
    if (read_file(argv[2]) != 0) {
        return 1;
    }

    return run_server(argv[1]);
}
