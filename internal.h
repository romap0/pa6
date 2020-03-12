#include "common.h"
#include "ipc.h"
#include "pa1.h"

typedef struct {
  local_id id;
  int *pipes;
  int node_count;
} Node;

int get_pipe_id(int pipes_count, int from, int to, int is_write);
