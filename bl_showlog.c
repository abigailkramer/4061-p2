// read the binary log file and print its output. 
// This should include the who_t at the beginning
// and all log messages.

#include "blather.h"

int main (int argc, char *argv[]) {

    if (argc < 2) {
        printf("usage: %s <log_name>\n",argv[0]);
        exit(1);
    }

    char log_name[MAXNAME];
    strncpy(log_name, argv[1], sizeof(argv[1]));

    // open log

    // maybe start w/ just reading the one who_t

    // then make a for/while loop to read through mesg_t structs?


    // close log

    return 0;
}