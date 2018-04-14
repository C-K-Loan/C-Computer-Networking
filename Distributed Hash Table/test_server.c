#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#define TEST
#include "server.c"
#include "hash_table.c"

#define DEL 1
#define SET 2
#define GET 4
#define ACK 8

#define SERVER_PORT "2000"

#define MAX_LEN 256
#define N_MALFORMED 7
#define N_MSGS 9

char malformed_msgs[N_MALFORMED][MAX_LEN] = {
    /* completely malformed, no complete header */
    {0},
    /* no complete key */
    {GET, 1, 0, 1, 0, 2, 'a', 'b'},
    /* no key given */
    {SET, 2, 0, 0, 0, 0},
    {GET, 3, 0, 0, 0, 0},
    {DEL, 4, 0, 1, 0, 0},
    /* only value given */
    {SET, 6, 0, 0, 0, 1, 'a'},
    /* message too long */
    {GET, 7, 0, 1, 0, 1, 'a', 'b', 'c'}
};

int malformed_msg_lens[N_MALFORMED] = {
    1,
    8,
    6,
    6,
    6,
    7,
    9
};

/* ACTION, ID, KEY_MSB, KEY_LSB, VAL_MSB, VAL_LSB, KEY, VAL */
/* FIXME first entry should be the length or something */
char msgs[N_MSGS][MAX_LEN] = {
    {SET, 2, 0, 1, 0, 1, 'a', '1'},
    {GET, 3, 0, 1, 0, 0, 'a'},
    {SET, 4, 0, 1, 0, 1, 'a', '2'},
    {GET, 5, 0, 1, 0, 0, 'a'},
    {SET, 6, 0, 1, 0, 1, 'b', '3'},
    {GET, 7, 0, 1, 0, 0, 'b'},
    {DEL, 8, 0, 1, 0, 0, 'a'},
    {GET, 9, 0, 1, 0, 0, 'a'},
    {DEL, 10, 0, 1, 0, 0, 'a'}
};

char expected_msgs[N_MSGS][MAX_LEN] = {
    {ACK | SET, 2, 0, 0, 0, 0},
    {ACK | GET, 3, 0, 1, 0, 1, 'a', '1'},
    {ACK | SET, 4, 0, 0, 0, 0},
    {ACK | GET, 5, 0, 1, 0, 1, 'a', '2'},
    {ACK | SET, 6, 0, 0, 0, 0},
    {ACK | GET, 7, 0, 1, 0, 1, 'b', '3'},
    {ACK | DEL, 8, 0, 0, 0, 0},
    {ACK, 9, 0, 0, 0, 0},
    {ACK, 10, 0, 0, 0, 0}
};

int msg_lens[N_MSGS] = {
    8,
    7,
    8,
    7,
    8,
    7,
    7,
    7,
    7
};

int expected_msg_lens[N_MSGS] = {
    6,
    8,
    6,
    8,
    6,
    8,
    6,
    6,
    6
};

void test_malformed(struct addrinfo *client_info) {
    struct timeval timeout;
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;

    for (int i = 0; i < N_MALFORMED; i++) {
        char *msg = malformed_msgs[i];
        int msg_len = malformed_msg_lens[i];
        char recv_buffer[1];
        int client_sock = socket(client_info->ai_family, client_info->ai_socktype, client_info->ai_protocol);
        setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

        if (connect(client_sock, client_info->ai_addr, client_info->ai_addrlen) < 0) {
            fprintf(stderr, "connect: %s\n", strerror(errno));
        }
        if (send(client_sock, msg, msg_len, 0) == -1) {
            fprintf(stderr, "send %s\n", strerror(errno));
        }

        int recv_len = recv(client_sock, recv_buffer, sizeof recv_buffer, 0);
        assert(recv_len <= 0); /* either error or no data received */
        if (recv_len < 0) {
            assert(errno == ETIMEDOUT || errno == ECONNRESET);
        }

        close(client_sock);
    }
}

void test_response(struct addrinfo *client_info) {
    for (int i = 0; i < N_MSGS; i++) {
        char *msg = msgs[i];
        char *expected_msg = expected_msgs[i];
        int msg_len = msg_lens[i];
        int expected_msg_len = expected_msg_lens[i];
        char *recv_buffer = calloc(expected_msg_len, sizeof *recv_buffer);

        int client_sock = socket(client_info->ai_family, client_info->ai_socktype, client_info->ai_protocol);

        if (connect(client_sock, client_info->ai_addr, client_info->ai_addrlen) < 0) {
            fprintf(stderr, "connect: %s\n", strerror(errno));
        }
        if (send(client_sock, msg, msg_len, 0) == -1) {
            fprintf(stderr, "send: %s\n", strerror(errno));
        }

        int recv_len = recv(client_sock, recv_buffer, expected_msg_len, 0);
        if (recv_len == -1) {
            fprintf(stderr, "recv: %s\n", strerror(errno));
        }
        assert(recv_len == expected_msg_len);

        for (int j = 0; j < recv_len; j++) {
            assert(recv_buffer[j] == expected_msg[j]);
        }

        close(client_sock);
    }
}

void serve_responses(int server_sock, int n) {
    hash_table *tbl;
    tbl = ht_create(tbl);

    for (int i = 0; i < n; i++) {
        serve(server_sock, tbl);
    }

    ht_destroy(tbl);
}

int main(int argc, char *argv[]) {
    struct addrinfo client_hints;
    struct addrinfo *client_info;

    memset(&client_hints, 0, sizeof client_hints); // make sure the struct is empty
    client_hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
    client_hints.ai_socktype = SOCK_STREAM; // TCP stream sockets

    getaddrinfo("127.0.0.1", SERVER_PORT, &client_hints, &client_info);

    int server_sock;
    ssize_t status;
    struct addrinfo server_hints;
    struct addrinfo *server_info;
    memset(&server_hints, 0, sizeof server_hints);
    server_hints.ai_family = AF_UNSPEC;
    server_hints.ai_socktype = SOCK_STREAM;
    server_hints.ai_flags = AI_PASSIVE;

    getaddrinfo(NULL, SERVER_PORT, &server_hints, &server_info);
    server_sock = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);
    if (server_sock < 0) {
        fprintf(stderr, "socket: %s\n", strerror(errno));
    }

    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    status = setsockopt(server_sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);
    if (status < 0) {
        fprintf(stderr, "sockopt: %s\n", strerror(errno));
        goto cleanup;
    }

    int optval = 1;
    status = setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
    if (bind(server_sock, server_info->ai_addr, server_info->ai_addrlen) < 0) {
        fprintf(stderr, "bind: %s\n", strerror(errno));
        goto cleanup;
    }
    if (listen(server_sock, 20) < 0) {
        fprintf(stderr, "listen: %s\n", strerror(errno));
        goto cleanup;
    }

    if (fork() == 0) {
        test_response(client_info);
        _exit(0);
    } else {
        serve_responses(server_sock, N_MSGS);
    }

    if (fork() == 0) {
        test_malformed(client_info);
        _exit(0);
    } else {
        serve_responses(server_sock, N_MALFORMED);
    }

    printf("%s: all tests passed\n", argv[0]);
cleanup:
    close(server_sock);
    freeaddrinfo(server_info);
    freeaddrinfo(client_info);
}
