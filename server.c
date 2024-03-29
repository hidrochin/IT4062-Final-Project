#include <stdio.h> /* These are the usual header files */
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "game.h"
#include "communicate.h"
#include <ctype.h>
#include <signal.h>

 
// Global variables
client_room_type client_room;

// Handle client tasks
void *client_handle(void *arg);

int specify_turn(int turn, client_room_type client_room);

int is_all_afk(client_room_type client_room);

// Summary game after game is over
void summary(game_state_type *game_state);

// Check if username is exist in waiting room
int is_exist_username(waiting_room_type waiting_room, char *username);

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Usage: %s <Server Port>\n", argv[0]);
        return 0;
    }

    int listenfd, *connfd;
    int break_nested_loop = 0;
    struct sockaddr_in server, *client; // Server's address information
    int sin_size;
    pthread_t tid;
    int bytes_received, bytes_sent;
    int current_joined;
    char temp[300];

    // Ignore SIGPIPE signal (when server try to send data to client but client is disconnected)
    sigaction(SIGPIPE, &(struct sigaction){SIG_IGN}, NULL);

    // Create new communicate message variable
    conn_msg_type conn_msg;

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    { /* calls sockets() */
        perror("\nError: ");
        return 0;
    }

    // Set client waiting time, if client doesn't send any data in 
    // WAIT_TIME seconds, server will close connection (AFK)
    struct timeval timeout;
    timeout.tv_sec = WAIT_TIME;
    timeout.tv_usec = 0;

    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(atoi(argv[1]));
    server.sin_addr.s_addr = htonl(INADDR_ANY); /* INADDR_ANY puts your IP address automatically */

    if (bind(listenfd, (struct sockaddr *)&server, sizeof(server)) == -1)
    {
        perror("\nError: ");
        return 0;
    }

    if (listen(listenfd, BACKLOG) == -1)
    {
        perror("\nError: ");
        return 0;
    }

    sin_size = sizeof(struct sockaddr_in);
    client = malloc(sin_size);

    while (1)
    {
        client_room_type *client_room = init_client_room();
        waiting_room_type waiting_room = init_waiting_room();
        current_joined = 0;

        // Waiting for client to connect (3 clients)
        while (current_joined < PLAYER_PER_ROOM)
        {
            if ((client_room->connfd[current_joined] = accept(listenfd, (struct sockaddr *)client, &sin_size)) == -1)
                perror("\nError: ");
            printf("You got a connection from %s\n", inet_ntoa((*client).sin_addr)); /* Print client's IP */

            // Set waiting time for this client
            setsockopt(client_room->connfd[current_joined], SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));

            // Receive username from client
            bytes_received = recv(client_room->connfd[current_joined], &conn_msg, sizeof(conn_msg), 0);
            if (bytes_received <= 0)
            {
                close(client_room->connfd[current_joined]);
                continue;
            }

            // Check if username is exist
            break_nested_loop = 0;
            while (is_exist_username(waiting_room, conn_msg.data.player.username))
            {
                // Send notification to client
                strcpy(temp, conn_msg.data.player.username);
                sprintf(conn_msg.data.notification, "Username %s is already exist. Please choose another username", temp);

                conn_msg = make_conn_msg(REFUSE, conn_msg.data);
                bytes_sent = send(client_room->connfd[current_joined], &conn_msg, sizeof(conn_msg), 0);

                if (bytes_sent <= 0)
                {
                    break_nested_loop = 1;
                    break;
                }
                
                bytes_received = recv(client_room->connfd[current_joined], &conn_msg, sizeof(conn_msg), 0);
                if (bytes_received <= 0)
                {
                    break_nested_loop = 1;
                    break;
                }
            }

            if (break_nested_loop)
            {
                close(client_room->connfd[current_joined]);
                continue;
            }

            // If username is not exist, add it to waiting room
            printf("Waiting room: received %d bytes\n", bytes_received);
            fflush(stdout);
            strcpy(client_room->username[current_joined], conn_msg.data.player.username);
            strcpy(waiting_room.player[current_joined].username, conn_msg.data.player.username);

            // Set active status
            client_room->status[current_joined] = 1;

            client_room->joined++;
            waiting_room.joined++;

            // Send waiting room to client
            copy_waiting_room_type(&conn_msg.data.waiting_room, waiting_room);
            conn_msg = make_conn_msg(WAITING_ROOM, conn_msg.data);
            send_all(*client_room, conn_msg);

            current_joined++;
        }

        // For each client's room, spawns a thread, and the thread handles the new client's room
        pthread_create(&tid, NULL, &client_handle, (void *)client_room);
    }
}

void print_crossword(game_state_type *game_state) {
    printf("[DEBUG] Current Crossword: %s\n", game_state->crossword);
}


void *client_handle(void *arg)
{

    int i;
    srand(time(0));
    client_room_type client_room = *(client_room_type *)arg;

    free(arg);
    int correct = 1;
    char guess_char;
    int is_afk = 0;

    int bytes_sent, bytes_received;
    conn_msg_type conn_msg;

    // Init key
    char key[50];
    char main_question[50];
    init_key(main_question, key);
    key[strlen(key) - 1] = '\0';

    // Init game state
    game_state_type game_state = init_game_state(key, main_question);

    // Init player
    for (i = 0; i < client_room.joined; i++)
    {
        game_state.player[i] = init_player(client_room.username[i], client_room.connfd[i]);
    }
 
    for (i = 0; i < client_room.joined; i++)
    {
        printf("Player %d: %s\n", i, game_state.player[i].username);
    }

    printf("Question: %s\n", game_state.main_question);
    printf("Key: %s\nCrossword: %s\n", key, game_state.crossword);

    // Loop while crosswords are not solved
    while (strcmp(game_state.crossword, key) != 0)
    {

        // Clear previous game message
        sprintf(game_state.game_message, "%s", "");
        printf("\n\n\n");
        for (i = 0; i < client_room.joined; i++)
        {
            printf("[DEBUG] Client %d status: %d\n", i, client_room.status[i]);
        }
        // Check AFK
        if (is_all_afk(client_room))
        {
            printf("All players are AFK\n");
            break;
        }
        // Specify current player if previous player's guess is incorrect
        if (correct == 0)
        {
            game_state.turn = specify_turn(game_state.turn, client_room);
            printf("[DEBUG] Current turn: %s\n", game_state.player[game_state.turn].username);
        }

        // Set sector of wheel
        roll_wheel(&game_state);
        printf("[DEBUG] Sector: %d\n", game_state.sector);

        print_crossword(&game_state);

        // Handle sector's case
        switch (game_state.sector)
        {
        case -1:

            get_sub_question(&conn_msg.data.sub_question, game_state.player[game_state.turn].username);

            // Send sub question to all clients
            conn_msg = make_conn_msg(SUB_QUESTION, conn_msg.data);
            send_all(client_room, conn_msg);

            // TRICK to handle bug:
            // Check guess_char is alphabet or not
            guess_char = '0';
            while (!isalpha(guess_char))
            {
                bytes_received = recv(client_room.connfd[game_state.turn], &conn_msg, sizeof(conn_msg), 0);
                // Handle AFK
                if ((is_afk = check_afk(bytes_received, &client_room, game_state.turn)))
                {

                    // Send afk notification to all clients
                    sprintf(conn_msg.data.notification, "[%s] is AFK", game_state.player[game_state.turn].username);
                    conn_msg = make_conn_msg(NOTIFICATION, conn_msg.data);
                    send_all(client_room, conn_msg);

                    // Skip this player's turn
                    correct = 0;
                    break;
                }

                guess_char = conn_msg.data.sub_question.guess;
            }

            if (is_afk)
            {
                break;
            }

            // Check answer, if correct, add 200 points to current player

            if (conn_msg.data.sub_question.guess == conn_msg.data.sub_question.key)
            {
                game_state.player[game_state.turn].point += 200;
                sprintf(conn_msg.data.notification, "Correct answer! [%s] gained 200 points", game_state.player[game_state.turn].username);
                correct = 1;
            }
            else
            {
                game_state.player[game_state.turn].point = max(0, game_state.player[game_state.turn].point - 100);
                sprintf(conn_msg.data.notification, "Wrong answer! [%s] lost 100 points", game_state.player[game_state.turn].username);
                correct = 0;
            }

            // Send game notification to all clients

            conn_msg = make_conn_msg(NOTIFICATION, conn_msg.data);
            send_all(client_room, conn_msg);

            break;
        case -2:

            // Minus 150
            game_state.player[game_state.turn].point = max(0, game_state.player[game_state.turn].point - 150);

            sprintf(conn_msg.data.notification, "Unlucky! %s lost 150 points", game_state.player[game_state.turn].username);

            // Send game notification to all clients
            conn_msg = make_conn_msg(NOTIFICATION, conn_msg.data);
            send_all(client_room, conn_msg);

            // Current player's lost turn
            correct = 0;
            break;
        case -3:

            // Bonus 200
            game_state.player[game_state.turn].point += 200;
            sprintf(conn_msg.data.notification, "Lucky! %s gained 200 points", game_state.player[game_state.turn].username);

            // Send game notification to all clients
            conn_msg = make_conn_msg(NOTIFICATION, conn_msg.data);
            send_all(client_room, conn_msg);

            // Current player's lost turn
            correct = 0;
            break;
        default:

            // Send game state to all clients
            copy_game_state_type(&conn_msg.data.game_state, game_state);
            conn_msg = make_conn_msg(GAME_STATE, conn_msg.data);
            send_all(client_room, conn_msg);

            // Receive player's guess
            printf("[DEBUG] Waiting for guess from %d\n", client_room.connfd[game_state.turn]);

            // TRICK to handle bug:
            // Check guess_char is alphabet or not
            guess_char = '0';
            while (!isalpha(guess_char))
            {
                bytes_received = recv(client_room.connfd[game_state.turn], &conn_msg, sizeof(conn_msg), 0);
                // Handle AFK
                if ((is_afk = check_afk(bytes_received, &client_room, game_state.turn)))
                {

                    // Send afk notification to all clients
                    sprintf(conn_msg.data.notification, "[%s] is AFK", game_state.player[game_state.turn].username);
                    conn_msg = make_conn_msg(NOTIFICATION, conn_msg.data);
                    send_all(client_room, conn_msg);

                    // Skip this player's turn
                    correct = 0;
                    break;
                }

                guess_char = conn_msg.data.game_state.guess_char;
            }

            if (is_afk)
            {
                break;
            }
            printf("[DEBUG] Guess: %c\n", conn_msg.data.game_state.guess_char);

            correct = solve_crossword(&game_state, key, conn_msg.data.game_state.guess_char);
            printf("[DEBUG] Correct: %d\n", correct);

            // Send result to all clients
            sprintf(conn_msg.data.notification, "%s\n", game_state.game_message);
            conn_msg = make_conn_msg(NOTIFICATION, conn_msg.data);
            send_all(client_room, conn_msg);
            break;
        }

    }
    summary(&game_state);
    // Send game summary to all clients
    copy_game_state_type(&conn_msg.data.game_state, game_state);
    conn_msg = make_conn_msg(END_GAME, conn_msg.data);
    send_all(client_room, conn_msg);

    printf("Close thread\n");
    pthread_exit(NULL);
}

int specify_turn(int turn, client_room_type client_room)
{

    // Find next active player
    int current_turn = (turn + 1) % client_room.joined;

    while (client_room.status[current_turn] != 1 && current_turn != turn)
    {
        current_turn = (current_turn + 1) % client_room.joined;
    }

    return current_turn;
}

int is_all_afk(client_room_type client_room)
{

    // Check if all players are afk
    int i;
    for (i = 0; i < client_room.joined; i++)
    {
        if (client_room.status[i] == 1)
        {
            return 0;
        }
    }
    return 1;
}

void summary(game_state_type *game_state)
{
    // Write summary to game_state->game_message
    int i;
    char temp[100];
    int max_point = 0;
    
    // Track the number of winners
    int num_winners = 0;
    
    sprintf(game_state->game_message, "Summary:\n");
    
    for (i = 0; i < PLAYER_PER_ROOM; i++)
    {
        if (game_state->player[i].point > max_point)
        {
            max_point = game_state->player[i].point;
            num_winners = 1; // Reset the count for a new highest score
        }
        else if (game_state->player[i].point == max_point)
        {
            // Another player with the same highest score
            num_winners++;
        }

        sprintf(temp, "%s: %d points\n", game_state->player[i].username, game_state->player[i].point);
        strcat(game_state->game_message, temp);
    }
    
    // Check if there are multiple winners
    if (num_winners > 1)
    {
        strcat(game_state->game_message, "Winners: ");
    }
    else
    {
        strcat(game_state->game_message, "Winner: ");
    }

    // Track the number of winners added to the message
    int winners_added = 0;

    for (i = 0; i < PLAYER_PER_ROOM; i++)
    {
        if (game_state->player[i].point == max_point)
        {
            if (winners_added > 0)
            {
                strcat(game_state->game_message, ", ");
            }

            strcat(game_state->game_message, game_state->player[i].username);
            winners_added++;
        }
    }

    strcat(game_state->game_message, "\n");
}


int is_exist_username(waiting_room_type waiting_room, char *username)
{
    int i;

    // Check if username is exist
    for (i = 0; i < waiting_room.joined; i++)
    {
        if (strcmp(waiting_room.player[i].username, username) == 0)
        {
            printf("[DEBUG] Username %s is exist\n", username);
            return 1;
        }
    }
    return 0;
}