#include <stdlib.h>
#include <string.h>
#include <semaphore.h>

#include "debug.h"
#include "csapp.h"
#include "client_registry.h"

//struct
//double check what things to add and shit
typedef struct client_registry {
    //max number of clients is 64
    CLIENT *clients[MAX_CLIENTS];
    //name variable? (make it the username it's logined under)
    //how do i set this though
    //char *name;
    //unsigned int client_index;
    unsigned int client_count;
    sem_t semaphore_block;
    sem_t empty_sem;
}CLIENT_REGISTRY;

/*
 * Initialize a new client registry.
 *
 * @return  the newly initialized client registry, or NULL if initialization
 * fails.
 */
CLIENT_REGISTRY *creg_init(){
    debug("CREG INIT ENTER");
    CLIENT_REGISTRY *new_reg = malloc(sizeof(CLIENT_REGISTRY));
    //(semaphore address, 0 b/c shared btwn threads not processes,init value for how many threads are running at the same time)
    sem_init(&((*new_reg).semaphore_block),0,1);
    sem_init(&((*new_reg).empty_sem),0,0);
    new_reg->client_count=0;
    //how do i set the name variable

    //go through array and set values
    for (int i=0; i<MAX_CLIENTS; i++)
    {
        //set array element to NULL to indicate that it's unused
        new_reg->clients[i] = NULL;
        //set client index to 0 to init
        //new_reg->client_index = 0;
    }
    debug("CREG INIT EXIT");
    return new_reg;
}

/*
 * Finalize a client registry, freeing all associated resources.
 * This method should not be called unless there are no currently
 * registered clients.
 *
 * @param cr  The client registry to be finalized, which must not
 * be referenced again.
 */
void creg_fini(CLIENT_REGISTRY *cr){
    debug("CREG FINI");
    // sem_wait(&(cr->empty_sem));
    // sem_destroy(&(cr->semaphore_block));
    // sem_destroy(&(cr->empty_sem));
    free(cr);
}

/*
 * Register a client file descriptor.
 * If successful, returns a reference to the the newly registered CLIENT,
 * otherwise NULL.  The returned CLIENT has a reference count of one.
 *
 * @param cr  The client registry.
 * @param fd  The file descriptor to be registered.
 * @return a reference to the newly registered CLIENT, if registration
 * is successful, otherwise NULL.
 */
CLIENT *creg_register(CLIENT_REGISTRY *cr, int fd){
    debug("CREG REGISTER ENTER");
    //make new client, returned client has ref count = 1 and in logged out state
    sem_wait(&(cr->semaphore_block));
    CLIENT *newClient = client_create(cr,fd);
    if (newClient == NULL) {
        return NULL;
    }

    int i;
    for (i = 0; i < MAX_CLIENTS; i++) {
        if (cr->clients[i] == NULL) {
            cr->clients[i] = newClient;
            debug("REGISTER cr->clients[%d]: %p\n", i, cr->clients[i]);
            //cr->client_index = i;
            cr->client_count++;
            break;
        }
    }

    //if end of array reached then no space is found
    if (i == MAX_CLIENTS) {
        return NULL;
    }
    //return address of index of array (DOUBLE CHECK THIS)
    //or do i return the newClient itself after putting the fd into the array
    //return &cr->clients[i];
    sem_post(&(cr->semaphore_block));
    debug("CREG REGISTER Exit");
    return newClient;
}

/*
 * Unregister a CLIENT, removing it from the registry.
 * The client reference count is decreased by one to account for the
 * pointer discarded by the client registry.  If the number of registered
 * clients is now zero, then any threads that are blocked in
 * creg_wait_for_empty() waiting for this situation to occur are allowed
 * to proceed.  It is an error if the CLIENT is not currently registered
 * when this function is called.
 *
 * @param cr  The client registry.
 * @param client  The CLIENT to be unregistered.
 * @return 0  if unregistration succeeds, otherwise -1.
 */
int creg_unregister(CLIENT_REGISTRY *cr, CLIENT *client){
    debug("CREG UNREG ENTER");
    sem_wait(&(cr->semaphore_block));
    //find matching fd in the thing
    int found = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_get_fd(cr->clients[i]) == client_get_fd(client)) {
            cr->clients[i] = NULL;
            cr->client_count--;
            found = 1;
            break;
        }
    }

    //if not found then unlock
    if(found==0){
        sem_post(&(cr->semaphore_block));
        return -1;
    }

    //unref
    client_unref(client, "Unregister ref--");

    //if # of ref clients is 0, continue
    if (cr->client_count == 0) { 
        //DOUBLE CHECK THIS CONDITION
        //function shall unblock all threads currently blocked on the specified condition variable cond.
        // pthread_cond_broadcast(&(cr->semaphore_block));
        sem_post(&cr->empty_sem);
    }
    sem_post(&(cr->semaphore_block)); 
    debug("CREG UNREG exit");
    return 0;
}

/*
 * Given a username, return the CLIENT that is logged in under that
 * username.  The reference count of the returned CLIENT is
 * incremented by one to account for the reference returned.
 *
 * @param cr  The registry in which the lookup is to be performed.
 * @param user  The username that is to be looked up.
 * @return the CLIENT currently registered under the specified
 * username, if there is one, otherwise NULL.
 */
CLIENT *creg_lookup(CLIENT_REGISTRY *cr, char *user){
    debug("CREG LOOKUP ENTER");
    CLIENT *foundClient = NULL;
    sem_wait(&(cr->semaphore_block));
    int i=0;
    for(; i<MAX_CLIENTS; i++) {
        // FIND ANOTHER WAY TO COMPARE NAMES OR SOMETHING
        PLAYER *targetPlayer = client_get_player(cr->clients[i]);
        char *playerName = player_get_name(targetPlayer);
        if(cr->clients[i] != NULL && strcmp(playerName, user) == 0) {
            foundClient = cr->clients[i];
            foundClient = client_ref(foundClient, "lookup ref++");
            break;
        }
    }
    if(i==MAX_CLIENTS){
        return NULL;
    }
    sem_post(&(cr->semaphore_block));
    debug("CREG LOOKUP EXIT");

    return foundClient;
}

/*
 * Return a list of all currently logged in players.  The result is
 * returned as a malloc'ed array of PLAYER pointers, with a NULL
 * pointer marking the end of the array.  It is the caller's
 * responsibility to decrement the reference count of each of the
 * entries and to free the array when it is no longer needed.
 *
 * @param cr  The registry for which the set of usernames is to be
 * obtained.
 * @return the list of players as a NULL-terminated array of pointers.
 */
PLAYER **creg_all_players(CLIENT_REGISTRY *cr){
    debug("CREG ALL PLAYERS ENTER");
    sem_wait(&cr->semaphore_block); // acquire the registry's mutex

    int count = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (cr->clients[i] != NULL) {
            count++;
            debug("Player Count: %d", count);
        }
    }

    PLAYER **players = malloc(sizeof(PLAYER *) * (count + 1)); // allocate array for players
    //PLAYER **players[8056];
    debug("Size of players: %lu bytes\n", sizeof(PLAYER *) * (count + 1));
    int j = 0;
    for (int i = 0; i < count; i++) {
        //USE CLIENT_GET_PLAYER HERE ON EACH INDEX
        debug("client get player at index: %d", i);
        debug("ALL PLAYERS cr->clients[%d]: %p\n", i, cr->clients[i]);
        PLAYER *targetPlayer = client_get_player(cr->clients[i]);
        debug("TARGET PLAYER: %p", targetPlayer);
        // if (cr->clients[i] != NULL && targetPlayer != NULL) {
        if (targetPlayer != NULL) {
            players[j++] = targetPlayer;
            //P(&cr->client[i]->player->mutex); // acquire each player's mutex to increment their ref count
            //do something with the ref count?

            //V(&cr->client[i]->player->mutex); // release each player's mutex
        }
    }
    players[count] = NULL; // mark end of array with NULL pointer
    sem_post(&cr->semaphore_block); // release the registry's mutex
    debug("CREG ALL PLAYERS ENTER");

    return players;
}

/*
 * A thread calling this function will block in the call until
 * the number of registered clients has reached zero, at which
 * point the function will return.  Note that this function may be
 * called concurrently by an arbitrary number of threads.
 *
 * @param cr  The client registry.
 */
void creg_wait_for_empty(CLIENT_REGISTRY *cr){
    debug("CREG WAIT EMPTY");
    sem_wait(&(cr->semaphore_block));
    return;
}

/*
 * Shut down (using shutdown(2)) all the sockets for connections
 * to currently registered clients.  The clients are not unregistered
 * by this function.  It is intended that the clients will be
 * unregistered by the threads servicing their connections, once
 * those server threads have recognized the EOF on the connection
 * that has resulted from the socket shutdown.
 *
 * @param cr  The client registry.
 */
void creg_shutdown_all(CLIENT_REGISTRY *cr){
    debug("CREG SHUTDOWN ALL");
    sem_wait(&(cr->semaphore_block));
    for (int i=0; i<MAX_CLIENTS; i++)
    {
        if(cr->clients[i] != NULL){
            shutdown(client_get_fd(cr->clients[i]), SHUT_RD);
            //cr -> client_count--;
        }
    }
    sem_post(&(cr->semaphore_block));
}
