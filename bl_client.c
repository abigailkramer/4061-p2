//bl_client

#include "blather.h"

pthread_t client_thread;
pthread_t server_thread;

simpio_t simpio_actual;
simpio_t *simpio = &simpio_actual;

client_t client_actual;
client_t *client = &client_actual;

void *client_worker(void *arg) {    
    while (!simpio->end_of_input) {
        simpio_reset(simpio);
        iprintf(simpio, "");
        while (!simpio->line_ready && !simpio->end_of_input) {
            simpio_get_char(simpio);
        }
        if (simpio->line_ready) {
            // format message
            mesg_t usr_mesg_actual;
            mesg_t *usr_mesg = &usr_mesg_actual;
            strncpy(usr_mesg->name, client->name, sizeof(client->name));
            strncpy(usr_mesg->body, simpio->buf, sizeof(simpio->buf));
            usr_mesg->kind = BL_MESG;
            
            // write to the to-server fifo
            write(client->to_server_fd, usr_mesg, sizeof(*usr_mesg));
        }
    }
    iprintf(simpio, "End of Input, Departing\n");
    
    // write departed message to server
    mesg_t depart_mesg_actual;
    mesg_t *depart_mesg = &depart_mesg_actual;
    strncpy(depart_mesg->name, client->name, sizeof(client->name));
    depart_mesg->kind = BL_DEPARTED;
    write(client->to_server_fd, depart_mesg, sizeof(*depart_mesg));
    
    pthread_cancel(server_thread); // kill the server thread
    return NULL;
}

void *server_worker(void *arg) {
    //int nread;
    while(1) {
        mesg_t message_actual;
        mesg_t *message = &message_actual;
        int nread = read(client->to_client_fd, message, sizeof(*message));
        
        if (nread != sizeof(*message)) {
            continue;
        }

        if (message->kind == BL_SHUTDOWN) {
            break;
        }
        if (message->kind == BL_MESG) {
            iprintf(simpio, "[%s] : %s\n", message->name, message->body);
        } else if (message->kind == BL_JOINED) {
            iprintf(simpio, "-- %s JOINED --\n", message->name);
        } else if (message->kind == BL_DEPARTED) {
            iprintf(simpio, "-- %s DEPARTED --\n", message->name);
        }
    }
    iprintf(simpio, "== SERVER SHUTDOWN ==\n");
    pthread_cancel(client_thread);  // kill the client thread
    return NULL;
}

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0);                     // Turn off output buffering

    if (argc < 3) {
        printf("usage: %s <server_name> <client_name>\n",argv[0]);
        exit(1);
    }

    char server_name[MAXNAME];
    strncpy(server_name, argv[1], sizeof(argv[1]));
    strcat(server_name, ".fifo");
    char client_name[MAXNAME];
    strncpy(client_name, argv[2], sizeof(argv[2]));

    // create to-client and to-server fifos
    strncpy(client->name, client_name, sizeof(client_name));
    sprintf(client->to_client_fname, "%d_to_client.fifo", getpid());
    sprintf(client->to_server_fname, "%d_to_server.fifo", getpid());

    // create to server and to client fifos
    mkfifo(client->to_client_fname, 0666);
    mkfifo(client->to_server_fname, 0666);

    client->to_client_fd = open(client->to_client_fname, O_RDWR);
    check_fail(client->to_client_fd==-1, 1, "Couldn't open file %s", client->to_client_fname);
    
    client->to_server_fd = open(client->to_server_fname, O_RDWR);
    check_fail(client->to_server_fd==-1, 1, "Couldn't open file %s", client->to_server_fname);    
    
    // write join request to server fifo
    join_t join_actual;
    join_t *join = &join_actual;
    strncpy(join->name, client->name, sizeof(client->name));
    strncpy(join->to_client_fname, client->to_client_fname, sizeof(client->to_client_fname));
    strncpy(join->to_server_fname, client->to_server_fname, sizeof(client->to_server_fname));

    int fd = open(server_name, O_WRONLY);
    check_fail(fd==-1, 1, "Couldn't open file %s", server_name);
    
    write(fd, join, sizeof(*join));
    close(fd);    // client won't need join_fd anymore
    unlink(server_name);

    char prompt[MAXNAME+3];
    snprintf(prompt, MAXNAME+3, "%s>> ",client_name); // create a prompt string
    simpio_set_prompt(simpio, prompt);         // set the prompt
    simpio_reset(simpio);                      // initialize io
    simpio_noncanonical_terminal_mode();       // set the terminal into a compatible mode

    pthread_create(&client_thread, NULL, client_worker, NULL);
    pthread_create(&server_thread, NULL, server_worker, NULL);
    pthread_join(client_thread, NULL);
    pthread_join(server_thread, NULL);
  
    simpio_reset_terminal_mode();
    printf("\n");
    
    close(client->to_client_fd);
    close(client->to_server_fd);
    remove(client->to_client_fname);
    remove(client->to_server_fname);
    unlink(client->to_client_fname);
    unlink(client->to_server_fname);
    return 0;
}