/***************************************************************************
 *
 * This file is part of the ProtSim framework developed by TKN for a
 * practical course on basics of simulation and Internet protocol functions
 *
 * Copyright:   (C)2004-2014 Telecommunication Networks Group (TKN) at
 *              Technische Universitaet Berlin, Germany.
 *
 * Authors:     Lars Westerhoff, Guenter Schaefer
 *
 **************************************************************************/

#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdbool.h>
#include <inttypes.h>
#include <limits.h>

#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <errno.h>

#include <time.h>
#include "timeval_macros.h"

#include "DataBuffer.h"
#include "SocketConnection.h"

#define DEBUG
#ifdef DEBUG
#define DEBUGOUT(...) fprintf(stderr, __VA_ARGS__)
#else
#define DEBUGOUT(...)
#endif

#define DEFAULT_LOCAL_PORT 12005
#define DEFAULT_REMOTE_PORT 4343
#define DEFAULT_PAYLOAD_SIZE 1024
#define MAX_FILE_SIZE 1024

struct timeval timeout;
unsigned       window;
unsigned       localPort, remotePort;
long           destNode, destAppl;
char*          remoteName;
char*          fileName;
DataBuffer     dataBuffer;

long           lastAckSeqNo;
long           nextSendSeqNo;

struct timeval timerExpiration;

void help(int exitCode) {
    fprintf(stderr,"GoBackNSender [--timeout|-t msec] [--window|-w count] [--dest|-d node/appl] [--local|-l port] [--remote|-r port] hostname file\n");

    exit(exitCode);
}

void initialize(int argc, char** argv) {
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;

    timerExpiration.tv_sec = LONG_MAX;
    timerExpiration.tv_usec = 0;

    window = 25;
    localPort = DEFAULT_LOCAL_PORT;
    remotePort = DEFAULT_REMOTE_PORT;

    bool destSet = false;

    while (1) {
        static struct option long_options[] = {
            {"timeout", 1, NULL, 't'},
            {"window", 1, NULL, 'w'},
            {"dest", 1, NULL, 'd'},
            {"local", 1, NULL, 'l'},
            {"remote", 1, NULL, 'r'},
            {"help", 0, NULL, 'h'},
            {0, 0, 0, 0}
        };

        int c = getopt_long (argc, argv, "t:w:d:l:r:h", long_options, NULL);
        if (c == -1) break;

        int retval = 0;
        switch (c) {
            case 't': {
                unsigned msec;
                retval = sscanf(optarg, "%u", &msec);
                if (retval < 1) help(1);
                timeout.tv_sec = msec / 1000;
                timeout.tv_usec = (msec % 1000)*1000;
            } break;

            case 'w':
                retval = sscanf(optarg, "%u", &window);
                if (retval < 1) help(1);
                break;

            case 'd':
                retval = sscanf(optarg, "%ld/%ld",&destNode, &destAppl);
                if (retval < 2) help(1);
                destSet = true;
                break;

            case 'l':
                retval = sscanf(optarg, "%u", &localPort);
                if (retval < 1) help(1);
                break;

            case 'r':
                retval = sscanf(optarg, "%u", &remotePort);
                if (retval < 1) help(1);
                break;

            case 'h':
                help(0);
                break;

            case '?':
                help(1);
                break;

            default:
                printf ("?? getopt returned character code 0%o ??\n", c);
        }
    }

    if (!destSet || argc < optind+2 || window <= 0) help(1);

    remoteName = argv[optind];
    fileName = argv[optind+1];

    dataBuffer = allocateDataBuffer(MAX_FILE_SIZE);

    lastAckSeqNo = nextSendSeqNo = 0;
}

bool readIntoBuffer(FILE* file, long seqNo) {
    DataPacket* dataPacket = (DataPacket*)malloc(sizeof(DataPacket));
    dataPacket->packet = (GoBackNMessageStruct*)malloc(sizeof(GoBackNMessageStruct)+DEFAULT_PAYLOAD_SIZE);
    dataPacket->packet->destNode = destNode;
    dataPacket->packet->destAppl = destAppl;
    dataPacket->packet->seqNo = seqNo;
    dataPacket->packet->seqNoExpected = -1;
    dataPacket->packet->hasErrors = false;
    size_t bytesRead = fread(dataPacket->packet->data, 1, DEFAULT_PAYLOAD_SIZE, file);
    DEBUGOUT("FILE: %u bytes read\n", (unsigned int) bytesRead);
    dataPacket->packet->size = bytesRead + sizeof(GoBackNMessageStruct);
    if (bytesRead < DEFAULT_PAYLOAD_SIZE) {
        if (ferror(file)) {
            perror("fread");
            exit(1);
        }
        if (bytesRead == 0) {
            putDataPacketIntoBuffer(dataBuffer, dataPacket);
            return false;
        }
    }
    putDataPacketIntoBuffer(dataBuffer, dataPacket);
    return true;
}

long readFileIntoBuffer() {
    long seqNo = 0;

    FILE* input = fopen(fileName, "rb");
    if (input == NULL) {
        perror("fopen");
        exit(1);
    }

    while (readIntoBuffer(input,seqNo)) ++seqNo;
    fclose(input);

    return seqNo;
}

int openConnection() {
    return connectToServer(localPort, remoteName, remotePort);
}

int main(int argc, char** argv) {
    initialize(argc, argv);

    // read file
    long veryLastSeqNo = readFileIntoBuffer();
    DEBUGOUT("veryLastSeqNo: %ld\n",veryLastSeqNo);

    // open connection
    int s = openConnection();
    
    // We are finished when there is no more data in the buffer to send most likely?
    // Maybe just a simple as using the getBufferSize() function?
    // Perhaps while (getBufferSize > 0) {...}
    while (getBufferSize(dataBuffer) > 0) {
        DEBUGOUT("nextSendSeqNo: %ld, lastAckSeqNo: %ld\n", nextSendSeqNo, lastAckSeqNo);

        fd_set readfds, writefds;
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        FD_SET(s, &readfds);

        if (nextSendSeqNo <= veryLastSeqNo &&
            nextSendSeqNo <= getLastSeqNoOfBuffer(dataBuffer) &&
            nextSendSeqNo < lastAckSeqNo + window) {
            FD_SET(s, &writefds);
        }

        struct timeval selectTimeout, currentTime;
        if (gettimeofday(&currentTime, NULL) < 0) {
            perror("gettimeofday");
            exit(1);
        }
        DEBUGOUT("current time: %ld,%ld\n",
                 currentTime.tv_sec, currentTime.tv_usec);
        timersub(&timerExpiration, &currentTime, &selectTimeout);

        if (selectTimeout.tv_sec < 0 || selectTimeout.tv_usec < 0) {
            fprintf(stderr, "WARNING: Timeout was negative (%ld,%ld)\n", selectTimeout.tv_sec, selectTimeout.tv_usec);
            selectTimeout.tv_sec = selectTimeout.tv_usec = 0;
        }
        
        int n;
        if ((n = select(s+1, &readfds, &writefds, NULL, &selectTimeout)) < 0) {
            perror("select");
            exit(1);
        }

        // Handle acknowledgements
        if (FD_ISSET(s, &readfds)) {
            int bytesRead = 0;
            GoBackNMessageStruct * ack = allocateGoBackNMessageStruct(0);
            if ((bytesRead = recv(s, ack, sizeof(*ack), MSG_DONTWAIT)) < 0) {
                perror("recv");
                exit(1);
            }
            DEBUGOUT("SOCKET: %d bytes received\n", bytesRead);

            /* YOUR TASK:
             * - Check acknowledgement for errors
             * - Are new packets acknowledged by this packet?
             * - Free buffers of acknowledged packets
             * - Set lastAckSeqNo as needed
             * - Recalculate timerExpiration, distinguish between two cases:
             *   # Still sent but unacknowledged packets in the buffer
             *   # No packets waiting for acknowledgement
             * - If needed: Set nextSendSeqNo so that never packets are sent
             *              that where already acknowledged
             *
             * FUNCTIONS YOU MAY NEED:
             * - freeBuffer()
             * - getDataPacketFromBuffer()
             */
            
            // Each packet is a GoBackNMessageStruct.
            // The struct has a flag hasErrors, which tells if if the
            // packet was damaged during transport. If it is, then we:
            //  - throw it away?
            // seqNo == -1 with ACKs
            // As we are going through, we should be updating the variable
            // lastAckSeqNo, with the next-expected packet of the receiver.
            // So here we con compare the seqNoExpected field of the 
            // GoBackNMessageStruct with lastAckSeqNo, and 
            // if seqNoExpected > lastAckSeqNo, then we have a new packet
            // that has been acknowledged.
            // seqNoOfACKdPacket = seqNoExpected - 1
            // Then we need to:
            //  - free the buffers of acknowledged packet(s? - can it be > 1?)
            //    - freeBuffer(dataBuffer, seqNoOfACKdPacket, seqNoOfACKdPacket)?
            //  - lastAckSeqNo = seqNoOfACKdPacket
            
            // timerExpiration
            // if still unacknowledged packets in buffer, then
            // DataPacket * next_packet = getDataPacketFromBuffer(dataBuffer, seqNoExpected); # I think?
            // timerExpiration = next_packet.timeout
            // if buffer is empty, then reset timeout?
            //  timerExpiration.tv_sec = LONG_MAX;
            
            long seqNoOfACKdPacket;
            
            if (ack->hasErrors) {
              // We ignore the ACK
              DEBUGOUT("RECEIVED PACKET HAD ERRORS. IGNORING...\n");
            } else {
              // We're only interested in ACK's that we haven't received yet
              // Any others we basically ignore
              if (ack->seqNoExpected > lastAckSeqNo) {
                // Sequence No. of the ACK'd packet is the one before
                // the one it's expecting as the next packet (PRETTY SURE?)
                // Note: This should normallyalso be the first packet in the buffer,
                // otherwise something has gone wrong (an ACK may have gone missing for example)
                seqNoOfACKdPacket = ack->seqNoExpected - 1;
                DEBUGOUT("ACK RECEIVED FOR: %ld\n", seqNoOfACKdPacket);
                // Find the first non-ACK'd packet in the buffer
                long seqNoOfFirstPacketInBuffer = getFirstSeqNoOfBuffer(dataBuffer);
                // Delete everything from first packet thru currently ACK'd packet from the buffer
                // NOTE: We can do this, because the receiver will only send ACK's for
                // packets that it has received in order. If we happen to receive an 
                // "out of order" ACK, this just means that one probably got lost or damaged during
                // transmission, but we can basically assume that every unacknowledge packet prior
                // to the currently ACK'd packet was also received.
                freeBuffer(dataBuffer, seqNoOfFirstPacketInBuffer, seqNoOfACKdPacket);
                lastAckSeqNo = seqNoOfACKdPacket;
                if (getBufferSize(dataBuffer) > 0) { // there are still unacknowledged packets 
                  // Adjust the timerExpiration to the next unacknowledged packet
                  DataPacket *next_unacknowledged_packet = getDataPacketFromBuffer(dataBuffer, ack->seqNoExpected);
                  timerExpiration.tv_sec = next_unacknowledged_packet->timeout.tv_sec;
                  timerExpiration.tv_usec = next_unacknowledged_packet->timeout.tv_usec;
                } else { // no more unacknowledged packets
                  // Reset the timerExpiration
                  timerExpiration.tv_sec = LONG_MAX;
                  timerExpiration.tv_usec = 0;
                }
              }
            }

            freeGoBackNMessageStruct(ack);
        }

        // Handle timeout
        if (gettimeofday(&currentTime, NULL) < 0) {
            perror("gettimeofday");
            exit(1);
        }
        DEBUGOUT("current time: %ld,%ld\n",
                 currentTime.tv_sec, currentTime.tv_usec);
        if (timercmp(&timerExpiration, &currentTime, <)) { // timerExpiration is in the past
            DEBUGOUT("TIMEOUT (Current: %ld,%ld; expiration: %ld,%ld)\n",
                     currentTime.tv_sec, currentTime.tv_usec,
                     timerExpiration.tv_sec, timerExpiration.tv_usec);
            /* YOUR TASK:
             * - Make sure that all unacknowledged packets will be resent (do
             *   NOT send them here!)
             * - Reset timers
             *
             * FUNCTIONS YOU MAY NEED:
             * - resetTimers()
             */
            // printBuffer(dataBuffer);
            DEBUGOUT("Flagging all unacknowledged packets for resend\n");
            nextSendSeqNo = (lastAckSeqNo + 1); // the next packet expected by the receiver
            DEBUGOUT("Resetting timerExpiration\n");
            // Reset the timerExpiration
            timerExpiration.tv_sec = LONG_MAX;
            timerExpiration.tv_usec = 0;
            
            resetTimers(dataBuffer);
        }

        // Send packets
        if (FD_ISSET(s, &writefds)) {
            // Everytime we send, we increment nextSendSeqNo (nextSendSeqNo++)
            // Everytime we acknowledge a packet, we increment lastAckSeqNo
            // And we have a window of max unacknowledged packets we can have
            // So basically we are allowed to send when:
            // (nextSendSeqNo - lastAckSeqNo) < window
            // But we also need to take care not to try and send more packets
            // than are actually in the buffer!! (otherwise = SEGFAULT)
            // So we could add && nextSendSeqNo <= getLastSeqNoOfBuffer(dataBuffer)
            // But actually, there also the variable veryLastSeqNo, which also has 
            // the last sequence number in the buffer, and it's more efficient to just 
            // compare with the variable than call the function each time.
            while (((nextSendSeqNo - lastAckSeqNo) < window) && (nextSendSeqNo <= veryLastSeqNo)) {
                DataPacket * data = getDataPacketFromBuffer(dataBuffer, nextSendSeqNo);
                DEBUGOUT("Sending Packet: SeqNo = %ld\n", nextSendSeqNo);
                // Send data
                int retval = send(s, data->packet, data->packet->size, MSG_DONTWAIT);
                if (retval < 0) {
                    if (errno == EAGAIN) break;
                    else {
                        perror("send");
                        exit(1);
                    }
                }

                DEBUGOUT("SOCKET: %d bytes sent\n", retval);

                /* YOUR TASK: Sending was successful, what now? 
                 * - Update sequence numbers
                 */
                nextSendSeqNo++;


                // Update timers
                struct timeval currentTime;
                if (gettimeofday(&currentTime, NULL) < 0) {
                    perror("gettimeofday");
                    exit(1);
                }
                DEBUGOUT("current time: %ld,%ld\n",
                         currentTime.tv_sec, currentTime.tv_usec);

                /* YOUR TASK:
                 * - Store the timeout with the packet
                 * - Recalculate timerExpiration if needed
                 *
                 * MACROS YOU MAY NEED:
                 * - timeradd(a, b, result)
                 * - timercmp(a, b, cmp) // NOTE: cmp is a comparison operator
                 *                          like <, >, <=, >=, ==
                 */
                
                // data->timeout = current time + timeout
                timeradd(&currentTime, &timeout, &(data->timeout));
                         
                // timerExpiration recalculation:
                // We only need to recalculate the timerExpiration if it already expired
                // ie. timerExpiration is in the past
                // ie. timerExpiration < currentTime
                
                if (timercmp(&timerExpiration, &currentTime, <)) {
                  timerExpiration.tv_sec = data->timeout.tv_sec;
                  timerExpiration.tv_usec = data->timeout.tv_usec;
                  DEBUGOUT("Set timerExpiration to: %ld,%ld\n", timerExpiration.tv_sec, timerExpiration.tv_usec);
                }
            }
        }
    }
    close(s);
    deallocateDataBuffer(dataBuffer);
    return 0;
}
