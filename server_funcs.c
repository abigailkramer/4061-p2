//server_funcs

#include "blather.h"

char sem_name[MAXPATH] = "";

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

    strncpy(server->server_name, server_name, sizeof(server_name));
    strncat(server->server_name, ".fifo", sizeof(".fifo"));
    server->n_clients = 0;
    server->join_ready = 0;

    remove(server->server_name);
    mkfifo(server->server_name, perms);     // using DEFAULT_PERMS from blather.h
    server->join_fd = open(server->server_name, perms);
    check_fail(server->join_fd==-1, 1, "Couldn't open file %s", server->server_name);

    // ADVANCED: create log and semaphore
    char log_name[MAXPATH] = "";
    strncpy(log_name, server_name, sizeof(server_name));
    strncat(log_name, ".log", sizeof(".log"));
    
    //char sem_name[MAXNAME];
    strncpy(sem_name, "/", 2);
    strncat(sem_name, server_name, sizeof(server_name));
    strncat(sem_name, ".sem", ".sem");

    server->log_fd = open(log_name, O_CREAT | O_RDWR , S_IRUSR | S_IWUSR);
    check_fail(server->log_fd==-1, 1, "Couldn't open file %s", log_name);
    server->log_sem = sem_open(sem_name, O_CREAT, S_IRUSR | S_IWUSR);
    
    sem_init(&server->log_sem, 1, 1);

    sem_wait(&server->log_sem);

    who_t who = {};
    who.n_clients = server->n_clients;
    lseek(server->log_fd, 0, SEEK_END);
    write(server->log_fd, &who, sizeof(who));
    
    sem_post(&server->log_sem);

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

    int fd = close(server->join_fd);
    check_fail(fd==-1, 1, "Couldn't close file %s", server->join_fd);
    remove(server->server_name);
    
    fd = close(server->log_fd);
    check_fail(fd==-1, 1, "Couldn't close file %s", server->log_fd);

    // send a BL_SHUTDOWN message to all clients
    mesg_t shutdown_actual = {};
    mesg_t *shutdown = &shutdown_actual;
    shutdown->kind = BL_SHUTDOWN;
    server_broadcast(server, shutdown);

    // proceed to remove all clients in any order
    for (int i = server->n_clients-1; i >= 0; i--) {
        server_remove_client(server,i);
    }
    
    sem_close(server->log_sem);
    sem_unlink(sem_name);

    log_printf("END: server_shutdown()\n");
    return;
}
// ADVANCED: Close the log file. Close the log semaphore and unlink it.

int server_add_client(server_t *server, join_t *join) {
    log_printf("BEGIN: server_add_client()\n");         // at beginning of function
    
    check_fail(server->n_clients >= MAXCLIENTS, 0, "Index out of bounds: %d vs max %d\n",server->n_clients,MAXCLIENTS);
    if (server->n_clients+1 >= MAXCLIENTS) {
    	log_printf("END: server_add_client()\n");           // at end of function
    	return 1;
    }

    int n = server->n_clients;
    strncpy(server->client[n].to_client_fname, join->to_client_fname, sizeof(join->to_client_fname));
    strncpy(server->client[n].to_server_fname, join->to_server_fname, sizeof(join->to_server_fname));
    strncpy(server->client[n].name, join->name, sizeof(join->name));

    int sfd = open(server->client[n].to_server_fname, O_RDONLY);
    check_fail(sfd==-1, 1, "Couldn't open file %s", server->client[n].to_server_fname);   
    server->client[n].to_server_fd = sfd;

    int cfd = open(server->client[n].to_client_fname, O_WRONLY);
    check_fail(cfd==-1, 1, "Couldn't open file %s", server->client[n].to_client_fname);    
    server->client[n].to_client_fd = cfd;
    server->client[n].data_ready = 0;
    
    server->n_clients++;    
        
    mesg_t message_actual = {};
    mesg_t *join_mesg = &message_actual;
    join_mesg->kind = BL_JOINED;
    strncpy(join_mesg->name, server->client[n].name, sizeof(server->client[n].name));
    server_broadcast(server, join_mesg);

    log_printf("END: server_add_client()\n");           // at end of function
    return 0;
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
    
    for (int i = idx+1; i < server->n_clients; i++) {
        server->client[i-1] = server->client[i];
    }
    server->n_clients--;

    return 0;
}


void server_broadcast(server_t *server, mesg_t *mesg) {
    dbg_printf("server_broadcast()\n");
    
    if (mesg->kind != BL_PING) {
        //write in log -- semaphore is for who_t part of log file
        lseek(server->log_fd, 0, SEEK_END);
        write(server->log_fd, mesg, sizeof(*mesg));
    }
    
    for (int i = 0; i < server->n_clients; i++) {
        write(server->client[i].to_client_fd, mesg, sizeof(*mesg));
    }
    dbg_printf("end server_broadcast()\n");
    return;
}
// ADVANCED: Log the broadcast message unless it is a PING which
// should not be written to the log.

void server_check_sources(server_t *server) {
    log_printf("BEGIN: server_check_sources()\n");             // at beginning of function
    int sources = server->n_clients+1;

    struct pollfd pfds[sources];
    pfds[0].fd = server->join_fd;
    pfds[0].events = POLLIN;
    for(int i = 1; i < sources; i++) {
        pfds[i].fd = server->client[i-1].to_server_fd;
        pfds[i].events = POLLIN;
    }

    log_printf("poll()'ing to check %d input sources\n", sources);  // prior to poll() call

    int ret = poll(pfds, sources, -1);

    log_printf("poll() completed with return value %d\n", ret);     // after poll() call
    
    if (ret == -1) {
        log_printf("poll() interrupted by a signal\n");             // if poll interrupted by a signal
        log_printf("END: server_check_sources()\n");
        // initiate server shutdown by returning immediately
        return;
    }

    if (pfds[0].revents && POLLIN) {
        server->join_ready = 1;
    }

    log_printf("join_ready = %d\n", server->join_ready);       // whether join queue has data

    
    for(int i = 1; i < sources; i++) {
        if( pfds[i].revents && POLLIN ) {
            server->client[i-1].data_ready = 1;
        }

        log_printf("client %d '%s' data_ready = %d\n", i-1, server->client[i-1].name, server->client[i-1].data_ready);    // whether client has data ready
    }


    log_printf("END: server_check_sources()\n");               // at end of function

    dbg_printf("Finished checking sources\n");
    return;
}


int server_join_ready(server_t *server) {
    return server->join_ready;
}


void server_handle_join(server_t *server) {
    if (!server_join_ready(server)) {
        return;
    }

    log_printf("BEGIN: server_handle_join()\n");               // at beginnning of function

    join_t join_actual = {};
    join_t *join = &join_actual;
    
    read(server->join_fd, join, sizeof(*join));
    log_printf("join request for new client '%s'\n", join->name);      // reports name of new client
    
    server_add_client(server, join);

    server->join_ready = 0;
    log_printf("END: server_handle_join()\n");                 // at end of function

    return;
}


int server_client_ready(server_t *server, int idx) {
    return server_get_client(server,idx)->data_ready;
}


void server_handle_client(server_t *server, int idx) {
    if (!server_client_ready) {
        return;
    }
    log_printf("BEGIN: server_handle_client()\n");           // at beginning of function

    // read mesg from to_server_fd
    mesg_t message_actual = {};
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
    // ADVANCED: update last_contact_time
    client->last_contact_time = server->time_sec;
    log_printf("END: server_handle_client()\n");             // at end of function 
    return;
}


void server_tick(server_t *server) {
    server->time_sec++;
    return;
}


void server_ping_clients(server_t *server) {
    mesg_t ping_actual = {};
    mesg_t *ping = &ping_actual;
    ping->kind = BL_PING;
    server_broadcast(server, ping);
    return;
}


void server_remove_disconnected(server_t *server, int disconnect_secs) {

    for (int i = 0; i < server->n_clients; i++) {
    	int time = server->time_sec - server->client[i].last_contact_time;
    	if (time >= disconnect_secs) {
    	    server_remove_client(server,i);
    	    
    	    // broadcast disconnect message
    	    mesg_t disconnect_actual = {};
    	    mesg_t *disconnect = &disconnect_actual;
    	    disconnect->kind = BL_DISCONNECTED;
    	    strncpy(disconnect->name, server->client[i].name, sizeof(server->client[i].name));
    	    server_broadcast(server, disconnect);
    	    
    	    i--;
    	}    
    }

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
    // lock semaphore
    
    // complete write to log file in its own thread
    // consider pwrite() to write to a specific location
    // in an open file descriptor -- which won't alter
    // the position of log_fd so that appends continue
    // to the end of the file

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

    write(server->log_fd, mesg, sizeof(*mesg));			// probs needs more

    return;
}
// ADVANCED: Write the given message to the end of log file associated
// with the server.
