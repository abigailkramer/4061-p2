// read the binary log file and print its output. 
// This should include the who_t at the beginning
// and all log messages.

#include "blather.h"

int main (int argc, char *argv[]) {

    if (argc < 2) {
        printf("usage: %s <log_name>\n",argv[0]);
        exit(1);
    }

    char log_name[MAXPATH];
    strncpy(log_name, argv[1], sizeof(argv[1]));        // should already be in .log form

    // open log
    int fd = open(log_name, O_RDONLY);
    check_fail(fd==-1, 1, "Couldn't open file %s", log_name);

    // read the one who_t struct at the beginning
    who_t who;
    read(fd, &who, sizeof(who));
    printf("%d CLIENTS\n", who.n_clients);
    for (int i = 0; i < who.n_clients; i++) {
        printf("%d : %s\n", i, who.names[i]);
    }

    // read through the mesg_t structs until the end of the file
    printf("MESSAGES\n");
    while(1) {
        mesg_t message;
        int nread = read(fd, &message, sizeof(message));

        // nothing else to read
        if (nread != sizeof(message)) {
            break;
        }

        if (message.kind == BL_SHUTDOWN) {
            printf("!!! server is shutting down !!!\n");
        } else if (message.kind == BL_MESG) {
            printf("[%s] : %s\n", message.name, message.body);
        } else if (message.kind == BL_JOINED) {
            printf("-- %s JOINED --\n", message.name);
        } else if (message.kind == BL_DEPARTED) {
            printf("-- %s DEPARTED --\n", message.name);
        } else if (message.kind == BL_DISCONNECTED) {
            printf("-- %s DISCONNECTED --\n", message.name);
        }
        // don't need to check for BL_PING - not written to log files

    }

    // close log
    int check = close(fd);
    check_fail(check==-1, 1, "Couldn't close file %s", fd);

    return 0;
}