#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <inttypes.h>

/* be64toh, network byte order is big endian */
#include <endian.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <time.h>

#define TIMEOUT 2
#define MAX_STRATUM 0xFF
#define PACK_LEN  12 //package_len in 32bit integers
#define NTP_VERSION 4
#define NTP_MODE 3
#define NTP_HEADER (NTP_MODE << 24) | (NTP_VERSION << 27)
#define NTP_OFFSET 2208988800
#define FRACTIONS_MASK (0xFFFFFFFF)
#define N_SERVERS 5

enum log_level {
    ERROR,
    WARN,
    DEBUG
};

#define LOG_LEVEL DEBUG

const char *ntp_servers[N_SERVERS] = {
    "0.de.pool.ntp.org",
    "1.de.pool.ntp.org",
    "2.de.pool.ntp.org",
    "3.de.pool.ntp.org",
    "ntps1-0.cs.tu-berlin.de"
};

typedef int64_t ntp_time;
typedef int32_t ntp_short;
typedef struct ntp_result {
    const char *server_name;
    int stratum;
    ntp_short root_dispersion;
    ntp_time offset;
} ntp_result;

/* t_1: client send
 * t_2: server receive
 * t_3: server send
 * t_4: client receive
 * returns time offset \theta = ((t_2 - t_1) + (t_3 - t_4)) / 2, see lecture
 */
ntp_time time_offset(const ntp_time t_1, const ntp_time t_2, const ntp_time t_3, const ntp_time t_4) {
    return ((t_2 - t_1) + (t_3 - t_4)) / 2;
}

ntp_time time_delay(const ntp_time t_1, const ntp_time t_2, const ntp_time t_3, const ntp_time t_4) {
    return (t_4 - t_1) + (t_3 - t_2);
}

/* converts a unix timestamp to an ntp 64bit fixed point number */
ntp_time to_ntp(const time_t unix_timestamp) {
    uint64_t tmp = (uint64_t) unix_timestamp;
    return (ntp_time) ((tmp + NTP_OFFSET) << 32);
}

/* converts a ntp 64bit fixed point number to a unix timestamp */
time_t to_unix(const ntp_time timestamp) {
    uint64_t tmp = (uint64_t) timestamp;
    return (time_t) ((tmp >> 32) - NTP_OFFSET);
}

/* returns the seconds for a ntp 64bit */
int32_t seconds(const ntp_time t) {
    uint64_t tmp = (uint64_t) t;
    return (int32_t) (tmp >> 32);
}

/* returns the ntp 64bit as double */
double fractional(const ntp_time t) {
    uint64_t scaling_factor = (uint64_t) 1 << 32;
    return ((double) t) / (double) scaling_factor;
}

/* sock: socket on which package is sent
 * servinfo: destination of package
 * client_transmit: timestamp when sending
 * return != 0 if sending fails
 */
int send_ntp_request(const int sock, const struct addrinfo *servinfo, ntp_time *client_transmit) {
    // create buffer & send
    /* send request*/
    ssize_t status;
    uint32_t package [PACK_LEN];
    memset(package,0, sizeof(package));
    package[0] = htonl(NTP_HEADER);

    // set client_transmit_timestamp
    *client_transmit = to_ntp(time(NULL));

    status = sendto(sock, package, sizeof package, 0, servinfo->ai_addr, (socklen_t) servinfo->ai_protocol);
    if (status == -1) {
        fprintf(stderr, "send: %s\n", strerror(errno));
        return 5;
    }

    return 0;
}

void  print_result(const ntp_result res) {
    time_t cur_time = time(NULL);
    printf("ntp-server: %s\n", res.server_name);
    printf("current time: %s", ctime(&cur_time));
    printf("time offset: %lf\n", fractional(res.offset));
    cur_time += seconds(res.offset);
    printf("corrected time: %s", ctime(&cur_time));
}

/* sock: socket on which we receive package
 * msg: result of response
 * return != 0 if sending fails
 */
int recv_ntp_response(int sock,
                      ntp_time *client_receive,
                      int *stratum,
                      ntp_short *dispersion,
                      ntp_time *server_receive,
                      ntp_time *server_transmit) {
    // recv response & parse to ntp_msg struct
    /* receive message and write to msg_buffer */
    ssize_t status;
    struct sockaddr_storage their_addr;
    socklen_t addr_size = sizeof(their_addr);

    uint32_t msg_buffer[PACK_LEN];
    memset(msg_buffer, 0, sizeof msg_buffer);
    status = recvfrom(sock, msg_buffer, sizeof msg_buffer, 0, (struct sockaddr *)&their_addr, &addr_size);
    if (status == -1) {
        fprintf(stderr, "recv: %s\n", strerror(errno));
        return 6;
    }
    assert(status == sizeof msg_buffer);

    *client_receive = to_ntp(time(NULL));

    uint32_t *cur_buffer = msg_buffer;

    *stratum = (int) ntohl(*cur_buffer) >> 16 & 0xFF;
    cur_buffer += 1; /* header */
    cur_buffer += 1; /* root delay */

    memcpy(dispersion, cur_buffer, sizeof(*dispersion));
    *dispersion = (ntp_short) be32toh((unsigned int)*dispersion);
    cur_buffer += 1; /* root dispersion */

    cur_buffer += 1; /* reference id */
    cur_buffer += 2; /* reference timestamp */
    cur_buffer += 2; /* origin timestamp */

    memcpy(server_receive, cur_buffer, sizeof(*server_receive));
    *server_receive = (ntp_time) be64toh((uint64_t) *server_receive);
    cur_buffer += 2; /* receive timestamp */

    memcpy(server_transmit, cur_buffer, sizeof(*server_transmit));
    *server_transmit = (ntp_time) be64toh((uint64_t) *server_transmit);
    cur_buffer += 2; /* transmit timestamp */

    assert(*server_transmit >= *server_receive);

    return 0;
}

int get_time(const char *domain_name, const char *port, ntp_result *res) {
    assert(res != NULL);

    res->server_name = domain_name;

    int sock, status;
    struct addrinfo hints;
    struct addrinfo *servinfo;

    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_UNSPEC;     // let getaddrinfo decide which ai_family
    hints.ai_socktype = SOCK_DGRAM; // DATAGRAM sockets

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

    struct timeval timeout;
    timeout.tv_sec = TIMEOUT;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

    ntp_time client_transmit;
    status = send_ntp_request(sock, servinfo, &client_transmit);
    if (status) {
        return 1;
    }

    ntp_time client_receive, server_receive, server_transmit;
    status = recv_ntp_response(sock,
                               &client_receive,
                               &(res->stratum),
                               &(res->root_dispersion),
                               &server_receive,
                               &server_transmit);
    if (status) {
        return 1;
    }

    ntp_time offset = time_offset(client_transmit, server_receive, server_transmit, client_receive);
    res->offset = offset;

    status = close(sock);
    freeaddrinfo(servinfo);
    if (status == -1) {
        fprintf(stderr, "close: %s\n", strerror(errno));
        return 7;
    }

    return 0;
}

/* queries all the ntp-servers defined in ntp_servers
 * prints offset for server with smallest root delay
 */
int main() {
    /* a little bit hacky, but should work */
    ntp_result best_result;
    best_result.stratum = MAX_STRATUM + 1; /* impossible stratum */

    for (int i = 0; i < N_SERVERS; i++) {
        ntp_result res;
        int status = get_time(ntp_servers[i], "123", &res);
        if (status) {
            fprintf(stderr, "INFO: server \"%s\" did not respond\n", ntp_servers[i]);
            continue;
        }

        if (res.stratum < best_result.stratum || res.root_dispersion < best_result.root_dispersion) {
            best_result = res;
        }
    }

    /* if the stratum that is set has not changed no server responded
     * the maximum possible (real) stratum is 0xFF */
    if (best_result.stratum > MAX_STRATUM) {
        fprintf(stderr, "ERROR: no NTP-server responded\n");
        return 1;
    }

    print_result(best_result);
    return 0;
}

