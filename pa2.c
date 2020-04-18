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

void closeUnusedPipes(int *pipes, int node_count, int node_id) {
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
      printf("wrong type\n");
    }
  }
}

void child_task(int node_id, int node_count, int *pipes, int balance) {
  Node node;
  node.id = node_id;
  node.pipes = pipes;
  node.node_count = node_count;
  node.balance = balance;
  node.physical_time = get_physical_time();

  closeUnusedPipes(pipes, node_count, node_id);

  Message sent_message;
  sent_message.s_header.s_magic = MESSAGE_MAGIC;
  Message received_message;

  TransferOrder *transfer_order_sent = (TransferOrder *)sent_message.s_payload;
  TransferOrder *transfer_order_received =
      (TransferOrder *)received_message.s_payload;

  BalanceHistory history = {node_id, 1, {{node.balance, 0, 0}}};

  char log_string[100];
  sprintf(log_string, log_started_fmt, node.physical_time, node_id, getpid(),
          getppid(), node.balance);
  log_event(log_string);

  memcpy(sent_message.s_payload, log_string, strlen(log_string));
  sent_message.s_header.s_payload_len = strlen(log_string);
  sent_message.s_header.s_type = STARTED;
  sent_message.s_header.s_local_time = node.physical_time;

  send_multicast(&node, &sent_message);
  wait_all(&node, STARTED);

  node.physical_time = get_physical_time();

  sprintf(log_string, log_received_all_started_fmt, node.physical_time,
          node_id);
  log_event(log_string);

  uint16_t done = node_count - 2;
  // return;
  while (done) {
    printf("before receive\n");
    // while(1){}
    int receive_result = receive_any(&node, &received_message);
    printf("receive_result: %d\n", receive_result);

    switch (received_message.s_header.s_type) {
    case STOP:
      node.physical_time = get_physical_time();

      sprintf(log_string, log_done_fmt, node.physical_time, node_id,
              node.balance);
      log_event(log_string);

      memcpy(sent_message.s_payload, log_string, strlen(log_string));
      sent_message.s_header.s_payload_len = strlen(log_string);
      sent_message.s_header.s_type = DONE;
      sent_message.s_header.s_local_time = node.physical_time;

      send_multicast(&node, &sent_message);
      break;

    case DONE:
      done--;
      break;

    case TRANSFER:
      if (node_id == transfer_order_received->s_src) {
        node.balance -= transfer_order_received->s_amount;

        *transfer_order_sent = *transfer_order_received;

        node.physical_time = get_physical_time();

        sprintf(log_string, log_transfer_out_fmt, node.physical_time, node_id,
                transfer_order_received->s_amount,
                transfer_order_received->s_dst);
        log_event(log_string);

        // memcpy(sent_message.s_payload, log_string, strlen(log_string));
        sent_message.s_header.s_local_time = node.physical_time;

        sent_message.s_header.s_type = received_message.s_header.s_type;
        sent_message.s_header.s_payload_len =
            received_message.s_header.s_payload_len;

        // send(&node, 0, &sent_message);
        send(&node, transfer_order_received->s_dst, &sent_message);
        client_update_balance_history(
            &history, sent_message.s_header.s_local_time, node.balance);
      } else {
        node.balance += transfer_order_received->s_amount;

        node.physical_time = get_physical_time();

        sprintf(log_string, log_transfer_in_fmt, node.physical_time, node_id,
                transfer_order_received->s_amount,
                transfer_order_received->s_dst);
        log_event(log_string);

        // memcpy(sent_message.s_payload, log_string, strlen(log_string));
        sent_message.s_header.s_local_time = node.physical_time;

        sent_message.s_header.s_type = ACK;
        sent_message.s_header.s_payload_len = 0;

        send(&node, 0, &sent_message);

        client_update_balance_history(
            &history, sent_message.s_header.s_local_time, node.balance);
      }
      break;
    }
    // sleep(1);
  }

  node.physical_time = get_physical_time();

  sprintf(log_string, log_received_all_done_fmt, node.physical_time, node_id);
  log_event(log_string);

  client_update_balance_history(&history, sent_message.s_header.s_local_time,
                                node.balance);

  node.physical_time = get_physical_time();
  sent_message.s_header.s_type = BALANCE_HISTORY;
  sent_message.s_header.s_local_time = node.physical_time;
  sent_message.s_header.s_payload_len = (uint16_t)sizeof(history);
  memcpy(sent_message.s_payload, &history, sizeof(history));
  send(&node, PARENT_ID, &sent_message);
}

void main_task(int node_count, int *pipes) {
  Node node;
  node.id = 0;
  node.pipes = pipes;
  node.node_count = node_count;
  node.physical_time = get_physical_time();

  Message sent_message;
  sent_message.s_header.s_magic = MESSAGE_MAGIC;
  Message received_message;

  BalanceHistory *history = (BalanceHistory *)received_message.s_payload;
  AllHistory all = {0};

  closeUnusedPipes(pipes, node_count, 0);

  char log_string[100];

  wait_all(&node, STARTED);
  sprintf(log_string, log_received_all_started_fmt, node.physical_time,
          PARENT_ID);
  log_event(log_string);

  bank_robbery(&node, node_count - 1);
  // return;

  node.physical_time = get_physical_time();

  sent_message.s_header.s_type = STOP;
  sent_message.s_header.s_local_time = node.physical_time;
  sent_message.s_header.s_payload_len = 0;

  send_multicast(&node, &sent_message);

  wait_all(&node, DONE);
  sprintf(log_string, log_received_all_done_fmt, get_physical_time(), 0);
  log_event(log_string);

  for (int i = 1; i < node_count; i++) {
    receive(&node, i, &received_message);
    if (received_message.s_header.s_type == BALANCE_HISTORY) {
      all.s_history_len++;
      all.s_history[i - 1] = *history;
    } else {
      printf(">>>> ERROR: BALANCE_HISTORY %d\n",
             received_message.s_header.s_type);
    }
  }

  print_history(&all);

  for (int node_id = 1; node_id < node_count; node_id++) {
    wait(NULL);
  }
}

void create_children(int node_count, int *pipes, char **argv) {
  for (int node_id = 1; node_id < node_count; node_id++) {
    pid_t pid = fork();

    if (pid == 0) {
      // Child process
      child_task(node_id, node_count, pipes, atoi(argv[optind + node_id - 1]));
      return;
    }
  }
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

  if (node_count > argc - optind) {
    fprintf(stderr, "Initial balance is not specified\n");
    exit(1);
  }

  int *pipes = create_pipes(node_count + 1);
  printf("\nOpened all pipes\n\n");

  create_children(node_count + 1, pipes, argv);

  // Main process
  main_task(node_count + 1, pipes);

  fclose(PIPES_LOG);
  fclose(EVENTS_LOG);

  exit(EXIT_SUCCESS);
}
