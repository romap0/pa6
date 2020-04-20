#include <fcntl.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "common.h"
#include "internal.h"
#include "ipc.h"
#include "pa2345.h"

int mutexl = 0;
ForkLock lock;
int done_count = 0;

FILE *EVENTS_LOG, *PIPES_LOG;

void log_event(char *fmt) {
  fprintf(EVENTS_LOG, "%s", fmt);
  fflush(EVENTS_LOG);
  printf("%s", fmt);
}

void log_pipe(int from, int to, int fd[]) {
  fprintf(PIPES_LOG, "Open pipe %d -> %d [%d, %d]\n", from, to, fd[0], fd[1]);
  fflush(PIPES_LOG);
  printf("Open pipe %d -> %d [%d, %d]\n", from, to, fd[0], fd[1]);
}

int *create_pipes(int node_count) {
  int *pipes = (int *)malloc(node_count * node_count * 2 * sizeof(int));
  int fd[2];

  for (int from = 0; from < node_count; from++) {
    for (int to = 0; to < node_count; to++) {
      if (from == to)
        continue;

      pipe(fd);
      fcntl(fd[0], F_SETFL, O_NONBLOCK);
      fcntl(fd[1], F_SETFL, O_NONBLOCK);
      pipes[get_pipe_id(node_count, from, to, 0)] = fd[0];
      pipes[get_pipe_id(node_count, from, to, 1)] = fd[1];

      log_pipe(from, to, fd);
    }
  }

  return pipes;
}

void close_unused_pipes(int *pipes, int node_count, int node_id) {
  // return;
  for (int i = 0; i < node_count; i++) {
    for (int k = 0; k < node_count; k++) {
      if (k != node_id) {
        int pipe1 = pipes[get_pipe_id(node_count, i, k, 0)];
        if (pipe1 != 0) {
          close(pipe1);
        }
      }

      if (i != node_id) {
        int pipe2 = pipes[get_pipe_id(node_count, i, k, 1)];
        if (pipe2 != 0) {
          close(pipe2);
        }
      }
    }
  }
}

void wait_all(Node *node, int s_type) {
  Message input_message;
  for (int sender_id = 1; sender_id < node->node_count; sender_id++) {
    if (sender_id == node->id)
      continue;

    printf("(%d <- %d) %d?\n", node->id, sender_id, s_type);
    if (receive(node, sender_id, &input_message) != 0 ||
        input_message.s_header.s_type != s_type) {
      sender_id--;
      // printf("wrong type\n");
    }
  }
}

int check_all_forks(const Node *node) {
  for (int i = 1; i < node->node_count; i++) {
    if (i == node->id)
      continue;
    if (lock.forks[i] == 0)
      return 0;
  }

  return 1;
}

int request_cs(const void *self) {
  Node *node = (Node *)self;

  while (!check_all_forks(node)) {
    Message sent_message;
    Message received_message;

    set_lamport_time(0);
    sent_message.s_header.s_magic = MESSAGE_MAGIC;
    sent_message.s_header.s_type = CS_REQUEST;
    sent_message.s_header.s_local_time = get_lamport_time();
    sent_message.s_header.s_payload_len = 0;

    for (int i = 1; i < node->node_count; i++) {
      if (i == node->id)
        continue;

      if (lock.forks[i] == 0 && lock.reqf[i] == 1) {
        send(node, i, &sent_message);
        lock.reqf[i] = 0;
      }
    }

    receive_any(node, &received_message);

    switch (received_message.s_header.s_type) {
    case DONE:
      done_count++;
      break;
    case CS_REQUEST:
      lock.reqf[node->last_id] = 1;

      if (lock.forks[node->last_id] && lock.dirty[node->last_id]) {
        lock.forks[node->last_id] = lock.dirty[node->last_id] = 0;

        set_lamport_time(0);
        sent_message.s_header.s_magic = MESSAGE_MAGIC;
        sent_message.s_header.s_type = CS_REPLY;
        sent_message.s_header.s_local_time = get_lamport_time();
        sent_message.s_header.s_payload_len = 0;
        send(node, node->last_id, &sent_message);
      }
      break;
    case CS_REPLY:
      lock.forks[node->last_id] = 1;
      lock.dirty[node->last_id] = 0;
      break;
    }
  }

  return 0;
}

int release_cs(const void *self) {
  Message sent_message;
  Node *node = (Node *)self;

  set_lamport_time(0);
  sent_message.s_header.s_magic = MESSAGE_MAGIC;
  sent_message.s_header.s_type = CS_REPLY;
  sent_message.s_header.s_local_time = get_lamport_time();
  sent_message.s_header.s_payload_len = 0;

  for (int n_id = 1; n_id < node->node_count; ++n_id) {
    if (n_id == node->id)
      continue;
    lock.dirty[n_id] = 1;
    if (lock.reqf[n_id] == 1) {
      lock.forks[n_id] = 0;
      send(node, n_id, &sent_message);
    }
  }

  return 0;
}

void wait_all_done(void *self) {
  Message sent_message;
  Message received_message;
  Node *node = (Node *)self;

  while (done_count < node->node_count - 2) {
    receive_any(node, &received_message);

    switch (received_message.s_header.s_type) {
    case DONE:
      done_count++;
      break;
    case CS_REQUEST:
      lock.reqf[node->last_id] = 1;

      if (lock.forks[node->last_id] && lock.dirty[node->last_id]) {
        lock.forks[node->last_id] = lock.dirty[node->last_id] = 0;

        set_lamport_time(0);
        sent_message.s_header.s_magic = MESSAGE_MAGIC;
        sent_message.s_header.s_type = CS_REPLY;
        sent_message.s_header.s_local_time = get_lamport_time();
        sent_message.s_header.s_payload_len = 0;
        send(node, node->last_id, &sent_message);
      }
      break;
    case CS_REPLY:
      lock.forks[node->last_id] = 1;
      lock.dirty[node->last_id] = 0;
      break;
    }
  }

  return;
}

void child_task(int node_id, int node_count, int *pipes, int mutexl) {
  Node node = {node_id, pipes, node_count};

  close_unused_pipes(pipes, node_count, node_id);

  Message sent_message;
  sent_message.s_header.s_magic = MESSAGE_MAGIC;

  set_lamport_time(0);

  char log_string[100];
  sprintf(log_string, log_started_fmt, get_lamport_time(), node_id, getpid(),
          getppid(), 0);
  log_event(log_string);

  memcpy(sent_message.s_payload, log_string, strlen(log_string));
  sent_message.s_header.s_payload_len = strlen(log_string);
  sent_message.s_header.s_type = STARTED;
  sent_message.s_header.s_local_time = get_lamport_time();

  send_multicast(&node, &sent_message);
  wait_all(&node, STARTED);

  sprintf(log_string, log_received_all_started_fmt, get_lamport_time(),
          node_id);
  log_event(log_string);

  for (int i = 1; i < node.node_count; i++) {
    lock.forks[i] = lock.dirty[i] = lock.reqf[i] = 0;
    if (node.id < i)
      lock.forks[i] = lock.dirty[i] = 1;
    if (node.id > i)
      lock.reqf[i] = 1;
  }

  for (int i = 1; i <= node_id * 5; i++) {
    sprintf(log_string, log_loop_operation_fmt, node_id, i, node_id * 5);

    if (mutexl)
      request_cs(&node);

    print(log_string);

    if (mutexl)
      release_cs(&node);
  }
  set_lamport_time(0);

  sprintf(log_string, log_done_fmt, get_lamport_time(), node_id, 0);
  log_event(log_string);

  memcpy(sent_message.s_payload, log_string, strlen(log_string));
  sent_message.s_header.s_payload_len = strlen(log_string);
  sent_message.s_header.s_type = DONE;
  sent_message.s_header.s_local_time = get_lamport_time();

  send_multicast(&node, &sent_message);

  // wait_all(&node, DONE);
  wait_all_done(&node);

  sprintf(log_string, log_received_all_done_fmt, get_lamport_time(), node_id);
  log_event(log_string);
}

void main_task(int node_count, int *pipes) {
  Node node = {PARENT_ID, pipes, node_count};

  close_unused_pipes(pipes, node_count, 0);

  char log_string[100];

  wait_all(&node, STARTED);
  sprintf(log_string, log_received_all_started_fmt, get_lamport_time(),
          PARENT_ID);
  log_event(log_string);
  // sleep(10);
  wait_all(&node, DONE);
  sprintf(log_string, log_received_all_done_fmt, get_lamport_time(), 0);
  log_event(log_string);

  for (int node_id = 1; node_id < node_count; node_id++) {
    wait(NULL);
  }
}

void create_children(int node_count, int *pipes, int mutexl) {
  for (int node_id = 1; node_id < node_count; node_id++) {
    pid_t pid = fork();

    if (pid == 0) {
      // Child process
      child_task(node_id, node_count, pipes, mutexl);
      exit(0);
      return;
    }
  }
}

int main(int argc, char *argv[]) {
  int node_count = 0;
  int ch;
  int mutexl = 0;

  while ((ch = getopt(argc, argv, "p:-:")) != -1) {
    switch (ch) {
    case 'p':
      node_count = atoi(optarg);
      break;

    case '-':
      if (!strcmp(optarg, "mutexl")) {
        mutexl++;
      }
      break;

    case '?':
    default:
      printf("Usage: %s -P X\n", argv[0]);
      exit(EXIT_FAILURE);
    }
  }
  // if (getopt(argc, argv, "p:") != 'p') {
  //   perror("Required parameter -p");
  //   exit(EXIT_FAILURE);
  // }

  if ((EVENTS_LOG = fopen(events_log, "w")) == 0)
    exit(EXIT_FAILURE);

  if ((PIPES_LOG = fopen(pipes_log, "w")) == 0)
    exit(EXIT_FAILURE);

  // int node_count = strtoul(optarg, NULL, 10);
  printf("Nodes count: %d\n", node_count);
  printf("Mutexl: %d\n\n", mutexl);

  int *pipes = create_pipes(node_count + 1);
  printf("\nOpened all pipes\n\n");

  create_children(node_count + 1, pipes, mutexl);

  // Main process
  main_task(node_count + 1, pipes);

  fclose(PIPES_LOG);
  fclose(EVENTS_LOG);

  exit(EXIT_SUCCESS);
}
