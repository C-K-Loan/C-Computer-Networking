#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

/* number of retries for at least once semantics */
#define RETRIES 5
/* 2 seconds timeout for at least once semantics */
#define TIMEOUT 2
/* maximum length of a key or value */
#define MAX_LEN 1 << 16
/* maximum number of films allowed */
#define MAX_FILMS 1000
/* number of attributes of a film */
#define N_FIELDS 5
/* number of films that we send to the server */
#define N_FILMS 25
/* the length of a package header */
#define HEADER_LEN 6
/* maximum length of a package */
#define MAX_REQUEST_LEN (HEADER_LEN + 2*MAX_LEN)

/* bitmasks fro action */
uint8_t DEL = 1;
uint8_t SET = 1 << 1;
uint8_t GET = 1 << 2;

/* 2 seconds timeout */
uint8_t transaction_id = 0;

typedef struct {
    char *title;
    char *fields[N_FIELDS];
} film;

film *films[MAX_FILMS];
int n_films = 0;

char *field_names[N_FIELDS] = {
    "Originaltitel",
    "Herstellungsjahr",
    "Länge",
    "Regie",
    "DarstellerInnen"
};

/* A wrapper for the hash table that resides on a remote server
 * The hash_table provides an rpc interface for get, set, delete actions.
 * each operation will be performed on a new connection. It's not
 * clear if the server needs to support connection reuse.
 */
typedef struct {
    struct addrinfo *servinfo;
} hash_table;

/* allocate a hash_table, domain_name and port define the remote server */
hash_table *ht_create(char *domain_name, char *port) {
    int status;
    struct addrinfo hints;
    struct addrinfo *servinfo;

    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_UNSPEC;     // let getaddrinfo decide which ai_family
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets

    /* resolve domain name */
    status = getaddrinfo(domain_name, port, &hints, &servinfo);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return NULL;
    }

    hash_table *ht = malloc(sizeof *ht);
    ht->servinfo = servinfo;
    return ht;
}

/* free hash tables that were allocated by ht_create */
void ht_destroy(hash_table *ht) {
    freeaddrinfo(ht->servinfo);
    free(ht);
}

/* marshal the request parameters, allocates and returns the request buffer,
 * don't forget to free. The length of the buffer is saved in len.
 * */
char *build_request(uint8_t action, char *key, char **val, int *len) {
    char *request = calloc(MAX_REQUEST_LEN, sizeof *request);
    char *cur = request;
    assert(cur - request == 0);

    *cur = action;
    cur++;
    *cur = transaction_id;
    cur++;
    uint16_t key_len = strlen(key);
    uint16_t num;
    num = htons(key_len);
    memcpy(cur, &num, sizeof num);
    cur += sizeof num;
    char *value_len_ptr = cur;
    cur += 2;

    memcpy(cur, key, key_len);
    cur += key_len;

    /* marshal value */
    if (val == NULL) {
        num = 0;
        memcpy(value_len_ptr, &num, sizeof num);
    } else {
        uint16_t value_len = 0;
        for (int i = 0; i < N_FIELDS; i++) {
            int field_len = strlen(val[i]);
            num = htons(field_len);
            memcpy(cur + value_len, &num, sizeof num);
            value_len += sizeof num;
            memcpy(cur + value_len, val[i], field_len);
            value_len += field_len;
        }
        num = htons(value_len);
        memcpy(value_len_ptr, &num, sizeof num);
        cur += value_len;
    }

    *len = cur - request;
    return request;
}

/* send a request to the remote hash table.
 * retry until correct response is sent. Timeout is 2 seconds.
 */
int perform_action(hash_table *ht, uint8_t action, char *key, char **val, film **f) {
    *f = NULL;
    if (action & SET) {
        assert(val != NULL);
    }

    int request_len;
    char *request = build_request(action, key, val, &request_len);

    struct timeval timeout;
    timeout.tv_sec = TIMEOUT;
    timeout.tv_usec = 0;

    for (int i = 0; i < RETRIES; i++) {
        uint16_t num;
        int status;
        int sock = socket(ht->servinfo->ai_family, ht->servinfo->ai_socktype, ht->servinfo->ai_protocol);
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof timeout);
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

        if (connect(sock, ht->servinfo->ai_addr, ht->servinfo->ai_addrlen) < 0) {
            goto retry;
        }

        if (send(sock, request, request_len, 0) < 0) {
            goto retry;
        }

        char msg_buffer[HEADER_LEN];
        if ((status = recv(sock, msg_buffer, sizeof msg_buffer, MSG_WAITALL)) < 0) {
            goto retry;
        }

        /* if the acknowledment of the server does not contain the
         * action we want to perform, we retry the request */
        char action_performed = msg_buffer[0] & action;
        if (!action_performed) {
            goto retry;
        }

        memcpy(&num, msg_buffer + 2, sizeof num);
        int key_len = ntohs(num);
        memcpy(&num, msg_buffer + 4, sizeof num);
        int val_len = ntohs(num);

        char *key_buffer = calloc(key_len, sizeof *key_buffer);
        char *val_buffer = calloc(val_len, sizeof *val_buffer);
        if ((status = recv(sock, key_buffer, key_len, MSG_WAITALL)) < 0) {
            free(key_buffer);
            free(val_buffer);
            goto retry;
        }
        if ((status = recv(sock, val_buffer, val_len, MSG_WAITALL)) < 0) {
            free(key_buffer);
            free(val_buffer);
            goto retry;
        }

        if (action & GET && val_len > 0) {
            /* unmarshal value */
            film *response_film = malloc(sizeof *response_film);
            response_film->title = malloc(key_len + 1);
            memcpy(response_film->title, key_buffer, key_len);
            response_film->title[key_len] = '\0';

            char *cur_val = val_buffer;
            for (int i = 0; i < N_FIELDS; i++) {
                uint16_t field_len;
                memcpy(&field_len, cur_val, sizeof field_len);
                field_len = ntohs(field_len);
                cur_val += sizeof field_len;
                response_film->fields[i] = malloc(field_len + 1);
                memcpy(response_film->fields[i], cur_val, field_len);
                response_film->fields[i][field_len] = '\0';
                cur_val += field_len;
            }
            *f = response_film;
        }

        /* success */
        free(key_buffer);
        free(val_buffer);
        close(sock);
        free(request);
        return 0;

        assert(0); /* can never happen */
retry:
        close(sock);
        continue;
    }

    free(request);
    transaction_id++;
    return -1;
}

/* rpc-interface for get on the hash table
 * on success a new film will be allocated, otherwise *f is set to NULL*/
int ht_get(hash_table *ht, char *key, film **f) {
    int status;

    status = perform_action(ht, GET, key, NULL, f);
    if (status < 0) {
        assert((*f) == NULL);
    }

    return status;
}

/* rpc-interface for a set on the hash table
 * if result < 0 error*/
int ht_set(hash_table *ht, char *key, char **val) {
    int status;
    film *f = NULL; /* ignore */
    status = perform_action(ht, SET, key, val, &f);
    assert(f == NULL); /* a set should never return a film */
    return status;
}

/* rpc-intreafce for a delete on the hash table
 * if result < 0 error */
int ht_delete(hash_table *ht, char *key) {
    int status;
    film *f = NULL; /* ignore */
    status = perform_action(ht, DEL, key, NULL, &f);
    assert(f == NULL); /* a delete should never return a film */
    return status;
}

/* free a film struct */
void film_destroy(film *f) {
    free(f->title);
    for (int i = 0; i < N_FIELDS; i++) {
        free(f->fields[i]);
    }
    free(f);
}

/* read csv file and create array of films. Films are saved in global
 * "films" array. The number of films is in the global variable n_films
 */
ssize_t read_file(char *filename) {
    for (int i = 0; i < MAX_FILMS; i++) {
        films[i] = NULL;
    }

    FILE *file;

    file = fopen(filename, "r");
    if (NULL == file) {
        fprintf(stderr, "fopen: %s\n", strerror(errno));
    }

    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    /* read file line by line and write line to films buffer */
    while ((read = getline(&line, &len, file)) != -1 && n_films < MAX_FILMS) {
        film *f = malloc(sizeof *f);
        char *title = strtok(line, ";"); /* title is null-terminated */
        assert(title != NULL);
        int title_len = strlen(title);

        f->title = calloc(title_len + 1, sizeof f->title);
        memcpy(f->title, title, title_len);
        f->title[title_len] = '\0';

        for (int i = 0; i < N_FIELDS; i++) {
            char *field = strtok(NULL, ";");
            assert(field != NULL);
            int field_len = strlen(field);

            f->fields[i] = calloc(field_len + 1, sizeof (f->fields[i]));
            memcpy(f->fields[i], field, field_len);
            f->fields[i][field_len] = '\0';
        }

        films[n_films] = f;
        n_films++;
    }

    /* if no films were read return error */
    if (n_films == 0) {
        fprintf(stderr, "read_file: no quotes in file %s\n", "filename");
        return 1;
    }

    free(line);
    fclose(file);

    return 0;
}

void print_film(film *f) {
    printf("Titel %s\n", f->title);
    for (int i = 0; i < N_FIELDS; i++) {
        printf("%s: %s\n", field_names[i], f->fields[i]);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("usage: %s <ip> <port> <filename>\n", argv[0]);
        return 1;
    }

    read_file(argv[3]);
    if (n_films < 0) {
        return 1;
    }
    assert(n_films <= MAX_FILMS);

    hash_table *ht = ht_create(argv[1], argv[2]);
    for (int i = 0; i < N_FILMS; i++) {
        int status;
        film *f = films[i % n_films];
        printf("--------------------------------------\n");
        printf("perform get,set,delete,get for film %s\n", f->title);

        status = ht_set(ht, f->title, f->fields);
        if (status < 0) {
            printf("could not perform set operation for film %s\n", f->title);
        } else {
            printf("set operation successfull for film %s\n", f->title);
        }

        film *f_remote;
        status = ht_get(ht, f->title, &f_remote);
        if (status < 0) {
            printf("could not perform get operation for film %s\n", f->title);
        } else if (f_remote == NULL) {
            printf("get operation could not find film %s\n", f->title);
        } else {
            printf("get operation successfull for film %s\n", f->title);
            print_film(f_remote);
            film_destroy(f_remote);
            f_remote = NULL;
        }

        status = ht_delete(ht, f->title);
        if (status < 0) {
            printf("could not perform delete operation for film %s\n", f->title);
        } else {
            printf("delete operation successfull for film %s\n", f->title);
        }

        status = ht_get(ht, f->title, &f_remote);
        if (status < 0) {
            printf("could not perform get operation for film %s\n", f->title);
        } else if (f_remote == NULL) {
            printf("get operation could not find film %s\n", f->title);
        } else {
            printf("unexpectedly get operation successfull for film %s\n", f->title);
            print_film(f_remote);
            film_destroy(f_remote);
        }
    }

    ht_destroy(ht);

    for (int i = 0; i < n_films; i++) {
        film_destroy(films[i]);
    }
}
