#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <getopt.h>

#include "debug.h"
#include "protocol.h"
#include "server.h"
#include "client_registry.h"
#include "player_registry.h"
#include "jeux_globals.h"
#include "csapp.h"

#ifdef DEBUG
int _debug_packets_ = 1;
#endif

static void terminate(int status);
void *thread(void *vargp);

// sighup handler
void sighup_handler(int signum) {
    //printf for test
    debug("Received SIGHUP signal");
    //exit(EXIT_SUCCESS);
    //is this what it means to clean up 
    terminate(EXIT_SUCCESS);
}

/*
 * "Jeux" game server.
 *
 * Usage: jeux <port>
 */
int main(int argc, char* argv[]){
    // Option processing should be performed here.
    // Option '-p <port>' is required in order to specify the port number
    // on which the server should listen.

    // Obtain the port number from the command-line arguments
    int opt, port;
    while ((opt = getopt(argc, argv, "p:")) != -1) {
        switch (opt) {
            case 'p':
                port = atoi(optarg);
                break;
        }
    }

    // Perform required initializations of the client_registry and
    // player_registry.
    client_registry = creg_init();
    player_registry = preg_init();

    // TODO: Set up the server socket and enter a loop to accept connections
    // on this socket.  For each connection, a thread should be started to
    // run function jeux_client_service().  In addition, you should install
    // a SIGHUP handler, so that receipt of SIGHUP will perform a clean
    // shutdown of the server.

    // Install a SIGHUP handler for clean termination
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = sighup_handler;
    sigaction(SIGHUP, &act, NULL);
    //TEST FOR SIGINT
    // struct sigaction act2;
    // memset(&act, 0, sizeof(act2));
    // act2.sa_handler = sigint_handler;
    // sigaction(SIGTERM, &act, NULL);

    // Server socket setup and enter loop to accept connections on socket and start new thread for each connection
    int listenfd, *connfdp;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    //listen from this port number
    listenfd = open_listenfd(port);

    while (1) {
        debug("listening on port %d", port);
        clientlen=sizeof(struct sockaddr_storage);
        connfdp = malloc(sizeof(int));
        *connfdp = accept(listenfd, (SA*) &clientaddr, &clientlen);
        pthread_create(&tid, NULL, thread, connfdp);
    }

    // fprintf(stderr, "You have to finish implementing main() "
	//     "before the Jeux server will function.\n");
    // terminate(EXIT_FAILURE);
}

/* Thread routine */
void *thread(void *vargp){
    int *temp = malloc(sizeof(int));
    int connfd = *((int *)vargp);
    *temp = connfd;
    
    pthread_detach(pthread_self());
    //echo(connfd);

    //connection descriptor
    free(vargp);
    jeux_client_service(temp);

    //free(temp);
    close(connfd);
    return NULL;
}


/*
 * Function called to cleanly shut down the server.
 */
void terminate(int status) {
    // Shutdown all client connections.
    // This will trigger the eventual termination of service threads.
    creg_shutdown_all(client_registry);
    
    debug("%ld: Waiting for service threads to terminate...", pthread_self());
    creg_wait_for_empty(client_registry);
    debug("%ld: All service threads terminated.", pthread_self());

    // Finalize modules.
    creg_fini(client_registry);
    preg_fini(player_registry);

    debug("%ld: Jeux server terminating", pthread_self());
    exit(status);
}
