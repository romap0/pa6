#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "ipc.h"

typedef struct {
  local_id id;
  int *pipes;
  int node_count;
} Node;

int get_pipe_id(int pipes_count, int from, int to, int is_write);
