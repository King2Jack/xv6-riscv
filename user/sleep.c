//
// Created by KingJack on 2024/7/22.
//

#include "kernel/types.h"
#include "user/user.h"

// sleep for n
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(2, "Usage: sleep <number>\n");
        exit(1);
    }
    sleep(atoi(argv[1]));
    fprintf(1, "Sleep for %s seconds\n", argv[1]);
    exit(0);
}

