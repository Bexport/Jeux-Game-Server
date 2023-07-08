#include "protocol.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include "debug.h"
#include "csapp.h"

/*
 * Send a packet, which consists of a fixed-size header followed by an
 * optional associated data payload.
 *
 * @param fd  The file descriptor on which packet is to be sent.
 * @param hdr  The fixed-size packet header, with multi-byte fields
 *   in network byte order
 * @param data  The data payload, or NULL, if there is none.
 * @return  0 in case of successful transmission, -1 otherwise.
 *   In the latter case, errno is set to indicate the error.
 *
 * All multi-byte fields in the packet are assumed to be in network byte order.
 */
int proto_send_packet(int fd, JEUX_PACKET_HEADER *hdr, void *data){
    debug("SENDING PACKET");
    JEUX_PACKET_HEADER *pHead = hdr;
    void* incomingPayload = data;

    //write header to "wire" connection first
    int status = write(fd,pHead,sizeof(JEUX_PACKET_HEADER));
    if(status == 0){
        errno = EOF;
        return -1;
    }
    else if(status < 0){
        errno = EINTR;
        return -1;
    }

    //if size is 0 and (maybe data != null) write payload immediately
    //convert multi-byte quantities from host to network byte order
    if(htons(pHead->size) > 0 && data != NULL){
        int status2 = write(fd,incomingPayload,htons(pHead->size));
        if(status2 == 0){
            errno = EOF;
            return -1;
        }
        else if(status2 < 0){
            errno = EINTR;
            return -1;
        }
    }
    //successful write
    return 0;
}

/*
 * Receive a packet, blocking until one is available.
 *
 * @param fd  The file descriptor from which the packet is to be received.
 * @param hdr  Pointer to caller-supplied storage for the fixed-size
 *   packet header.
 * @param datap  Pointer to a variable into which to store a pointer to any
 *   payload received.
 * @return  0 in case of successful reception, -1 otherwise.  In the
 *   latter case, errno is set to indicate the error.
 *
 * The returned packet has all multi-byte fields in network byte order.
 * If the returned payload pointer is non-NULL, then the caller has the
 * responsibility of freeing that storage.
 */
int proto_recv_packet(int fd, JEUX_PACKET_HEADER *hdr, void **payloadp){
    debug("GETTING PACKET");
    int status = read(fd, hdr, sizeof(JEUX_PACKET_HEADER));
    if(status == 0){
        errno = EOF;
        return -1;
    }
    else if (status < 0){
        errno = EINTR;
        return -1;
    }
    debug("Read %d bytes", status);

    // do i read uint8_t type as well?
    hdr->size = ntohs(hdr->size);

    //A pointer to the payload is stored in a variable supplied by the caller
    if ((hdr->size) > 0){
        *payloadp = malloc(hdr->size);
        int status2 =read(fd, *payloadp, hdr->size);
        if(status2 == 0){
            errno = EOF;
            return -1;
        }
        else if(status2 < 0){
            errno = EINTR;
            return -1;
        }
        ((char *)*payloadp)[hdr->size] = '\0'; // add null terminator
    }
    return 0;
}