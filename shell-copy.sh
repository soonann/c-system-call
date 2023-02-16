#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/******************************************************************************
 * Types
 ******************************************************************************/

typedef enum process_status {
  UNUSED = -1,
  RUNNING = 0,
  READY = 1,
  STOPPED = 2,
  TERMINATED = 3,
} process_status;

typedef struct process_record {
  pid_t pid;
  process_status status;
} process_record;

/******************************************************************************
 * Globals
 ******************************************************************************/

enum { MAX_PROCESSES = 64 };

process_record process_records[MAX_PROCESSES];

void perform_run(char *args[]) {
  int index = -1;
  for (int i = 0; i < MAX_PROCESSES; ++i) {
    if (process_records[i].status == UNUSED) {
      index = i;
      break;
    }
  }
  if (index < 0) {
    printf("no unused process slots available; searching for an entry of a "
           "killed process.\n");
    for (int i = 0; i < MAX_PROCESSES; ++i) {
      if (process_records[i].status == TERMINATED) {
        index = i;
        break;
      }
    }
  }
  if (index < 0) {
    printf("no process slots available.\n");
    return;
  }
  pid_t pid = fork();
  if (pid < 0) {
    fprintf(stderr, "fork failed\n");
    return;
  }

  // child process
  if (pid == 0) {
    const int len = strlen(args[0]);
    char exec[len + 3];
    strcpy(exec, "./");
    strcat(exec, args[0]);

    // immediately sigstop before it can execute
    // bg handler will schedule this to be run
    raise(SIGSTOP);
    execvp(exec, args);
    // Unreachable code unless execution failed.
    exit(EXIT_FAILURE);
  }
  // add the record to the list of processes
  process_record *const p = &process_records[index];
  p->pid = pid;
  p->status = RUNNING;
  printf("[%d] %d created\n", index, p->pid);
}

// kill the specified process
void perform_kill(char *args[]) {
  const pid_t pid = atoi(args[0]);

  if (pid <= 0) {
    printf("The process ID must be a positive integer.\n");
    return;
  }

  // loop through the processes and look for the one with specified pid
  for (int i = 0; i < MAX_PROCESSES; ++i) {
    process_record *const p = &process_records[i];
    if ((p->pid == pid) && (p->status == RUNNING)) {
      kill(p->pid, SIGTERM);
      printf("[%d] %d killed\n", i, p->pid);
      p->status = TERMINATED;
      return;
    }
  }
  printf("Process %d not found.\n", pid);
}

void perform_list(void) {
  // loop through all child processes, display status
  bool anything = false;
  for (int i = 0; i < MAX_PROCESSES; ++i) {
    process_record *const p = &process_records[i];
    switch (p->status) {
    case RUNNING:
      printf("[%d] %d created\n", i, p->pid);
      anything = true;
      break;
    case TERMINATED:
      printf("[%d] %d killed\n", i, p->pid);
      anything = true;
      break;
    case UNUSED:
      // Do nothing.
      break;
    }
  }
  // if there isnt anything in the list, print no processes to list
  if (!anything) {
    printf("No processes to list.\n");
  }
}

void perform_exit(void) { printf("bye!\n"); }

// get input from terminal
char *get_input(char *buffer, char *args[], int args_count_max) {
  // capture a command
  printf("cs205> ");
  fgets(buffer, 79, stdin);
  for (char *c = buffer; *c != '\0'; ++c) {
    if ((*c == '\r') || (*c == '\n')) {
      *c = '\0';
      break;
    }
  }
  strcat(buffer, " ");
  // tokenize command's arguments
  char *p = strtok(buffer, " ");
  int arg_cnt = 0;
  while (p != NULL) {
    args[arg_cnt++] = p;
    if (arg_cnt == args_count_max - 1) {
      break;
    }
    p = strtok(NULL, " ");
  }
  args[arg_cnt] = NULL;
  return args[0];
}

/******************************************************************************
 * Entry point
 ******************************************************************************/

int main(void) {
  char buffer[80];
  // NULL-terminated array
  char *args[10];
  const int args_count = sizeof(args) / sizeof(*args);
  while (true) {
    char *const cmd = get_input(buffer, args, args_count);
    if (strcmp(cmd, "kill") == 0) {
      perform_kill(&args[1]);
    } else if (strcmp(cmd, "run") == 0) {
      perform_run(&args[1]);
    } else if (strcmp(cmd, "list") == 0) {
      perform_list();
    } else if (strcmp(cmd, "exit") == 0) {
      perform_exit();
      break;
    } else {
      printf("invalid command\n");
    }
  }
  return EXIT_SUCCESS;
}
