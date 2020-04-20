#include "common.h"
#include "ipc.h"
#include "pa2345.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  local_id id;
  int *pipes;
  int node_count;
  int last_id;
} Node;

int get_pipe_id(int pipes_count, int from, int to, int is_write);

int set_lamport_time(timestamp_t lt);
timestamp_t get_lamport_time();

typedef struct {
  int forks[MAX_PROCESS_ID];
  int dirty[MAX_PROCESS_ID];
  int reqf[MAX_PROCESS_ID];
} __attribute((packed)) ForkLock;
