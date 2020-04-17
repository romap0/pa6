#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "ipc.h"
#include "banking.h"
#include "pa2345.h"

typedef struct {
  local_id id;
  int *pipes;
  int node_count;
  int balance;
  int physical_time;
} Node;

int get_pipe_id(int pipes_count, int from, int to, int is_write);

void client_update_balance_history(BalanceHistory *history,
                                   timestamp_t local_time, balance_t balance);
