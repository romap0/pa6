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

int *create_pipes(int node_count) {
  int *pipes = (int *)malloc(node_count * node_count * 2 * sizeof(int));
  int args[2];

  for (int from = 0; from < node_count; from++) {
    for (int to = 0; to < node_count; to++) {
      if (from == to)
        continue;

      pipe(args);
      pipes[get_pipe_id(node_count, from, to, 0)] = args[0];
      pipes[get_pipe_id(node_count, from, to, 1)] = args[1];
    }
  }

  return pipes;
}

void create_children(int node_count, int *pipes) {
  for (int node_id = 1; node_id < node_count; node_id++) {
    pid_t pid = fork();

    if (pid == 0) {
      // Child process
      printf("Child process. node_id: %d\n", node_id);

      Node *node = malloc(sizeof(Node));
      node->id = node_id;
      node->pipes = pipes;
      node->node_count = node_count;

      char out_string[100];
      sprintf(out_string, "message %d", node_id * 10);

      Message *message = malloc(sizeof(Message));
      memcpy(message->s_payload, out_string, strlen(out_string));
      message->s_header.s_payload_len = strlen(out_string);

      send_multicast(node, message);

      Message *message2 = malloc(sizeof(Message));
      receive(node, node_id == 1 ? 2 : 1, message2);

      printf("Message in node %d: %s\n", node_id, message2->s_payload);

      return;
    } else {
      // Main process
      printf("Spawned process. PID: %d\n", pid);
    }
  }

  Node *node = malloc(sizeof(Node));
  node->id = 0;
  node->pipes = pipes;
  node->node_count = node_count;

  Message *message = malloc(sizeof(Message));
  receive_any(node, message);

  printf("Message in main: %s\n", message->s_payload);

  receive_any(node, message);

  printf("Message in main: %s\n", message->s_payload);

  wait(NULL);
}

int main(int argc, char *argv[]) {
  if (getopt(argc, argv, "p:") != 'p') {
    perror("Required parameter -p");
    exit(EXIT_FAILURE);
  }

  uint8_t node_count = strtoul(optarg, NULL, 10);
  printf("Nodes count: %d\n", node_count);

  int *pipes = create_pipes(node_count + 1);
  create_children(node_count + 1, pipes);
}
