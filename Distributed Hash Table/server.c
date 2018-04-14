#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "hash_table.h"
#define BUF_LEN 1 << 16
#define HEADER_LEN 6

const uint8_t delete_mask = 1;
const uint8_t set_mask = 1 << 1;
const uint8_t get_mask = 1 << 2;
const uint8_t acknowledgment_mask = 1 << 3;
const uint8_t reserved_mask = 0xF0;

void serve(int sock, hash_table *tbl) {
    struct sockaddr_storage their_addr;
    socklen_t addr_size;
    int conn_sock;
    ssize_t status;

    addr_size = sizeof their_addr;
    conn_sock = accept(sock, (struct sockaddr *)&their_addr, &addr_size);
    if (conn_sock == -1) {
        fprintf(stderr, "accept: %s\n", strerror(errno));
        return ;
    }

    char recv_key_buffer[BUF_LEN];
    char recv_value_buffer[BUF_LEN];
    char header_buffer[HEADER_LEN];

    status = recv(conn_sock, header_buffer, sizeof header_buffer, MSG_WAITALL);
    if (status == -1) {
        fprintf(stderr, "recv: %s\n", strerror(errno));
        goto close_conn_sock;
    }

    /* discard malformed packages */
    if (status != HEADER_LEN) {
        goto close_conn_sock;
    }

    /* recv request */
    char action = header_buffer[0];
    uint16_t recv_key_len;
    memcpy(&recv_key_len, header_buffer + 2, sizeof recv_key_len);
    recv_key_len = ntohs(recv_key_len);

    uint16_t recv_value_len;
    memcpy(&recv_value_len, header_buffer + 4, sizeof recv_value_len);
    recv_value_len = ntohs(recv_value_len);

    if (recv_key_len > 0) {
        status = recv(conn_sock, recv_key_buffer, recv_key_len, MSG_WAITALL);
        if (status == -1) {
            fprintf(stderr, "recv: %s\n", strerror(errno));
            goto close_conn_sock;
        }
        assert(status < BUF_LEN);
        recv_key_buffer[status] = '\0';

        if (recv_value_len > 0) {
            status = recv(conn_sock, recv_value_buffer, recv_value_len, MSG_WAITALL);
            if (status == -1) {
                fprintf(stderr, "recv: %s\n", strerror(errno));
                goto close_conn_sock;
            }
            assert(status < BUF_LEN);
            recv_value_buffer[status] = '\0';
        }
    }

    /* process request */
    if (action & delete_mask) {
        status = ht_delete_key(tbl, recv_key_buffer, recv_key_len);
        if (status == -1) {
            action ^= delete_mask;
        }
    }

    if (action & set_mask) {
        status = ht_set_value(tbl, recv_key_buffer, recv_key_len, recv_value_buffer, recv_value_len);
        if (status == -1) {
            action ^= set_mask;
        }
    }

    char *send_value_buffer = NULL;
    char *send_key_buffer = NULL;
    size_t send_value_len = 0;
    size_t send_key_len = 0;
    if (action & get_mask) {
        status = ht_get_value(tbl, recv_key_buffer, recv_key_len, (void **) &send_value_buffer, &send_value_len);
        printf("GET DETECTED\n");
        if (status == -1) {
            action ^= get_mask;
        } else {
            send_key_buffer = recv_key_buffer;
            send_key_len = recv_key_len;
        }
    }

    action ^= acknowledgment_mask;

    /* build response header */
    uint16_t num;
    header_buffer[0] = action;
    num = htons(send_key_len);
    memcpy(header_buffer + 2, &num, sizeof num);

    num = htons(send_value_len);
    memcpy(header_buffer + 4, &num, sizeof num);

    /* build response */
    size_t response_len = HEADER_LEN + send_key_len + send_value_len;
    char *response = calloc(response_len, sizeof *response);
    memcpy(response, header_buffer, HEADER_LEN);
    if (send_key_len > 0) {
        assert(send_key_buffer != NULL);
        memcpy(response + HEADER_LEN, send_key_buffer, send_key_len);
    }
    if (send_value_len > 0) {
        assert(send_value_buffer != NULL);
        memcpy(response + HEADER_LEN + send_key_len, send_value_buffer, send_value_len);
    }

    status = send(conn_sock, response, response_len, 0);
    if (status == -1) {
        fprintf(stderr, "send: %s\n", strerror(errno));
    }
    free(response);

close_conn_sock:
    close(conn_sock);
}

int run_server(char *port) {
    hash_table *tbl = ht_create();

    int status;
    struct addrinfo hints;
    struct addrinfo *servinfo;  // will point to the results

    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

    if ((status = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return 1;
    }

    int sock = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (sock == -1) {
        status = 1;
        fprintf(stderr, "socket: %s\n", strerror(errno));
        goto cleanup;
    }

    status = bind(sock, servinfo->ai_addr, servinfo->ai_addrlen);
    if (status == -1) {
        status = 1;
        fprintf(stderr, "bind: %s\n", strerror(errno));
        goto cleanup;
    }

    status = listen(sock, 20);
    if (status == -1) {
        status = 1;
        fprintf(stderr, "bind: %s\n", strerror(errno));
        goto cleanup;
    }

    /* FIXME keyboard interrupt */
    while (1) {
        serve(sock, tbl);
    }

cleanup:
    close(sock);
    freeaddrinfo(servinfo);
    ht_destroy(tbl);
    return status;
}

#ifndef TEST
int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("usage: %s <port>", argv[0]);
    }

    return run_server(argv[1]);
}
#endif
