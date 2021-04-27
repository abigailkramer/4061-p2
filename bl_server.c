//bl_server

#include "blather.h"

int DO_ADVANCED = 0;

// track when SIGINT or SIGTERM have been received
int SHUTDOWN = 0;
void shutdown_handler(int signum) {
    SHUTDOWN = 1;
}

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0);                     // Turn off output buffering

    if (argc < 2) {
        printf("usage: %s <server_name>\n",argv[0]);
        exit(1);
    }
    
    if (getenv("BL_ADVANCED")) {
    	DO_ADVANCED = 1;
    }

    char server_name[MAXNAME];
    strncpy(server_name, argv[1], sizeof(argv[1]));

    struct sigaction my_sa = {};
    my_sa.sa_handler = shutdown_handler;

    // set signal handler for SIGTERM and SIGINT
    sigaction(SIGTERM, &my_sa, NULL);
    sigaction(SIGINT, &my_sa, NULL);

    server_t server_actual;
    server_t *server = &server_actual;
    server_start(server, server_name, O_RDWR);

    while(!SHUTDOWN) {
        server_check_sources(server);
        
        // ADVANCED
        if (DO_ADVANCED) {
            server_tick(server);
            server_remove_disconnected(server, 10);	// 10 is placeholder disconnect_secs    
        }
        
        for (int i = 0; i < server->n_clients; i++) {
            if (server->client[i].data_ready) {
                server_handle_client(server,i);
            }
        }
    }

    server_shutdown(server);
    return 0;

}
