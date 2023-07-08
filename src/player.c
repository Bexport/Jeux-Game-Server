#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include "player.h"
#include "debug.h"
#include "protocol.h"

typedef struct player{
    char *name;
    int rating;
    sem_t semaphore_block;
    unsigned int ref_count;
}PLAYER;

/*
 * Create a new PLAYER with a specified username.  A private copy is
 * made of the username that is passed.  The newly created PLAYER has
 * a reference count of one, corresponding to the reference that is
 * returned from this function.
 *
 * @param name  The username of the PLAYER.
 * @return  A reference to the newly created PLAYER, if initialization
 * was successful, otherwise NULL.
 */
PLAYER *player_create(char *name){
    //when does creawting a new player not work
    PLAYER *new_player = malloc(sizeof(PLAYER));
    if (new_player == NULL) {
        return NULL;
    }

    sem_init(&((*new_player).semaphore_block),0,1);
    //new_player->name = name;
    new_player->name = strdup(name);
    if (new_player->name == NULL) {
        free(new_player);
        return NULL;
    }
    new_player->ref_count = 1;
    new_player->rating = PLAYER_INITIAL_RATING;
    return new_player;
}

/*
 * Increase the reference count on a player by one.
 *
 * @param player  The PLAYER whose reference count is to be increased.
 * @param why  A string describing the reason why the reference count is
 * being increased.  This is used for debugging printout, to help trace
 * the reference counting.
 * @return  The same PLAYER object that was passed as a parameter.
 */
PLAYER *player_ref(PLAYER *player, char *why){
    sem_wait(&player->semaphore_block);
    //int new=++inv->ref_count;
    debug("(%d->%d) %s", player->ref_count, player->ref_count+1, why);
    player->ref_count++;
    //debug("%s",why);
    sem_post(&player->semaphore_block);
    return player;
}

/*
 * Decrease the reference count on a PLAYER by one.
 * If after decrementing, the reference count has reached zero, then the
 * PLAYER and its contents are freed.
 *
 * @param player  The PLAYER whose reference count is to be decreased.
 * @param why  A string describing the reason why the reference count is
 * being decreased.  This is used for debugging printout, to help trace
 * the reference counting.
 *
 */
void player_unref(PLAYER *player, char *why){
    sem_wait(&player->semaphore_block);
    debug("(%d->%d) %s", player->ref_count, player->ref_count-1, why);
    player->ref_count--;
    if(player->ref_count == 0){
        free(player);
    }
    sem_post(&player->semaphore_block);
}

/*
 * Get the username of a player.
 *
 * @param player  The PLAYER that is to be queried.
 * @return the username of the player.
 */
char *player_get_name(PLAYER *player){
    return player->name;
}

/*
 * Get the rating of a player.
 *
 * @param player  The PLAYER that is to be queried.
 * @return the rating of the player.
 */
int player_get_rating(PLAYER *player){
    return player->rating;
}

/*
 * Post the result of a game between two players.
 * To update ratings, we use a system of a type devised by Arpad Elo,
 * similar to that used by the US Chess Federation.
 * The player's ratings are updated as follows:
 * Assign each player a score of 0, 0.5, or 1, according to whether that
 * player lost, drew, or won the game.
 * Let S1 and S2 be the scores achieved by player1 and player2, respectively.
 * Let R1 and R2 be the current ratings of player1 and player2, respectively.
 * Let E1 = 1/(1 + 10**((R2-R1)/400)), and
 *     E2 = 1/(1 + 10**((R1-R2)/400))
 * Update the players ratings to R1' and R2' using the formula:
 *     R1' = R1 + 32*(S1-E1)
 *     R2' = R2 + 32*(S2-E2)
 *
 * @param player1  One of the PLAYERs that is to be updated.
 * @param player2  The other PLAYER that is to be updated.
 * @param result   0 if draw, 1 if player1 won, 2 if player2 won.
 */
void player_post_result(PLAYER *player1, PLAYER *player2, int result){
    int S1,S2;
    if(result == 0){
        S1 = 0.5;
        S2 = 0.5;
    }
    else if(result == 1){
        S1 = 1;
        S2 = 0;
    }
    else if(result == 2){
        S1 = 0;
        S2 = 1;
    }
    int R1 = player1->rating;
    int R2 = player2->rating;
    //DOUBLE CHECK THE CALCULATIONS FOR E1 AND E2 TO SEE IF IT REQUIRES A POINTER INSTEAD
    int E1 = 1/(1 + 10*((R2-R1)/400));
    int E2 = 1/(1 + 10*((R1-R2)/400));
    R1 = R1 + 32*(S1-E1);
    R2 = R2 + 32*(S2-E2);
}