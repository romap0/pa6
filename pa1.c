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
#include "pa1.h"

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
      pipes[get_pipe_id(node_count, from, to, 0)] = fd[0];
      pipes[get_pipe_id(node_count, from, to, 1)] = fd[1];

      log_pipe(from, to, fd);
    }
  }

  return pipes;
}

void wait_all(Node *node, int s_type) {
  Message input_message;
  for (int sender_id = 1; sender_id < node->node_count; sender_id++) {
    if (sender_id == node->id || sender_id)
      continue;

    if (receive(node, sender_id, &input_message) != 0 ||
        input_message.s_header.s_type != STARTED)
      sender_id--;
  }
}

void create_children(int node_count, int *pipes) {
  for (int node_id = 1; node_id < node_count; node_id++) {
    pid_t pid = fork();

    if (pid == 0) {
      // Child process
      Node node;
      node.id = node_id;
      node.pipes = pipes;
      node.node_count = node_count;

      char log_string[100];
      sprintf(log_string, log_started_fmt, node_id, getpid(), getppid());
      log_event(log_string);

      Message start_message;
      memcpy(start_message.s_payload, log_string, strlen(log_string));
      start_message.s_header.s_payload_len = strlen(log_string);
      start_message.s_header.s_type = STARTED;

      send_multicast(&node, &start_message);

      wait_all(&node, STARTED);
      sprintf(log_string, log_received_all_started_fmt, node_id);
      log_event(log_string);

      sprintf(log_string, log_done_fmt, node_id);
      log_event(log_string);

      Message done_message;
      memcpy(done_message.s_payload, log_string, strlen(log_string));
      done_message.s_header.s_payload_len = strlen(log_string);
      done_message.s_header.s_type = DONE;

      send_multicast(&node, &done_message);

      wait_all(&node, DONE);
      sprintf(log_string, log_received_all_done_fmt, node_id);
      log_event(log_string);

      return;
    }
  }

  // Main process
  Node node;
  node.id = 0;
  node.pipes = pipes;
  node.node_count = node_count;

  char log_string[100];

  wait_all(&node, STARTED);
  sprintf(log_string, log_received_all_started_fmt, 0);
  log_event(log_string);

  wait_all(&node, DONE);
  sprintf(log_string, log_received_all_done_fmt, 0);
  log_event(log_string);

  wait(NULL);
}

int main(int argc, char *argv[]) {
  if (getopt(argc, argv, "p:") != 'p') {
    perror("Required parameter -p");
    exit(EXIT_FAILURE);
  }

  if ((EVENTS_LOG = fopen(events_log, "w")) == 0)
    exit(EXIT_FAILURE);

  if ((PIPES_LOG = fopen(pipes_log, "w")) == 0)
    exit(EXIT_FAILURE);

  int node_count = strtoul(optarg, NULL, 10);
  printf("Nodes count: %d\n\n", node_count);

  int *pipes = create_pipes(node_count + 1);
  printf("\nOpened all pipes\n\n");

  create_children(node_count + 1, pipes);

  fclose(PIPES_LOG);
  fclose(EVENTS_LOG);

  exit(EXIT_SUCCESS);
}
