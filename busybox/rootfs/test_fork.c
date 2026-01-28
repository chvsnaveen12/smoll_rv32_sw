#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    printf("Parent PID: %d\n", getpid());
    fflush(stdout);
    
    pid_t pid = fork();
    
    if (pid < 0) {
        printf("Fork FAILED with %d\n", pid);
    } else if (pid == 0) {
        printf("Child running! PID: %d\n", getpid());
        fflush(stdout);
        _exit(0);
    } else {
        printf("Parent: child PID is %d\n", pid);
        int status;
        waitpid(pid, &status, 0);
        printf("Child exited with status %d\n", status);
    }
    
    fflush(stdout);
    while(1) sleep(1);
    return 0;
}
