#ifndef COMMUNICATE_H
#define COMMUNICATE_H

#include <stdlib.h>
#include "game.h"

#define BUFF_SIZE 1024
#define BACKLOG 30 // Maximum number of client can connect to server
#define WAIT_TIME 30
#define SLEEP_TIME 5

// Define communicate message
typedef enum conn_msg_type_type_t
{
    JOIN,
    REFUSE,
    WAITING_ROOM,
    GAME_STATE,
    GUESS_CHAR,
    NOTIFICATION,
    SUB_QUESTION,
    END_GAME
} conn_msg_type_type;

// Define data of communicate message
typedef union conn_data_type_t
{
    game_state_type game_state;
    player_type player;
    waiting_room_type waiting_room;
    char notification[300];
    sub_question_type sub_question;
} conn_data_type;

// Define communicate message
typedef struct conn_msg_type_t
{
    conn_msg_type_type type;
    conn_data_type data;
} conn_msg_type;

// Define client's room
typedef struct client_room_type_t
{
    int connfd[PLAYER_PER_ROOM];
    char username[PLAYER_PER_ROOM][50];
    // status of client
    // 0: not ready, 1: ready, -1: AFK
    int status[PLAYER_PER_ROOM];
    int joined;
} client_room_type;

// Define function's prototype
conn_msg_type make_conn_msg(conn_msg_type_type type, conn_data_type data);

client_room_type *init_client_room();
void send_all(client_room_type client_room, conn_msg_type conn_msg);
int check_afk(int bytes_communicate, client_room_type *client_room, int turn);

// Define function's body



conn_msg_type make_conn_msg(conn_msg_type_type type, conn_data_type data)
{
    conn_msg_type conn_msg;
    conn_msg.type = type;
    switch (type)
    {
    case JOIN:
        copy_player_type(&conn_msg.data.player, data.player);
        break;

    case WAITING_ROOM:
        copy_waiting_room_type(&conn_msg.data.waiting_room, data.waiting_room);
        break;

    case GAME_STATE:
        copy_game_state_type(&conn_msg.data.game_state, data.game_state);
        break;

    case NOTIFICATION:
        strcpy(conn_msg.data.notification, data.notification);
        break;
        
    case GUESS_CHAR:
        copy_game_state_type(&conn_msg.data.game_state, data.game_state);
        break;

    case SUB_QUESTION:
        copy_sub_question_type(&conn_msg.data.sub_question, data.sub_question);
        break;

    case END_GAME:
        copy_game_state_type(&conn_msg.data.game_state, data.game_state);
        break;

    case REFUSE:
        strcpy(conn_msg.data.notification, data.notification);
        break;
    
    }
    return conn_msg;
}

client_room_type *init_client_room()
{
    // Initialize client room
    client_room_type *client_room = (client_room_type *)malloc(sizeof(client_room_type));
    for (int i = 0; i < client_room->joined; i++)
    {
        client_room->connfd[i] = -1;
        client_room->status[i] = 0;
    }
    client_room->joined = 0;
    return client_room;
}

void send_all(client_room_type client_room, conn_msg_type conn_msg)
{
    sleep(SLEEP_TIME);
    // Send message to all client in room
    int bytes_sent;
    for (int i = 0; i < client_room.joined; i++)
    {
        if(client_room.status[i] != 1) continue;
        printf("[DEBUG] Send to username: %s\n", client_room.username[i]);
        bytes_sent = send(client_room.connfd[i], &conn_msg, sizeof(conn_msg), 0);
        printf("[DEBUG] Bytes sent: %d\n", bytes_sent);
        check_afk(bytes_sent, &client_room, i);
    }
}

int check_afk(int bytes_communicate, client_room_type *client_room, int turn)
{
    if (bytes_communicate <= 0)
    {
        printf("[DEBUG] Client [%s] AFK\n", client_room->username[turn]);
        client_room->status[turn] = -1;

        // Close connection to this client
        close(client_room->connfd[turn]);
        return 1;
    }
    return 0;
};


#endif