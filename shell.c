#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/******************************************************************************
 * Configs
 ******************************************************************************/

enum {
  MAX_PROCESSES = 64, // max number of processes
  MAX_RUNTIME = 5,
};

/******************************************************************************
 * Types and Enums
 ******************************************************************************/

typedef enum process_status {
  RUNNING = 0,
  READY = 1,
  STOPPED = 2,
  TERMINATED = 3,
} process_status;

typedef struct process_record {
  pid_t pid;
  process_status status;
} process_record;

// queue structure that stores the process to be executed in order
typedef struct process_queue {
  int _process_queue[MAX_PROCESSES];
  int headIndex;
  int tailIndex;
  int length;
} process_queue;

void init_process_queue(struct process_queue *process_queue) {
  process_queue->headIndex = 0;
  process_queue->tailIndex = 0;
  process_queue->length = 0;
}
// returns the pIndex if the element was enqueued
// otherwise returns -1 if queue is full
int enqueue(struct process_queue *process_queue, int pIndex) {
  if (process_queue->length >= 64) {
    return -1;
  }
  process_queue->_process_queue[process_queue->tailIndex] = pIndex;
  process_queue->tailIndex++;
  process_queue->length++;

  if (process_queue->tailIndex > MAX_PROCESSES - 1) {
    process_queue->tailIndex = 0;
  }

  return pIndex;
}

// returns -1 if queue is empty
// otherwise returns the pIndex at the head of the queue
int dequeue(struct process_queue *process_queue) {
  // if head == tail, it means there are no elements
  if (process_queue->length <= 0) {
    return -1;
  }
  int pIndex = process_queue->_process_queue[process_queue->headIndex];
  process_queue->headIndex++;
  process_queue->length--;
  if (process_queue->headIndex > MAX_PROCESSES - 1) {
    process_queue->headIndex = 1;
  }
  return pIndex;
}
/******************************************************************************
 * Global Variables
 ******************************************************************************/

static process_record *proc_records;
static process_queue *proc_queue;
static int *proc_record_count;

/******************************************************************************
 * Functions
 ******************************************************************************/

// running the given process
void perform_run(char *args[]) {
  int index = *proc_record_count;

  // if there isnt any process space that is unused, look for terminated ones
  if (index >= 64) {
    for (int i = 0; i < MAX_PROCESSES; ++i) {
      if (proc_records[i].status == TERMINATED) {
        index = i;
        break;
      }
    }
  }

  // completely no process slots available
  if (index >= 64) {
    printf("no process slots available.\n");
    return;
  }

  // managed to find a process space to fork
  pid_t pid_dummy = fork();
  int status;

  // dummy fork
  if (pid_dummy == 0) {

    // actual exec fork
    pid_t pid_exec = fork();
    if (pid_exec == 0) {

      // immediately SIGSTOP before it can execute
      // bg handler will schedule this to be run
      raise(SIGSTOP);
      execvp(args[0], args);

      // error handling
      printf("run command failed\n");
      exit(EXIT_FAILURE);
    }

    // exec fork - error handling
    if (pid_exec < 0) {
      fprintf(stderr, "fork failed\n");
      return;
    }

    // exec fork parent aka dummy fork's child process
    if (pid_exec > 0) {
      // add the record to the list of processes
      process_record *const p = &(proc_records[index]);
      p->pid = pid_exec;
      p->status = READY;

      // add the process to the process queue's end
      enqueue(proc_queue, index);
      *proc_record_count = *proc_record_count + 1;
      exit(EXIT_SUCCESS);
    }
  }

  // dummy fork - error handling
  if (pid_dummy < 0) {
    fprintf(stderr, "fork failed\n");
    return;
  }

  // wait for dummy fork to return
  while (waitpid(pid_dummy, &status, WNOHANG) == pid_dummy) {
  }
}

// kill the specified process
void perform_action(char *args[], int SIGNAL) {
  const pid_t pid = atoi(args[0]);

  if (pid <= 0) {
    printf("The process ID must be a positive integer.\n");
    return;
  }

  // loop through the processes and look for the one with specified pid
  for (int i = 0; i < MAX_PROCESSES; i++) {
    process_record *const p = &(proc_records[i]);
    // find the process matching the id
    if (p->pid == pid) {
      switch (SIGNAL) {
      case SIGCONT:
        // will not actually send SIGCONT as it the background handler will
        // handle it
        printf("resuming %d\n", p->pid);
        p->status = READY;
        enqueue(proc_queue, i);
        break;
      case SIGSTOP:
        printf("stopping %d\n", p->pid);
        p->status = STOPPED;
        kill(p->pid, SIGNAL);
        break;
      case SIGTERM:
        printf("terminating %d\n", p->pid);
        p->status = TERMINATED;
        kill(p->pid, SIGNAL);
        break;
      }
      return;
    }
  }
  printf("Process %d not found.\n", pid);
}

void perform_list(void) {
  // loop through all child processes, display status
  bool anything = false;
  for (int i = 0; i < *proc_record_count; ++i) {
    process_record *const p = &(proc_records[i]);
    printf("%d,%d \n", p->pid, p->status);
    anything = true;
  }

  // if there isnt anything in the list, print no processes to list
  if (!anything) {
    printf("No processes to list.\n");
  }
}

void perform_exit(void) { printf("exiting shell ...\n"); }

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

  proc_record_count = mmap(NULL, sizeof(int *), PROT_READ | PROT_WRITE,
                           MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  proc_records =
      mmap(NULL, sizeof(struct process_record) * MAX_PROCESSES,
           PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  proc_queue = mmap(NULL, sizeof(struct process_queue), PROT_READ | PROT_WRITE,
                    MAP_SHARED | MAP_ANONYMOUS, -1, 0);

  // init proc_queue
  init_process_queue(proc_queue);

  // init proc_record_count
  *proc_record_count = 0;

  // start background handler
  pid_t bg_handler_pid = fork();

  // bg handler process
  if (bg_handler_pid == 0) {

    int bgHandlerAlive = 1;
    int current_process_index = -1;

    // bg handler loop
    while (bgHandlerAlive) {

      // no process to handle currenly
      if (current_process_index == -1) {
        // keep trying to dequeue to see if there is a new process to run
        current_process_index = dequeue(proc_queue);
        process_record *const p = &(proc_records[current_process_index]);
        // if the process i've just popped is not ready, remove it from the
        // process queue entirely
        if (p->status != READY) {
          current_process_index = -1;
        }
      }

      // there is a process currently active, handle it
      // current_process_index != -1
      else {

        // current process we're inspecting
        process_record *const p = &(proc_records[current_process_index]);

        // resume the process by using SIGCONT and set it to RUNNING
        kill(p->pid, SIGCONT);
        p->status = RUNNING;

        // the two different pids we're going to be waiting for
        pid_t curr_proc_pid = p->pid;
        pid_t time_process_pid = fork();

        // child timer process
        if (time_process_pid == 0) {
          sleep(MAX_RUNTIME);
          exit(EXIT_SUCCESS);
        }

        // bg handler process
        bool has_timer_or_current_responded = false;
        int timer_status;
        int curr_proc_status;

        // wait for either the timer to return or the current process to return
        while (!has_timer_or_current_responded) {

          // process responds first, means it has completed and ran less
          // than 5 seconds and has ended by itself timer responds first,
          // means 5 seconds has run out
          if (waitpid(time_process_pid, &timer_status, WNOHANG) ==
              time_process_pid) {
            // SIGSTOP the process
            kill(curr_proc_pid, SIGSTOP);

            // enqueue it back to the end of the process queue to be run again
            // later
            enqueue(proc_queue, current_process_index);

            // update process listing detail
            p->status = READY;

            current_process_index = -1;
            has_timer_or_current_responded = true;
          }

          // using kill to check if the process has ended, cant use waitpid
          // because the process started is not the bg handler's child
          else if (kill(curr_proc_pid, 0) == -1) {

            // kill the process in case it hasnt been cleaned up yet
            kill(curr_proc_pid, SIGTERM);
            kill(time_process_pid, SIGTERM);

            // update process listing detail
            p->status = TERMINATED;

            // reset current process to be -1 to pop a new process
            current_process_index = -1;
            has_timer_or_current_responded = true;
          }
          // if process was manually terminated
          else if (p->status != RUNNING) {

            // kill the process in case it hasnt been cleaned up yet
            kill(time_process_pid, SIGTERM);

            // depending on what is the new status, send the signal
            switch (p->status) {
            case STOPPED:
              kill(curr_proc_pid, SIGSTOP);
              break;
            case TERMINATED:
              kill(curr_proc_pid, SIGTERM);
              break;
            default:
              break;
            }

            // reset current process to be -1 to pop a new process
            current_process_index = -1;
            has_timer_or_current_responded = true;
          }
        }
      }
    }
    printf("bg handler exited\n");
  }

  // create a buffer to store the string inputs
  char buffer[80];

  // NULL-terminated array
  char *args[10];

  // number of arguments
  const int args_count = sizeof(args) / sizeof(*args);

  // keep prompting until the user exits
  while (true) {
    char *const cmd = get_input(buffer, args, args_count);
    if (cmd == NULL) {
      // do nothing and let the enter show the newline
      // this is necessary to avoid segmentation fault :)
    }
    // run process
    else if (strcmp(cmd, "run") == 0) {
      perform_run(&args[1]);
    }
    // list processes
    else if (strcmp(cmd, "list") == 0) {
      perform_list();
    }
    // resume process
    else if (strcmp(cmd, "resume") == 0) {
      perform_action(&args[1], SIGCONT);
    }
    // stop process
    else if (strcmp(cmd, "stop") == 0) {
      perform_action(&args[1], SIGSTOP);
    }
    // kill process
    else if (strcmp(cmd, "kill") == 0) {
      perform_action(&args[1], SIGTERM);
    } else if (strcmp(cmd, "list-q") == 0) {
      for (int i = 0; i < proc_queue->length; i++) {
        printf("%d,", proc_queue->_process_queue[i]);
      }
    }
    // exit shell
    else if (strcmp(cmd, "exit") == 0) {
      perform_exit();
      break;
    }
    // command not found
    else {
      printf("%s: command not found\n", cmd);
    }
  }

  // clean up background handler
  kill(bg_handler_pid, SIGTERM);

  return EXIT_SUCCESS;
}
