#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <time.h>


#define HEADER_LEN 6

const uint8_t del_mask = 1;
const uint8_t set_mask = 1 << 1;
const uint8_t get_mask = 1 << 2;

char transaction_id = 0;

/* return timedifference in nanoseconds */
long timediff(struct timespec start, struct timespec end) {
    long sec_diff = end.tv_sec - start.tv_sec;
    long nsec_diff = end.tv_nsec - start.tv_nsec;
    /* 1000000000 ns = 1s*/
    return sec_diff/1000000000 + nsec_diff;
}

int send_msg(int sock, struct addrinfo *servinfo, uint8_t action, char *key, char *val) {
    uint16_t key_len = strlen(key);
    uint16_t val_len = strlen(val);
    int msg_len = HEADER_LEN + key_len + val_len;
    char *msg = calloc(msg_len, sizeof *msg);
    char *cur_msg = msg;

    printf("send message: action=%"PRIu8 " transaction_id=%c key=%s value=%s\n", action, transaction_id, key, val);

    cur_msg[0] |= action;
    cur_msg++;

    cur_msg[0] = transaction_id;
    cur_msg++;
    transaction_id++;

    uint16_t num;
    num = htons(key_len);
    memcpy(cur_msg, &num, sizeof num);
    cur_msg += sizeof num;

    num = htons(val_len);
    memcpy(cur_msg, &num, sizeof num);
    cur_msg += sizeof num;

    memcpy(cur_msg, key, key_len);
    cur_msg += key_len;

    memcpy(cur_msg, val, val_len);
    cur_msg += val_len;

    if (sendto(sock, msg, msg_len, 0, servinfo->ai_addr, servinfo->ai_addrlen) < 0) {
        fprintf(stderr, "sendto: %s\n", strerror(errno));
    }
    free(msg);
}

int recv_msg(int sock) {
    struct sockaddr_storage their_addr;
    socklen_t addr_size;

    char *msg = calloc(HEADER_LEN, sizeof *msg);
    recvfrom(sock, msg, HEADER_LEN, MSG_PEEK, (struct sockaddr *)&their_addr, &addr_size);

    uint16_t key_len;
    uint16_t val_len;
    memcpy(&key_len, msg+2, sizeof key_len);
    memcpy(&val_len, msg+4, sizeof val_len);
    key_len = ntohs(key_len);
    val_len = ntohs(val_len);

    int msg_len = HEADER_LEN + key_len + val_len;
    msg = realloc(msg, msg_len * sizeof(*msg));
    recvfrom(sock, msg, msg_len, 0, (struct sockaddr *)&their_addr, &addr_size);
    
    char *cur_msg = msg;
    char *recv_key = calloc(key_len + 1, sizeof *recv_key);
    char *recv_val = calloc(val_len + 1, sizeof *recv_val);

    char action = cur_msg[0];
    cur_msg++;
    char transaction_id = cur_msg[0];
    cur_msg++;
    cur_msg += sizeof key_len;
    cur_msg += sizeof val_len;
    memcpy(recv_key, cur_msg, key_len);
    cur_msg += key_len;
    memcpy(recv_val, cur_msg, val_len);
    cur_msg += val_len;

    printf("recv message: action=%"PRIu8 " transaction_id=%c key=%s value=%s\n", action, transaction_id, recv_key, recv_val);
    free(msg);
}

int main(int argc, char **argv) {
    if (argc != 6) {
        printf("usage %s <ip> <port> <action> <key> <value>", argv[0]);
        return 1;
    }

    int request_action;
    char *ip = argv[1];
    char *port = argv[2];
    char *action = argv[3];
    char *key = argv[4];
    char *value = argv[5];

    if (strcmp(action, "del") == 0) {
        request_action = del_mask;
    } else if (strcmp(action, "get") == 0) {
        request_action = get_mask;
    } else { // default is set
        request_action = set_mask;
    }

    ssize_t status;
    struct addrinfo hints;
    struct addrinfo *servinfo;  // will point to the results

    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_INET;       // IPv4
    hints.ai_socktype = SOCK_DGRAM;  // TCP stream sockets

    int sock;
    status = getaddrinfo(ip, port, &hints, &servinfo);
    // loop through all the results and make a socket
    for(struct addrinfo *p = servinfo; p != NULL; p = p->ai_next) {
        if ((sock = socket(p->ai_family, p->ai_socktype,
                        p->ai_protocol)) == -1) {
            perror("talker: socket");
            continue;
        }

        break;
    }

    send_msg(sock, servinfo, request_action, key, value);

    struct timespec start, end;

    //1. time stamp after request transmition
    clock_gettime(CLOCK_REALTIME, &start);

    recv_msg(sock);

    //2. time stamp
    clock_gettime(CLOCK_REALTIME, &end);
    printf("The DHT request needed: %ld Nanoseconds.\n", timediff(start, end));

    freeaddrinfo(servinfo);
}
