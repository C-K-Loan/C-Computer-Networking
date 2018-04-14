//HOMEWORK 2016
/*

reference
//Compile with  gcc -std=c99 tcpVideoServer.c -o tcpVS -D_POSIX_C_SOURCE=200112L 

https://www.youtube.com/watch?v=k8ISU3gfI1E
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <strings.h>
#include <time.h>
#define h_addr h_addr_list[0] /* for backward compatibility */
#define MAX_BUFFER_LENGTH 100
#define BACKLOG 1


int packData(unsigned char *buffer, unsigned int a, unsigned int b) {
    // assuming each unsigned int is 16 bits/2 bytes, which we need to pack
    // into the 4 bytes available in the buffer
    buffer[0] = a>>8; // most-significant byte
    buffer[1] = a;    // least-significant byte
    buffer[2] = b>>8; // most-significant byte
    buffer[3] = b;    // least-significant byte
    return 0;
}

int main(int argc, char *argv[])
{
    printf("Streaming socket client\n\n");
    
    if (argc != 3) {// arg 0 = ./ , arg1=DNS , arg2 = Port, insgesamt 3 argumente
    
        fprintf(stderr,"USAGE :TCP SERVER,Port\n");

        exit(EXIT_FAILURE);
    }




    int sockfd,clientSockFd;
    uint8_t buf[MAX_BUFFER_LENGTH];
    struct sockaddr_in their_addr, server_addr, client_addr; // the sock  we recieve the info from the server
    socklen_t addr_size;
    struct hostent *he;
    int server_port;
    int a = 0;
    int b = 0;
    uint8_t GET_Mask = 0x4;
    uint8_t DEL_Mask = 0x1;
    uint8_t SET_Mask = 0x2;
    
    int numbytes;
    struct addrinfo hints, *res;
   // int GETrq,DELrq;SETrq;// 0 for not requested, 1 for requested action
    char key[MAX_BUFFER_LENGTH];//save key here
    char value[MAX_BUFFER_LENGTH];//Save Value here
    int transaction_ID; //ID


    //Memory setup
    memset(&hints, 0, sizeof (hints));
    hints.ai_family = AF_UNSPEC;  
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;     
    getaddrinfo(NULL, argv[1], &hints, &res);

    
    // Create the socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);   
    if (sockfd<0)
        {
            printf("FAILURE CREATING SOCKET!\n");
            exit(EXIT_FAILURE);
        }
    printf("Socket created with sock fd: %i \n",sockfd);


   //setup server addr
    bzero((char *)&server_addr, sizeof(server_addr));
    server_port = atoi(argv[2]);
    server_addr.sin_family=AF_INET;
    server_addr.sin_port = htons(server_port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);



    //bind server adress
   if (bind(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
      perror("ERROR on binding");
      exit(1);
   }
      

        //listen for someone who wants to do some tcp business with us 
        if (listen(sockfd,5)==-1){
            printf("FAILURE LISTENING\n");
        }
        printf("Heared Something, going into main Loop \n");

      //server , tcp exchange time
      while (1){
        printf("beginning of Main loop\n");
        addr_size = sizeof(client_addr);
        clientSockFd = accept(sockfd,(struct sockaddr *) &client_addr, &addr_size);
        if (clientSockFd <0){
        printf("FAILURE CCEPTING\n");
        exit(1);}
        printf("Accepted!\n");
        int bitsRecv=0;
        bitsRecv= recv(clientSockFd,  buf, 100, 0 );
        printf("Recieved%d\n",bitsRecv );
        int i =0;
        int a =0;


        while (i != bitsRecv){
           printf("Recieved BIT NR %d  AS: %d and interpreted as a char %c  AND ",i,buf[i], buf[i]);
            //      if (buf[i] == "a") printf("IT IS A STRING ON POSITION %d \n",i );
            if (buf[i]!=0) printf(" IT IS NOT 0  \n");
            if (buf[i]==0) printf("IT IS 0\n");

            
            i++;
        }

        int get,set,del,ack;


        if((buf[0] & GET_Mask) != 0) {

            printf("GET DETECTED\n");

        }

        if((buf[0] & DEL_Mask) != 0) {

            printf("DEL DETECTED\n");

        }

        if((buf[0] & SET_Mask) != 0) {

            printf("SET DETECTED\n");

        }
        //check if get requested
        // if(buf[5]!=0){
        //     printf("get request detected\n");
        // }

        // //check if set requested
        // if(buf[5]!=0){
        //     printf("set request detected\n");
        // }

        // //check if del requested
        // if(buf[6]!=0){
        //     printf("del request detected\n");
        // }





        printf("____________|_____|___WAITING____FOR___PACKET___|_____|________________\n");
        memset(&buf, 0, sizeof (buf));


   }

    //old send from qout server
   // send (clientSockFd,singleLine[i], sizeof(singleLine),0);

    //closing time
    close(clientSockFd);
    close(sockfd);

    return 0;

}

