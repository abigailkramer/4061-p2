//server_funcs

#include "blather.h"

client_t *server_get_client(server_t *server, int idx) {
    if (idx > server->n_clients) {
        return NULL;
    } else {
        client_t *client = &server->client[idx];
        return client;
    }
}


void server_start(server_t *server, char *server_name, int perms) {
    log_printf("BEGIN: server_start()\n");              // at beginning of function

    strcpy(server->server_name, server_name);
    server->n_clients = 0;
    server->join_ready = 0;

    remove(server_name);
    mkfifo(server_name, 0666);
    
    server->join_fd = open(server_name, perms);

    log_printf("END: server_start()\n");                // at end of function
    return;
}
// ADVANCED: create the log file "server_name.log" and write the
// initial empty who_t contents to its beginning. Ensure that the
// log_fd is position for appending to the end of the file. Create the
// POSIX semaphore "/server_name.sem" and initialize it to 1 to
// control access to the who_t portion of the log.

void server_shutdown(server_t *server) {
    log_printf("BEGIN: server_shutdown()\n");

    close(server->join_fd);
    remove(server->server_name);
    unlink(server->server_name);

    // unlink (remove) - so no further clients can join
    // send a BL_SHUTDOWN message to all clients
    mesg_t shutdown_actual;
    mesg_t *shutdown = &shutdown_actual;
    shutdown->kind = BL_SHUTDOWN;
    server_broadcast(server, shutdown);

    // proceed to remove all clients in any order
    for (int i = server->n_clients-1; i >= 0; i--) {
        server_remove_client(server,i);
    }

    log_printf("END: server_shutdown()\n");
    return;
}
// ADVANCED: Close the log file. Close the log semaphore and unlink it.

int server_add_client(server_t *server, join_t *join) {
    log_printf("BEGIN: server_add_client()\n");         // at beginning of function
    int success = 0;
    
    if (server->n_clients >= MAXCLIENTS) {
    	log_printf("too many clients\n");
    	success = 1;
    }

    success = 0;
    int n = server->n_clients;
    strcpy(server->client[n].to_client_fname, join->to_client_fname);
    strcpy(server->client[n].to_server_fname, join->to_server_fname);
    strcpy(server->client[n].name, join->name);

    int sfd = open(server->client[n].to_server_fname, O_RDONLY);
    server->client[n].to_server_fd = sfd;

    int cfd = open(server->client[n].to_client_fname, O_WRONLY);
    server->client[n].to_client_fd = cfd;
    server->client[n].data_ready = 0;
    
    server->n_clients++;    
        
    mesg_t message_actual;
    mesg_t *join_mesg = &message_actual;
    join_mesg->kind = BL_JOINED;
    strcpy(join_mesg->name, server->client[n].name);
    server_broadcast(server, join_mesg);        

    log_printf("END: server_add_client()\n");           // at end of function
    return success;
}


int server_remove_client(server_t *server, int idx) {
    client_t *client = server_get_client(server, idx);
    if (client == NULL) {
        return 1;
    }
    
    // close fifos and stuff
    close(client->to_client_fd);
    close(client->to_server_fd);
    remove(client->to_client_fname);
    remove(client->to_server_fname);
    unlink(client->to_client_fname);
    unlink(client->to_server_fname);
    
    for (int i = idx+1; i < server->n_clients; i++) {
        server->client[i-1] = server->client[i];        // def may not be right
    }
    server->n_clients--;

    return 0;
}


void server_broadcast(server_t *server, mesg_t *mesg) {
    for (int i = 0; i < server->n_clients; i++) {
        write(server->client[i].to_client_fd, mesg, sizeof(*mesg));
    }
    return;
}
// ADVANCED: Log the broadcast message unless it is a PING which
// should not be written to the log.

void server_check_sources(server_t *server) {
    log_printf("BEGIN: server_check_sources()\n");             // at beginning of function
    int sources = server->n_clients+1;

    struct pollfd pfds[sources];
    for(int i = 0; i < server->n_clients; i++) {
        pfds[i].fd = server->client[i].to_server_fd;
        pfds[i].events = POLLIN;
    }
    pfds[sources-1].fd = server->join_fd;
    pfds[sources-1].events = POLLIN;


    log_printf("poll()'ing to check %d input sources\n", sources);  // prior to poll() call

    int ret = poll(pfds, sources, -1);

    log_printf("poll() completed with return value %d\n", ret); // after poll() call
    
    if (ret == -1) {
        log_printf("poll() interrupted by a signal\n");            // if poll interrupted by a signal
        
        // initiate server shutdown
        server_shutdown(server);
        return;
    }
    
    for(int i = 0; i < server->n_clients; i++) {
        if( pfds[i].revents && POLLIN ) {
            server->client[i].data_ready = 1;
            log_printf("client %d '%s' data_ready = %d\n", i, server->client[i].name, 1);    // whether client has data ready
            //server_handle_client(server,i);
        }
    }

    if (pfds[sources-1].revents && POLLIN) {
        server->join_ready = 1;
        log_printf("join_ready = %d\n", server->join_ready);                       // whether join queue has data
        server_handle_join(server);
    }
    

    log_printf("END: server_check_sources()\n");               // at end of function
    return;
}
// Checks all sources of data for the server to determine if any are
// ready for reading. Sets the servers join_ready flag and the
// data_ready flags of each of client if data is ready for them.
// Makes use of the poll() system call to efficiently determine which
// sources are ready.
// 
// NOTE: the poll() system call will return -1 if it is interrupted by
// the process receiving a signal. This is expected to initiate server
// shutdown and is handled by returning immediately from this function.


int server_join_ready(server_t *server) {
    return server->join_ready;
}
// Return the join_ready flag from the server which indicates whether
// a call to server_handle_join() is safe.

void server_handle_join(server_t *server) {
    if (!server_join_ready(server)) {
        return;
    }

    log_printf("BEGIN: server_handle_join()\n");               // at beginnning of function

    join_t join_actual;
    join_t *join = &join_actual;
    
    read(server->join_fd, join, sizeof(*join));
    log_printf("join request for new client '%s'\n", join->name);      // reports name of new client
    
    server_add_client(server, join);

    server->join_ready = 0;
    log_printf("END: server_handle_join()\n");                 // at end of function

    return;
}
// Call this function only if server_join_ready() returns true. Read a
// join request and add the new client to the server. After finishing,
// set the servers join_ready flag to 0.


int server_client_ready(server_t *server, int idx) {
    return server_get_client(server,idx)->data_ready;
}

void server_handle_client(server_t *server, int idx) {
    if (!server_client_ready) {
        return;
    }
    log_printf("BEGIN: server_handle_client()\n");           // at beginning of function

    // read mesg from to_server_fd
    mesg_t message_actual;
    mesg_t *mesg = &message_actual;

    client_t *client = server_get_client(server, idx);
    read(client->to_server_fd, mesg, sizeof(*mesg));

    // analyze message kind
    if (mesg->kind == BL_MESG) {
        log_printf("client %d '%s' MESSAGE '%s'\n",idx,mesg->name,mesg->body); // indicates client message
        server_broadcast(server, mesg);
    } else if (mesg->kind == BL_DEPARTED) {
    	server_remove_client(server,idx);
        log_printf("client %d '%s' DEPARTED\n",idx,mesg->name);     // indicates client departed
        server_broadcast(server, mesg);
    }

    client->data_ready = 0;
    log_printf("END: server_handle_client()\n");             // at end of function 
    return;
}
// ADVANCED: Update the last_contact_time of the client to the current
// server time_sec.


void server_tick(server_t *server) {
    server->time_sec++;                     // this seems too easy lol?
    return;
}
// ADVANCED: Increment the time for the server


void server_ping_clients(server_t *server) {
    return;
}
// ADVANCED: Ping all clients in the server by broadcasting a ping.


void server_remove_disconnected(server_t *server, int disconnect_secs) {
    return;
}
// ADVANCED: Check all clients to see if they have contacted the
// server recently. Any client with a last_contact_time field equal to
// or greater than the parameter disconnect_secs should be
// removed. Broadcast that the client was disconnected to remaining
// clients.  Process clients from lowest to highest and take care of
// loop indexing as clients may be removed during the loop
// necessitating index adjustments.


void server_write_who(server_t *server) {
    return;
}
// ADVANCED: Write the current set of clients logged into the server
// to the BEGINNING the log_fd. Ensure that the write is protected by
// locking the semaphore associated with the log file. Since it may
// take some time to complete this operation (acquire semaphore then
// write) it should likely be done in its own thread to preven the
// main server operations from stalling.  For threaded I/O, consider
// using the pwrite() function to write to a specific location in an
// open file descriptor which will not alter the position of log_fd so
// that appends continue to write to the end of the file.


void server_log_message(server_t *server, mesg_t *mesg) {
    return;
}
// ADVANCED: Write the given message to the end of log file associated
// with the server.
