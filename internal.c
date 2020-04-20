#include "ipc.h"
#include "common.h"
#include "pa2345.h"

#define max(a, b) ((a) > (b) ? (a) : (b))

int get_pipe_id(int node_count, int from, int to, int is_write) {
  return (node_count * 2 * from) + (2 * to) + is_write;
}

timestamp_t ltime = 0;

timestamp_t get_lamport_time(void) { return ltime; }

int set_lamport_time(timestamp_t lt) {
  ltime = max(ltime, lt) + 1;
  return ltime;
}
