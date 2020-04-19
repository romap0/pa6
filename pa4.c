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

#include "banking.h"
#include "common.h"
#include "internal.h"
#include "ipc.h"
#include "pa2345.h"

typedef struct {
  timestamp_t time;
  int done;
  int pending;
} lqueue_t;

lqueue_t lqueue[16];
int mutexl = 0;

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

int check_cs(void *self) {
  Node *node = (Node *)self;
  local_id i;
  local_id id = 0;

  for (i = 1; i <= node->node_count; i++) {
    if (lqueue[i].pending) {
      if (!id)
        id = i;
      else if (lqueue[i].time < lqueue[id].time)
        id = i;
    }
  }

  return id;
}

int request_cs(const void *self) {
  Message sent_message;
  Node *node = (Node *)self;
  local_id repcnt = 0;

  set_lamport_time(0);
  sent_message.s_header.s_magic = MESSAGE_MAGIC;
  sent_message.s_header.s_type = CS_REQUEST;
  sent_message.s_header.s_local_time = get_lamport_time();
  sent_message.s_header.s_payload_len = 0;

  lqueue[node->id].time = sent_message.s_header.s_local_time;
  lqueue[node->id].pending = 1;

  send_multicast(node, &sent_message);

  while (1) {
    Message received_message;

    receive_any(node, &received_message);

    switch (received_message.s_header.s_type) {
    case CS_REQUEST:
      lqueue[node->last_id].time = received_message.s_header.s_local_time;
      lqueue[node->last_id].pending = 1;

      set_lamport_time(0);

      sent_message.s_header.s_magic = MESSAGE_MAGIC;
      sent_message.s_header.s_type = CS_REPLY;
      sent_message.s_header.s_local_time = get_lamport_time();
      sent_message.s_header.s_payload_len = 0;

      send(node, node->last_id, &sent_message);

      break;

    case CS_REPLY:
      repcnt++;
      if ((repcnt == node->node_count - 2) && check_cs(node) == node->id)
        return 0;
      break;

    case CS_RELEASE:
      lqueue[node->last_id].pending = 0;

      if ((repcnt == node->node_count - 2) && check_cs(node) == node->id)
        return 0;
      break;

    case DONE:
      printf("id = %d  DONE %d\n", node->id, node->last_id);
      lqueue[node->last_id].done = 1;
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
  sent_message.s_header.s_type = CS_RELEASE;
  sent_message.s_header.s_local_time = get_lamport_time();
  sent_message.s_header.s_payload_len = 0;

  lqueue[node->id].pending = 0;
  send_multicast(node, &sent_message);

  return 0;
}

int check_done(void *self) {
  Node *node = (Node *)self;
  local_id i;

  for (i = 1; i < node->node_count; i++) {
    if (!lqueue[i].done)
      return 0;
  }

  return 1;
}

void wait_all_done(void *self) {
  Message sent_message;
  Message received_message;
  Node *node = (Node *)self;

  lqueue[node->id].done = 1;

  if (check_done(node))
    return;

  while (1) {
    receive_any(node, &received_message);

    switch (received_message.s_header.s_type) {
    case CS_REQUEST:

      lqueue[node->last_id].time = received_message.s_header.s_local_time;
      lqueue[node->last_id].pending = 1;

      set_lamport_time(0);
      sent_message.s_header.s_magic = MESSAGE_MAGIC;
      sent_message.s_header.s_type = CS_REPLY;
      sent_message.s_header.s_local_time = get_lamport_time();
      sent_message.s_header.s_payload_len = 0;
      send(node, node->last_id, &sent_message);

      break;
    case CS_RELEASE:
      break;

    case DONE:
      printf("id = %d  DONE %d\n", node->id, node->last_id);
      lqueue[node->last_id].done = 1;
      if (check_done(node))
        return;
      break;
    }
  }

  return;
}

void child_task(int node_id, int node_count, int *pipes, int mutexl) {
  Node node = {node_id, pipes, node_count};
  // node.id = node_id;
  // node.pipes = pipes;
  // node.node_count = node_count;

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
  // node.id = PARENT_ID;
  // node.pipes = pipes;
  // node.node_count = node_count;

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
