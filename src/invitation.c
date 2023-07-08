#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include "debug.h"
#include "client_registry.h"
#include "player.h"
#include "game.h"
#include "invitation.h"

typedef struct invitation{
    INVITATION_STATE invi_state;
    CLIENT *sender;
    CLIENT *reciever;
    GAME_ROLE sender_role;
    GAME_ROLE reciever_role;
    sem_t semaphore_block;
    GAME *game_state;
    int ref_count;
}INVITATION;

/*
 * Create an INVITATION in the OPEN state, containing reference to
 * specified source and target CLIENTs, which cannot be the same CLIENT.
 * The reference counts of the source and target are incremented to reflect
 * the stored references.
 *
 * @param source  The CLIENT that is the source of this INVITATION.
 * @param target  The CLIENT that is the target of this INVITATION.
 * @param source_role  The GAME_ROLE to be played by the source of this INVITATION.
 * @param target_role  The GAME_ROLE to be played by the target of this INVITATION.
 * @return a reference to the newly created INVITATION, if initialization
 * was successful, otherwise NULL.
 */
INVITATION *inv_create(CLIENT *source, CLIENT *target, GAME_ROLE source_role, GAME_ROLE target_role){
    INVITATION *new_inv = malloc(sizeof(INVITATION));
    sem_init(&((*new_inv).semaphore_block),0,1);
    new_inv->invi_state = INV_OPEN_STATE;
    new_inv->ref_count = 1;
    //make sure not the same
    if(source == target){
        return NULL;
    }
    else{
        source = client_ref(source, "new invitation source");
        target = client_ref(target, "new invitation target");
        debug("source: %p, target: %p", (void*)source, (void*)target);
        // if(strcmp(player_get_name(client_get_player(source)), player_get_name(client_get_player(target))) != 0){
        //     return NULL;
        // }
        new_inv->sender = source;
        new_inv->reciever = target;
        new_inv->sender_role = source_role;
        new_inv->reciever_role = target_role;
        new_inv->game_state = NULL;
        // if(client_make_invitation(source,target,source_role,target_role) == -1){
        //     return NULL;
        // }
        // if(client_add_invitation(source,new_inv) == -1 || client_add_invitation(target,new_inv) == -1){
        //     return NULL;
        // }
    }
    debug("NEW INVITE CREATED SENDING");
    return new_inv;
}

/*
 * Increase the reference count on an invitation by one.
 *
 * @param inv  The INVITATION whose reference count is to be increased.
 * @param why  A string describing the reason why the reference count is
 * being increased.  This is used for debugging printout, to help trace
 * the reference counting.
 * @return  The same INVITATION object that was passed as a parameter.
 */
INVITATION *inv_ref(INVITATION *inv, char *why){
    sem_wait(&inv->semaphore_block);
    //int new=++inv->ref_count;
    debug("(%d->%d) %s", inv->ref_count, inv->ref_count+1, why);
    inv->ref_count++;
    //debug("%s",why);
    sem_post(&inv->semaphore_block);
    return inv;
}

/*
 * Decrease the reference count on an invitation by one.
 * If after decrementing, the reference count has reached zero, then the
 * invitation and its contents are freed.
 *
 * @param inv  The INVITATION whose reference count is to be decreased.
 * @param why  A string describing the reason why the reference count is
 * being decreased.  This is used for debugging printout, to help trace
 * the reference counting.
 *
 */
void inv_unref(INVITATION *inv, char *why){
    // int a = 0;
     debug("Before inv_unref sem wait");
    //debug("Semph Value%d", sem_getvalue(&inv -> semaphore_block, &a));
    sem_wait(&inv->semaphore_block);
    debug("(%d->%d) %s", inv->ref_count, inv->ref_count-1, why);
    inv->ref_count--;
    if(inv->ref_count == 0){
        debug("Free invitation");
        client_unref(inv->sender, "sender in invitation unref");
        client_unref(inv->reciever, "receiver in invitation unref");
        if (inv->game_state != NULL) {
            game_unref(inv->game_state, "game in invitation unref");
        }
        sem_destroy(&inv->semaphore_block);
        free(inv);
    }
    sem_post(&inv->semaphore_block);
}

/*
 * Get the CLIENT that is the source of an INVITATION.
 * The reference count of the returned CLIENT is NOT incremented,
 * so the CLIENT reference should only be regarded as valid as
 * long as the INVITATION has not been freed.
 *
 * @param inv  The INVITATION to be queried.
 * @return the CLIENT that is the source of the INVITATION.
 */
CLIENT *inv_get_source(INVITATION *inv){
    return inv->sender;
}

/*
 * Get the CLIENT that is the target of an INVITATION.
 * The reference count of the returned CLIENT is NOT incremented,
 * so the CLIENT reference should only be regarded as valid if
 * the INVITATION has not been freed.
 *
 * @param inv  The INVITATION to be queried.
 * @return the CLIENT that is the target of the INVITATION.
 */
CLIENT *inv_get_target(INVITATION *inv){
    return inv->reciever;
}

/*
 * Get the GAME_ROLE to be played by the source of an INVITATION.
 *
 * @param inv  The INVITATION to be queried.
 * @return the GAME_ROLE played by the source of the INVITATION.
 */
GAME_ROLE inv_get_source_role(INVITATION *inv){
    return inv->sender_role;
}

/*
 * Get the GAME_ROLE to be played by the target of an INVITATION.
 *
 * @param inv  The INVITATION to be queried.
 * @return the GAME_ROLE played by the target of the INVITATION.
 */
GAME_ROLE inv_get_target_role(INVITATION *inv){
    return inv->reciever_role;
}

/*
 * Get the GAME (if any) associated with an INVITATION.
 * The reference count of the returned GAME is NOT incremented,
 * so the GAME reference should only be regarded as valid as long
 * as the INVITATION has not been freed.
 *
 * @param inv  The INVITATION to be queried.
 * @return the GAME associated with the INVITATION, if there is one,
 * otherwise NULL.
 */
GAME *inv_get_game(INVITATION *inv){
    if(inv->game_state == NULL){
        return NULL;
    }
    return inv->game_state;
}

/*
 * Accept an INVITATION, changing it from the OPEN to the
 * ACCEPTED state, and creating a new GAME.  If the INVITATION was
 * not previously in the the OPEN state then it is an error.
 *
 * @param inv  The INVITATION to be accepted.
 * @return 0 if the INVITATION was successfully accepted, otherwise -1.
 */
int inv_accept(INVITATION *inv){
    sem_wait(&inv->semaphore_block);
    debug("INVITATION ACCEPT");
    //if not init in open state return error
    if(inv->invi_state != INV_OPEN_STATE){
        return -1;
    }
    inv->invi_state = INV_ACCEPTED_STATE;
    inv->game_state = game_create();
    if(inv->game_state == NULL){
        return -1;
    }
    sem_post(&inv->semaphore_block);
    return 0;
}

/*
 * Close an INVITATION, changing it from either the OPEN state or the
 * ACCEPTED state to the CLOSED state.  If the INVITATION was not previously
 * in either the OPEN state or the ACCEPTED state, then it is an error.
 * If INVITATION that has a GAME in progress is closed, then the GAME
 * will be resigned by a specified player.
 *
 * @param inv  The INVITATION to be closed.
 * @param role  This parameter identifies the GAME_ROLE of the player that
 * should resign as a result of closing an INVITATION that has a game in
 * progress.  If NULL_ROLE is passed, then the invitation can only be
 * closed if there is no game in progress.
 * @return 0 if the INVITATION was successfully closed, otherwise -1.
 */
int inv_close(INVITATION *inv, GAME_ROLE role){
    //int a = 0;
    //debug("Semph Value%d", sem_getvalue(&inv -> semaphore_block, &a));
    sem_wait(&(inv->semaphore_block));
    debug("CLOSE INVITATION");
    
    if(inv->invi_state != INV_OPEN_STATE && inv->invi_state != INV_ACCEPTED_STATE){
        sem_post(&inv->semaphore_block);
        return -1;
    }

    //if null and no game in progress
    debug("GAME STATE: %p", inv->game_state);
    if(role == NULL_ROLE && inv->game_state == NULL){
        inv->invi_state = INV_CLOSED_STATE;
        sem_post(&inv->semaphore_block);
        debug("NULL ROLE AND NO GAME");
        inv_unref(inv, "invitation being removed from client's list");
        debug("RETURN 0 AFTER UNREF");
        return 0;
    }

    //1 if game is over, 0 if still going on
    if(game_is_over(inv->game_state) == 0){
        if(game_resign(inv->game_state,role) == -1){
            sem_post(&inv->semaphore_block);
            return -1;
        }
        else{
            inv->invi_state = INV_CLOSED_STATE;
            sem_post(&inv->semaphore_block);
            inv_unref(inv, "invitation being removed from client's list");
            return 0;
        }
    }
    inv->invi_state = INV_CLOSED_STATE;
    sem_post(&inv->semaphore_block);
    inv_unref(inv, "invitation being removed from client's list");
    return 0;
}