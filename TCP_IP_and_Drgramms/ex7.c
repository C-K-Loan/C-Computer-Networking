#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#define MAX_MSG_LEN  512000
#define PEEK_LEN 128
#define HTTP_PORT "80"

int get_chunk_len(int sock, char* buffer){
    char length [10];
    memset(length, 0, 10 * sizeof (char));
    int received = 0;
    
    recv(sock, length, 1+received, MSG_PEEK);
    while (length[received] != '\r'){
        received++;
        recv(sock, length, 1+received, MSG_PEEK);
    }

    //convert the chars from chunk length to int
    int convert[10];
    memset(convert, 0, 10 * sizeof (char));

    for(int i = 0; i < received; i++){
	//case 0-9 in ASCII
        if(length[i] <= 57){ 
	    convert[i] = length[i] - 48;
        } 

   	//case a-f in ASCII
        else {
	    convert[i] = length[i] - 87;
        }
    }

    int result = 0;
    for(int i = 0; i < received; i++ ){
        result += (int) (convert[i] << (received-1-i)*4);
    }
    
    //receive the chunk length and \r\n
    recv(sock, length, received+2, MSG_WAITALL);
    return result;
}


/*
 * This function sends a http request (via tcp) to www.ub.tu-berlin.de on port 80.
 * It prints title, link, description and date of the newest entry on
 * http://www.ub.tu-berlin.de/index.php?id=8339&type=100
 */
int get_http(char *domain_name, char *port) {
    int sock, status;
    struct addrinfo hints;
    struct addrinfo *servinfo;

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

    status = connect(sock, servinfo->ai_addr, servinfo->ai_addrlen);
    if (status == -1) {
        fprintf(stderr, "connect: %s\n", strerror(errno));
        return 4;
    }

    /* send http request */
    char msg[] = "GET /index.php?id=8339&type=100 HTTP/1.1\nHost: www.ub.tu-berlin.de\n\n";
    status = send(sock, msg, sizeof msg, 0);
    if (status == -1) {
        fprintf(stderr, "send: %s\n", strerror(errno));
        return 5;
    }

    /* receive message and write to msg_buffer */
    char buffer[MAX_MSG_LEN + 1];
    memset(buffer, 0, sizeof buffer);
    
    int header = 1; //is set to 0 when the end of the http header is reached
    int peaked = 0; //contains length of http header
    while (header){
            status = recv(sock, buffer, PEEK_LEN + peaked, MSG_PEEK);
            while (strncmp(buffer+peaked, "\r\n\r\n",strlen("\r\n\r\n")) != 0 && peaked <= status){
                peaked++;
            }
            if(strncmp(buffer+peaked, "\r\n\r\n",strlen("\r\n\r\n")) == 0){
                header = 0;
                peaked += 4;
            }
    }

    //store the header - can be discarded
    status = recv(sock, buffer, peaked, MSG_WAITALL);

    peaked = 0;

    //keep receiving chunks as long as there are remaining ones
    int chunk_len = get_chunk_len(sock, buffer); 
    while(chunk_len != 0){
        recv(sock, buffer+peaked, chunk_len, MSG_WAITALL);
	peaked += chunk_len;
	chunk_len = get_chunk_len(sock, buffer); 
    }

    buffer[peaked] = '\0';

    if (status == -1) {
        fprintf(stderr, "recv: %s\n", strerror(errno));
        return 6;
    }




    assert(status <= MAX_MSG_LEN);
    buffer[status] = '\0'; /* end string */

    status = close(sock);
    freeaddrinfo(servinfo);
    if (status == -1) {
        fprintf(stderr, "close: %s\n", strerror(errno));
        return 7;
    }

    //assume that xhtml file is formatted as wished

    //saves the position in the xhtml file
    int progress = 0;

    //look for the first news entry
    while (strncmp(buffer+progress, "<item>",strlen("<item>")) != 0){
        progress++;
    }

    //look for the title
    while (strncmp(buffer+progress, "<title>",strlen("<title>")) != 0){
        progress++;
    }
    progress += strlen("<title>");
    char* title = strtok(buffer+progress, "<");

    //search the link
    while (strncmp(buffer+progress, "<link>",strlen("<link>")) != 0){
        progress++;
    }
    progress += strlen("<link>");
    char* link = strtok(buffer+progress, "<");

    //search the description
    while (strncmp(buffer+progress, "<description>",strlen("<description>")) != 0){
       progress++;
    }
    progress += strlen("<description>");
    char* description = strtok(buffer+progress, "<");

    //search the date
    while (strncmp(buffer+progress, "<pubDate>",strlen("<pubDate>")) != 0){
        progress++;
    }
    progress += strlen("<pubDate>");
    char* date = strtok(buffer+progress, "<");


    printf("Titel: %s\n", title);
    printf("Link: %s\n", link);
    printf("Beschreibung: %s\n", description);
    printf("Datum: %s\n", date);

    return 0;
}

/* this programs connects to www.ub.tu-berlin.de on port 80
 * and downloads http://www.ub.tu-berlin.de/index.php?id=8339&type=100
 * It prints titel, link, description and date of the newest entry
 */
int main() {
    return get_http("www.ub.tu-berlin.de", HTTP_PORT);
}
