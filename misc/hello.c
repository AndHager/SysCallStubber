#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

void func() {
    long a[85];
    int j = 0;

    /*Process identification*/
    a[j++] = getuid();
    a[j++] = getgid();
    // a[j++] = getsid();

    a[j++] = setuid(123);
    a[j++] = setgid(456);
    // a[j++] = setsid();
    
    // a[j++] = getpgid();
    a[j++] = getpid();
    a[j++] = getppid();
    a[j++] = getpgrp();

    a[j++] = setpgid(123, 456);
    // a[j++] = setreuid();
    // a[j++] = setresuid();
    // a[j++] = setregid();
    // a[j++] = setresgid();

    // a[j++] = getresuid();
    // a[j++] = getresgid();

    // a[j++] = setgroups();
    // a[j++] = getgroups(0, a);
    // a[j++] = setfsuid();
    // a[j++] = setfsgid();
    // a[j++] = gettid();
    for(int i = 0; i < j; i++)
        printf("\t %d. HI %ld\n", i, a[i]);
}

int main() {
    char *path = "test.log";
    
    int i = -1;
    char str[22] = "                     ";
    sprintf(str, "%d", i);
    printf("hello world, %s, %s\n", str, path);
    int fd = -1;
    while ((fd = open(path, O_WRONLY | O_CREAT)) < 0 && errno == EINTR) {
        printf("__ERROR__: FILE: %s, LINE: %d, RETURN_CODE: %s;\n\tOpen file %s was interrupted!\n", __FILE__, __LINE__, strerror(fd), path);
    }
    if (fd < 0) {
        printf("__ERROR__: FILE: %s, LINE: %d, RETURN_CODE: %s;\n\tCould not open file %s!\n", __FILE__, __LINE__, strerror(fd), path);
        exit(EXIT_FAILURE);
    }  
    int s = -1;
    while ((s = write(fd, "Test Write File!", 17)) < 0 && errno == EINTR) {
        printf("__ERROR__: FILE: %s, LINE: %d, RETURN_CODE: %s;\n\tWrite file %s was interrupted!\n", __FILE__, __LINE__, strerror(s), path);
    }
    if (s < 0) {
        printf("__ERROR__: FILE: %s, LINE: %d, RETURN_CODE: %s;\n\tCould not write file %s!\n", __FILE__, __LINE__, strerror(s), path);
        exit(EXIT_FAILURE);
    }

    int c = -1;
    while ((c = close(fd)) < 0 && errno == EINTR) {
        printf("__ERROR__: FILE: %s, LINE: %d, RETURN_CODE: %s;\n\tClose file %s was interrupted!\n", __FILE__, __LINE__, strerror(c), path);
    }
    if (c < 0) {
        printf("__ERROR__: FILE: %s, LINE: %d, RETURN_CODE: %s;\n\tCould not close file %s!\n", __FILE__, __LINE__, strerror(c), path);
        exit(EXIT_FAILURE);
    }
    sleep(1);
    func();
    return EXIT_SUCCESS;
}

