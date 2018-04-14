#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

int sock;
struct addrinfo *servinfo;
uint8_t transaction_id = 0;

const uint8_t delete_mask = 1;
const uint8_t set_mask = 1 << 1;
const uint8_t get_mask = 1 << 2;
const uint8_t acknowledgment_mask = 1 << 3;
const uint8_t reserved_mask = 0xF0;


#define MAX_MSG_LEN 4096
#define EXPECTED_ARGC 4
#define HEADER_LEN 6

int start_sock(char *domain_name, char *port) {
    int status;
    struct addrinfo hints;

    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_UNSPEC;     // let getaddrinfo decide which ai_family
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets

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

    return 0;
}

int close_sock(){
    int status = close(sock);
    freeaddrinfo(servinfo);
    if (status == -1) {
        fprintf(stderr, "close: %s\n", strerror(errno));
        return 1;
    } else {
        return 0;
    }
}

void print_commands(char* header, char* key, char* value){
    printf("Commands:\n\n");
    printf("set:%d\n",header[0] == set_mask);
    printf("get:%d\n",header[0] == get_mask);
    printf("del:%d\n",header[0] == delete_mask);
    printf("ack:%d\n",header[0] == acknowledgment_mask);


    int print_key = 40;
    if (strlen(key) < 40){
	print_key = strlen(key);
    }

    int print_value = 40;
    if (strlen(value) < 40){
	print_value = strlen(value);
    }
    printf("key:%.*s\n",print_key, key);
    printf("keyLen:%zd\n", strlen(key));

    printf("value:%.*s\n",print_value, value);

    printf("valueLen:%zd\n", strlen(value));
    printf("transaction id:%d\n\n", header[1]);
}

//packs and sends a message
int marshall(char* header, char* key, char* value){

   
    uint16_t key_len = strlen(key);
    uint16_t reversed_key_len = htons(key_len);

    //count number of seperate entries in value
    uint16_t entries = 0;
    for(uint16_t i=0; i<strlen(value);i++){
	if(value[i] == 59){
    	    entries++;
	}
    }

    //size of value: length of value +2Byte for each field, -1Byte for each semicolon

    uint16_t value_len = strlen(value) + (2*(entries+1)) - entries;
    uint16_t reversed_value_len = htons(value_len);
    char value_marshalled [value_len];   
    memset(&value_marshalled, 0, sizeof value_marshalled);

    char* token;
    uint16_t token_len;
    uint16_t reversed_token_len;
    uint16_t copied =0;
	
    token = strtok(value, ";");
    while(token != NULL){
	token_len = strlen(token);
	reversed_token_len = htons(token_len);
	memcpy(value_marshalled+copied, &reversed_token_len, sizeof reversed_token_len);
	copied += 2;
	memcpy(value_marshalled+copied, token, token_len);
	copied += token_len;

	token = strtok(NULL, ";");
    }

    memcpy(header+2, &reversed_key_len, sizeof reversed_key_len);
    memcpy(header+4, &reversed_value_len, sizeof reversed_value_len);

    //build the message
    char message[HEADER_LEN+key_len+value_len];
    memcpy(message, header, HEADER_LEN);
    memcpy(message+HEADER_LEN, key, key_len);
    memcpy(message+HEADER_LEN+key_len, value_marshalled, value_len);

    uint16_t msg_len = HEADER_LEN + key_len + value_len;

    //send the message and print details of the command on the console
    int status = connect(sock, servinfo->ai_addr, servinfo->ai_addrlen);
    status = send(sock, message, sizeof message, 0);
    
    char msg_buffer [MAX_MSG_LEN];
    status = recv(sock, msg_buffer, MAX_MSG_LEN, 0);
    printf("%s",msg_buffer);  

    return 0;
}

int set(char* key, char* value){

    //build the header - fill with action and transaction id
    uint8_t id = transaction_id++;
    char header[HEADER_LEN];
    memset(&header, 0, sizeof header);
    header[0] = header[0] | set_mask;
    header[1] = header[1] | id;
    print_commands(header, key, value);
    marshall(header, key, value);
    return 0;
}

int read_file(char *filename) {

    FILE *file;

    file = fopen(filename, "r");
    if (NULL == file) {
        fprintf(stderr, "fopen: %s\n", strerror(errno));
    }

    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    /* read file line by line and write line to line buffer */
    while ((read = getline(&line, &len, file)) != -1) {
        assert(read <= MAX_MSG_LEN);
	char* key = strtok(line, ";");	
	char* value = strtok(NULL, "");

	
	set(key, value);
	/*get(key);
	delete(key);
	get(key);
	*/
    }


    free(line);
    fclose(file);

    return 0;
}


int main(int argc, char *argv[]){
    if (argc != EXPECTED_ARGC) {
        fprintf(stderr, "usage: %s <domain_name> <port> <input_file>\n", argv[0]);
        return 1;
    }

    start_sock(argv[1], argv[2]);

    if (read_file(argv[3]) != 0) {
        return 1;
    }



    close_sock();

}
