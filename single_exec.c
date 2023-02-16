#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void) {
  pid_t pid = fork();
  int status;
  if (pid < 0) {
    fprintf(stderr, "fork failed\n");
    return EXIT_FAILURE;
  }
  if (pid == 0) {
    char *const exec = strdup("./prog");
    char *const arg1 = strdup("a");
    char *const arg2 = strdup("2");
    char *const arg_list[] = {exec, arg1, arg2, NULL};
    execvp(arg_list[0], arg_list);
    // Unreachable code unless execution failed.
    free(arg1);
    free(exec);
    return EXIT_FAILURE;
  }
  while (waitpid(pid, &status, WNOHANG) == pid) {
    printf("parent exited %d\n", pid);
  }

  return EXIT_SUCCESS;
}
