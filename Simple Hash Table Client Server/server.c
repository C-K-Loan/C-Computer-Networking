#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "hash_table.h"

/* hash_table definitions */
const uint8_t delete_mask = 1;
const uint8_t set_mask = 1 << 1;
const uint8_t get_mask = 1 << 2;
const uint8_t acknowledgment_mask = 1 << 3;
const uint8_t reserved_mask = 0xF0;

/* the length of an internal message, header, ip, port, id */
#define INTL_MSG_LEN (1 + 4 + 2 + 2)
#define EXT_HEADER_LEN 6
#define ADDR_LEN sizeof(in_addr)

/* mask used to distinguish internal from external messages */
const uint8_t internal_mask = 1 << 7;

/* masks to distinguish internal message types */
const uint8_t join_mask = 1;
const uint8_t notify_mask = 1 << 1;
const uint8_t stabilize_mask = 1 << 2;

struct server_info {
    struct sockaddr_in address;
    uint16_t id; /* network byte order */
};

/* FIXME rename */
typedef struct dht_node {
    struct server_info *self;
    struct server_info *prev;
    struct server_info *next;
} dht_node;

/* FIXME good practice to have this as global variable ? */
static dht_node node = { .self=NULL, .prev=NULL, .next=NULL };

typedef struct finger_table{
    uint16_t finger_table[16];
    size_t entries;
	
} finger_table;

bool is_key_in_range(dht_node node, char *key, size_t key_len) {
    /* chord only contains one node */
    if (node.next == NULL) {
        return true;
    }

    uint16_t start_id = ntohs(node.self->id);
    uint16_t end_id = ntohs(node.next->id);
    uint16_t hash_val = string_hash(key, key_len);

    uint16_t end_offset = end_id - start_id;
    uint16_t val_offset = hash_val - start_id;

    return val_offset < end_offset;
}

bool is_in_range(struct server_info *start, struct server_info *end, struct server_info *val) {
    uint16_t start_id = ntohs(start->id);
    uint16_t end_id = ntohs(end->id);
    uint16_t val_id = ntohs(val->id);

    uint16_t end_offset = end_id - start_id;
    uint16_t val_offset = val_id - start_id;

    return val_offset < end_offset;
}

void print_server_info(struct server_info *srv) {
    if (srv == NULL) {
        printf("None\n");
        return;
    }

    printf("id=%" PRIu16 " ip=%s port=%" PRIu16 "\n", ntohs(srv->id), inet_ntoa(srv->address.sin_addr), ntohs(srv->address.sin_port));
}

int send_msg(int sock, char *msg, size_t msg_len, struct server_info *dest) {
    if (sendto(sock, msg, msg_len, 0, (struct sockaddr *)&(dest->address), sizeof dest->address) < 0) {
        fprintf(stderr, "send_msg: %s\n", strerror(errno));
    }
}

int send_int_msg(int sock, char *msg, size_t msg_len, struct server_info *dest) {
    assert(msg_len == INTL_MSG_LEN);
    printf("sending message to: ");
    print_server_info(dest);
    print_msg(msg, msg_len);
    send_msg(sock, msg, msg_len, dest);
}

struct sockaddr_in get_address(uint32_t ip_address, uint16_t port) {
    assert(sizeof (struct in_addr) == sizeof ip_address);
    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);     // short, network byte order
    ip_address = htonl(ip_address);
    memcpy(&(addr.sin_addr.s_addr), &ip_address, sizeof ip_address);
    memset(addr.sin_zero, '\0', sizeof addr.sin_zero);

    return addr;
}

char *fill_intl_msg(char action, struct server_info *srv, size_t *len) {
    assert(sizeof (struct in_addr) == sizeof (uint32_t));
    assert(INTL_MSG_LEN == (sizeof srv->address.sin_addr + sizeof srv->id + sizeof srv->address.sin_port + 1));

    char *msg = calloc(INTL_MSG_LEN, sizeof *msg);
    char *ret_msg = msg; /* pointer to start of array */

    msg[0] |= internal_mask;
    msg[0] |= action;
    msg++;

    struct in_addr ip_address = srv->address.sin_addr;
    uint16_t port = srv->address.sin_port;
    uint16_t id = srv->id;

    memcpy(msg, &ip_address, sizeof ip_address);
    msg += sizeof ip_address;
    memcpy(msg, &port, sizeof port);
    msg += sizeof port;
    memcpy(msg, &id, sizeof id);
    msg += sizeof id;

    *len = INTL_MSG_LEN;
    return ret_msg;
}

struct server_info *parse_msg(char *msg, size_t msg_len) {
    assert(msg[0] & internal_mask);
    assert(msg_len == INTL_MSG_LEN);

    uint16_t port;
    uint16_t id;
    uint32_t ip;
    struct server_info *res = malloc(sizeof *res);
    memcpy(&ip, msg+1, sizeof ip);
    memcpy(&port, msg+5, sizeof port);
    memcpy(&id, msg+7, sizeof id);

    res->id = id;

    assert(sizeof (struct in_addr) == sizeof ip);
    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = port; /* port already in network byte order */
    memcpy(&(addr.sin_addr.s_addr), &ip, sizeof ip); /* ip already in network byte order */
    memset(addr.sin_zero, '\0', sizeof addr.sin_zero);

    res->address = addr;

    return res;
}

void print_msg(char *msg, size_t msg_len) {
    char *action;
    if (msg[0] & join_mask) {
        action = "join";
    } else if (msg[0] & notify_mask) {
        action = "notify";
    } else if (msg[0] & stabilize_mask) {
        action = "stabilize";
    } else {
        action = "unkwnon action";
    }

    struct server_info *srv = parse_msg(msg, msg_len);
    printf("action: %s, data: ", action);
    print_server_info(srv);
    free(srv);

    printf("\n");
}

ssize_t join(int sock, char *registration_ip, char *registration_port) {
    /* send own ip to registration node */
    ssize_t status;
    struct addrinfo hints;
    struct addrinfo *registration_info;  // will point to the results

    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_INET;       // IPv4
    hints.ai_socktype = SOCK_DGRAM;  // TCP stream sockets
    getaddrinfo(registration_ip, registration_port, &hints, &registration_info);

    size_t msg_len;
    char *msg;
    msg = fill_intl_msg(join_mask, node.self, &msg_len);
    status = sendto(sock, msg, msg_len, 0, registration_info->ai_addr, registration_info->ai_addrlen);

    free(msg);
    freeaddrinfo(registration_info);
    return status;
}

int notify(int sock, struct server_info *dest, struct server_info *srv) {
    /* send prev_ip */
    ssize_t status;
    size_t msg_len;
    char *msg;
    msg = fill_intl_msg(notify_mask, srv, &msg_len);

    send_int_msg(sock, msg, msg_len, dest);
    if (status < 0) {
        fprintf(stderr, "notify: %s\n", strerror(errno));
    }

    free(msg);
    return status;
}

int stabilize(int sock, struct server_info *dest) {
    /* fill in own ip, send to next */
    ssize_t status;
    size_t msg_len;
    char *msg;

    msg = fill_intl_msg(stabilize_mask, node.self, &msg_len);
    send_int_msg(sock, msg, msg_len, dest);

    free(msg);
    return status;
}

int handle_join(int sock, char *msg, size_t msg_len) {
    /* check if responsible, if yes set prev, send notify to source */
    assert(msg[0] & join_mask && msg[0] & internal_mask);
    assert(msg_len == INTL_MSG_LEN);
    struct server_info *srv = parse_msg(msg, msg_len);

    /* we are the only node in the chord */
    if (node.next == NULL) {
        notify(sock, srv, node.self);
        set_next(sock, srv);
        return 0;
    }

    if (is_in_range(node.self, node.next, srv) || node.next->id == node.self->id) {
        notify(sock, srv, node.next);
        set_next(sock, srv);
    } else {
        send_int_msg(sock, msg, msg_len, node.next);
        free(srv);
    }
}

void set_next(int sock, struct server_info *next) {
    if (node.next != NULL) {
        free(node.next);
    }
    node.next = next;
    stabilize(sock, next);
}

int handle_notify(int sock, char *msg, size_t msg_len) {
    /* parse ip as src_ip, set prev to src_ip */
    assert(msg[0] & notify_mask && msg[0] & internal_mask);
    assert(msg_len == INTL_MSG_LEN);

    struct server_info *srv = parse_msg(msg, msg_len);
    if (node.next == NULL || is_in_range(node.self, node.next, srv) && srv->id != node.self->id) {
        set_next(sock, srv);
    } else {
        free(srv);
    }
}

int handle_stabilize(int sock, char *msg, size_t msg_len) {
    /* parse ip as dest_ip, fill in prev ip, send notify to dest_ip */
    assert(msg[0] & stabilize_mask && msg[0] & internal_mask);
    assert(msg_len == INTL_MSG_LEN);
    struct server_info *src = parse_msg(msg, msg_len);

    if (node.prev == NULL || is_in_range(node.prev, node.self, src)) {
        node.prev = src;
    }

    assert(node.prev != NULL); /* if we notify NULL, something went terribly wrong */
    notify(sock, src, node.prev);
}

int handle_intl_message(int sock) {
    printf("before message:\n");
    printf("prev: ");
    print_server_info(node.prev);

    printf("next: ");
    print_server_info(node.next);

    ssize_t status;
    char *msg = calloc(INTL_MSG_LEN, sizeof *msg);
    struct sockaddr_storage their_addr;
    socklen_t addr_size;
    status = recvfrom(sock, msg, INTL_MSG_LEN, 0, (struct sockaddr *)&their_addr, &addr_size);

    assert(msg[0] & internal_mask);

    printf("received: ");
    print_msg(msg, INTL_MSG_LEN);

    if (join_mask & msg[0]) {
        handle_join(sock, msg, INTL_MSG_LEN);
    } else if (notify_mask & msg[0]) {
        handle_notify(sock, msg, INTL_MSG_LEN);
    } else if (stabilize_mask & msg[0]) {
        handle_stabilize(sock, msg, INTL_MSG_LEN);
    } else {
        assert(0); /* no action bit set */
        /* FIXME return -1; */
    }

    free(msg);
    printf("after message:\n");
    printf("prev: ");
    print_server_info(node.prev);

    printf("next: ");
    print_server_info(node.next);
}

int handle_ext_message(hash_table *tbl, int sock) {
    printf("external message:\n");
    ssize_t status;
    char *msg = calloc(EXT_HEADER_LEN, sizeof *msg);
    struct sockaddr_storage their_addr;
    socklen_t addr_size;
    status = recvfrom(sock, msg, EXT_HEADER_LEN, MSG_PEEK, (struct sockaddr *)&their_addr, &addr_size);

    if (status < 0) {
        fprintf(stderr, "recvfrom: %s\n", strerror(errno));
    } else if (status < EXT_HEADER_LEN) {
        return -1;
    }

    /* read key length and value length */
    char action = msg[0];
    uint16_t recv_key_len;
    memcpy(&recv_key_len, msg + 2, sizeof recv_key_len);
    recv_key_len = ntohs(recv_key_len);

    uint16_t recv_value_len;
    memcpy(&recv_value_len, msg + 4, sizeof recv_value_len);
    recv_value_len = ntohs(recv_value_len);
    int msg_len = EXT_HEADER_LEN + recv_key_len + recv_value_len;

    msg = realloc(msg, msg_len * sizeof(*msg));
    recvfrom(sock, msg, msg_len, 0, (struct sockaddr *)&their_addr, &addr_size);
    char *recv_key_buffer = calloc(recv_key_len+1, sizeof *recv_key_buffer);
    char *recv_value_buffer = calloc(recv_value_len+1, sizeof *recv_value_buffer);
    char *cur_msg = msg;
    cur_msg += EXT_HEADER_LEN;

    memcpy(recv_key_buffer, cur_msg, recv_key_len);
    cur_msg += recv_key_len;

    memcpy(recv_value_buffer, cur_msg, recv_value_len);
    cur_msg += recv_value_len;

    printf("action=%"PRIu8 " transaction_id=%d key_len=%"PRIu16 " value_len=%"PRIu16 " key=%s value=%s\n",
            action, msg[1], recv_key_len, recv_value_len, recv_key_buffer, recv_value_buffer);

    if (is_key_in_range(node, recv_key_buffer, recv_key_len)) {
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
        size_t response_len = EXT_HEADER_LEN + send_key_len + send_value_len;
        char *response = calloc(response_len, sizeof *response);
        char *cur_response = response;
        cur_response[0] = action;
        cur_response++;
        cur_response[0] = msg[1]; /* transaction_id */
        cur_response++;

        num = htons(send_key_len);
        memcpy(cur_response, &num, sizeof num);
        cur_response += sizeof num;

        num = htons(send_value_len);
        memcpy(cur_response, &num, sizeof num);
        cur_response += sizeof num;

        /* build response */
        if (send_key_len > 0) {
            assert(send_key_buffer != NULL);
            memcpy(cur_response, send_key_buffer, send_key_len);
            cur_response += send_key_len;
        }
        if (send_value_len > 0) {
            assert(send_value_buffer != NULL);
            memcpy(response + EXT_HEADER_LEN + send_key_len, send_value_buffer, send_value_len);
            cur_response += send_value_len;
        }

        status = sendto(sock, response, response_len, 0, (struct sockaddr*)&their_addr, addr_size);
        if (status == -1) {
            fprintf(stderr, "send: %s\n", strerror(errno));
        }
        free(response);
    } else {
        /* forward, FIXME use fingertable */
        struct sockaddr_storage peer_addr;
        socklen_t peer_addr_size;
        char transaction_id = msg[1];
        send_msg(sock, msg, msg_len, node.next);
        /* wait for answert */
        char *response = calloc(EXT_HEADER_LEN, sizeof *response);
        recvfrom(sock, response, 1, MSG_PEEK, (struct sockaddr*)&peer_addr, &peer_addr_size);
        /* the server can not handle several clients at once. we expect that
         * no message except the ones we sent are answered,
         * if someone else sent a message we panic */
        assert(!(response[0] & internal_mask));
        recvfrom(sock, response, EXT_HEADER_LEN, MSG_PEEK, (struct sockaddr*)&peer_addr, &peer_addr_size);
        assert(response[1] == transaction_id); /* someone else sent a message, we cant handle such scenarios */

        uint16_t key_len;
        memcpy(&key_len, msg + 2, sizeof key_len);
        key_len = ntohs(key_len);

        uint16_t value_len;
        memcpy(&value_len, msg + 4, sizeof value_len);
        value_len = ntohs(value_len);
        int response_len = EXT_HEADER_LEN + key_len + value_len;
        response = realloc(response, response_len * (sizeof *response));
        recvfrom(sock, response, response_len, 0, (struct sockaddr*)&peer_addr, &peer_addr_size);

        /* send to origin of request */
        sendto(sock, response, response_len, 0, (struct sockaddr*)&their_addr, addr_size);
        free(response);
    }

    free(msg);
}

int handle_message(hash_table *ht, int sock) {
    printf("--------------------------------------- recv message\n");
    /* at first only handle internal messages (easier to copy to buffer) */
    ssize_t status;
    char *msg = calloc(1, sizeof *msg);
    struct sockaddr_storage their_addr;
    socklen_t addr_size;
    status = recvfrom(sock, msg, 1, MSG_PEEK, (struct sockaddr *)&their_addr, &addr_size);

    if (internal_mask & msg[0]) {
        handle_intl_message(sock);
    } else {
        handle_ext_message(ht, sock);
        /* FIXME return -1; */
    }

    free(msg);
    printf("--------------------------------------- end message\n");
}

int main(int argc, char **argv) {
    /* maybe we change the interface to this later,
     * printf("usage: %s <ip> <port> --id=<ip> --registration-ip=<reg_ip> --registration-port=<reg_port>\n", argv[0]);
     * for now we only support a very basic interface */

    char *registration_ip = NULL;
    char *registration_port = NULL;
    char *ip = NULL;
    char *port = NULL;
    uint16_t id = 0;
    bool create_chord = false;

    if (argc == 3 || argc == 4) {
        ip = argv[1];
        port = argv[2];

        if (argc == 4) {
            sscanf(argv[3], "%" SCNu16, &id);
        }

        create_chord = true;
    } else if (argc == 5 || argc == 6) {
        ip = argv[1];
        port = argv[2];
        registration_ip = argv[3];
        registration_port = argv[4];

        if (argc == 6) {
            sscanf(argv[5], "%" SCNu16, &id);
        }
    } else {
        /* TODO print usage */
        return 1;
    }

    /* FIXME cleanup */
    assert(ip != NULL && port != NULL);
    ssize_t status;
    struct addrinfo hints;
    struct addrinfo *servinfo;  // will point to the results

    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_INET;       // IPv4
    hints.ai_socktype = SOCK_DGRAM;  // TCP stream sockets

    status = getaddrinfo(ip, port, &hints, &servinfo);
    if (status < 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(errno));
    }

    int sock = socket(servinfo->ai_family, servinfo->ai_socktype , servinfo->ai_protocol);
    if (sock < 0) {
        fprintf(stderr, "socket: %s\n", strerror(errno));
    }

    int opt = 1;
    status = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);
    if (status < 0) {
        fprintf(stderr, "setsockopt: %s\n", strerror(errno));
    }

    status = bind(sock, servinfo->ai_addr, servinfo->ai_addrlen);
    if (status < 0) {
        fprintf(stderr, "bind: %s\n", strerror(errno));
    }

    node.self = malloc(sizeof *(node.self));
    assert(servinfo->ai_addrlen == sizeof(struct sockaddr_in));
    node.self->address = *((struct sockaddr_in *)servinfo->ai_addr); /* FIXME ugly */
    node.self->id = htons(id);

    if (create_chord) {
    } else {
        assert(registration_port != NULL);
        join(sock, registration_ip, registration_port);
    }

    hash_table *ht = ht_create();
    finger_table* ft = malloc(sizeof(*ft));
    memset(ft->finger_table, 0, sizeof(ft->finger_table));
    ft->entries = 0;


    while (1) {
        /* handle message */
        handle_message(ht, sock);
    }

    ht_destroy(ht);
    free(node.prev);
    free(node.next);
    free(node.self);
    close(sock);
    freeaddrinfo(servinfo);
    return 0;
}
