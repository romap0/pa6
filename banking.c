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

/** Transfer amount from src to dst.
 *
 * @param parent_data Any data structure implemented by students to perform I/O
 */
void transfer(void *parent_data, local_id src, local_id dst, balance_t amount) {
  local_id id = *(local_id *)parent_data;

  Message message;
  TransferOrder *transfer_order = (TransferOrder *)message.s_payload;

  message.s_header.s_magic = MESSAGE_MAGIC;
  message.s_header.s_type = TRANSFER;
  message.s_header.s_local_time = get_physical_time();
  message.s_header.s_payload_len = sizeof(TransferOrder);

  transfer_order->s_src = src;
  transfer_order->s_dst = dst;
  transfer_order->s_amount = amount;

  send(&id, src, &message);

  Message input_message;

  if (receive(&id, dst, &input_message)) {
    printf("transfer(): receive() error\n");
    exit(1);
  }

  if (input_message.s_header.s_type != ACK) {
    printf("transfer(): not ACK = %d\n", input_message.s_header.s_type);
    exit(1);
  }
}

void client_update_balance_history(BalanceHistory *history,
                                   timestamp_t local_time, balance_t balance) {
  int i;

  for (i = history->s_history_len;
       history->s_history[i - 1].s_time < local_time - 1; i++) {
    history->s_history_len++;
    history->s_history[i] = history->s_history[i - 1];
    history->s_history[i].s_time++;
  }

  history->s_history[history->s_history_len].s_balance = balance;
  history->s_history[history->s_history_len].s_time = local_time;
  history->s_history[history->s_history_len].s_balance_pending_in = 0;
  history->s_history_len++;
}

//------------------------------------------------------------------------------
// Functions below are implemented by lector, test implementations are
// provided to students for testing purposes
//------------------------------------------------------------------------------

/**
 * Returs the value of Lamport's clock.
 */
timestamp_t get_lamport_time() { return 0; }

/** Returns physical time.
 *
 * Emulates physical clock (for each process).
 */
timestamp_t get_physical_time() { return 0; }

/** Pretty print for BalanceHistories.
 *
 */
void print_history(const AllHistory *history);
