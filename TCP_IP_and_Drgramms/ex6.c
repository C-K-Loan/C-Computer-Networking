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
#define MAX_MSG_LEN  512

/*
 * This function sends a dummy package (via tcp) to a server given via its domain
 * and port. The response is the printed to stdout.
 * Note, that according to qotd-protocol the quote's maximum length is 512 characters.
 */
int print_quote(char *domain_name, char *port) {
    int sock, status;
    struct addrinfo hints;
    struct addrinfo *servinfo;

    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_UNSPEC;     // let getaddrinfo decide which ai_family
    hints.ai_socktype = SOCK_DGRAM; // TCP stream sockets

    /* resolve domain name */
    status = getaddrinfo(domain_name, port, &hints, &servinfo);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 2;
    }

    sock = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (sock == -1) {
        fprintf(stderr, "socket: %s\n", strerror(errno));
        return 3;
    }

    /* send useless message, will be discarded anyway */
    char msg[] = "hallo";
    status = sendto(sock, msg, sizeof msg, 0, servinfo->ai_addr, servinfo->ai_protocol);
    if (status == -1) {
        fprintf(stderr, "send: %s\n", strerror(errno));
        return 5;
    }

    struct sockaddr_storage their_addr;
    socklen_t addr_size;

    /* receive message and write to msg_buffer */
    char msg_buffer[MAX_MSG_LEN + 1];
    memset(msg_buffer, 0, sizeof msg_buffer);
    status = recvfrom(sock, msg_buffer, MAX_MSG_LEN, 0,
            (struct sockaddr *)&their_addr, &addr_size);
    if (status == -1) {
        fprintf(stderr, "recv: %s\n", strerror(errno));
        return 6;
    }
    assert(status <= MAX_MSG_LEN);
    msg_buffer[status] = '\0'; /* end string */

    printf("%s", msg_buffer);

    status = close(sock);
    freeaddrinfo(servinfo);
    if (status == -1) {
        fprintf(stderr, "close: %s\n", strerror(errno));
        return 7;
    }

    return 0;
}

/* this programs implements a qotd-protocol client.
 * It expects a domain_name and a port as input parameters.
 */
int main(int argc, char *argv[]) {
    if (argc != EXPECTED_ARGC) {
        fprintf(stderr, "usage: %s <domain_name> <port>\n", argv[0]);
        return 1;
    }

    return print_quote(argv[1], argv[2]);
}
