#include "ipc.h"
#include "common.h"
#include "internal.h"
#include "pa2345.h"
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

/** Send a message to the process specified by id.
 *
 * @param self    Any data structure implemented by students to perform I/O
 * @param dst     ID of recipient
 * @param msg     Message to send
 *
 * @return 0 on success, any non-zero value on error
 */
int send(void *self, local_id dst, const Message *msg) {
  Node *node = (Node *)self;
  size_t message_len = sizeof(MessageHeader) + msg->s_header.s_payload_len;
  int pipe = node->pipes[get_pipe_id(node->node_count, node->id, dst, 1)];
  printf("(%d -> %d) %d\n", node->id, dst, msg->s_header.s_type);
  return write(pipe, msg, message_len) == -1;
}

//------------------------------------------------------------------------------

/** Send multicast message.
 *
 * Send msg to all other processes including parent.
 * Should stop on the first error.
 *
 * @param self    Any data structure implemented by students to perform I/O
 * @param msg     Message to multicast.
 *
 * @return 0 on success, any non-zero value on error
 */
int send_multicast(void *self, const Message *msg) {
  Node *node = (Node *)self;

  for (int node_id = 0; node_id < node->node_count; node_id++) {
    if (node_id == node->id)
      continue;

    if (send(node, node_id, msg) != 0)
      return 1;
  }

  return 0;
}

//------------------------------------------------------------------------------

/** Receive a message from the process specified by id.
 *
 * Might block depending on IPC settings.
 *
 * @param self    Any data structure implemented by students to perform I/O
 * @param from    ID of the process to receive message from
 * @param msg     Message structure allocated by the caller
 *
 * @return 0 on success, any non-zero value on error
 */
int receive(void *self, local_id from, Message *msg) {
  Node *node = (Node *)self;

  int pipe = node->pipes[get_pipe_id(node->node_count, from, node->id, 0)];

  while (read(pipe, &(msg->s_header), sizeof(MessageHeader)) < 0) {
    // sleep(1);
  }

  while (read(pipe, msg->s_payload, msg->s_header.s_payload_len) < 0) {
    // sleep(1);
  }

  printf("(%d <- %d) %d\n", node->id, from, msg->s_header.s_type);
  set_lamport_time(msg->s_header.s_local_time);
  return 0;
}

//------------------------------------------------------------------------------

/** Receive a message from any process.
 *
 * Receive a message from any process, in case of blocking I/O should be used
 * with extra care to avoid deadlocks.
 *
 * @param self    Any data structure implemented by students to perform I/O
 * @param msg     Message structure allocated by the caller
 *
 * @return 0 on success, any non-zero value on error
 */
int receive_any(void *self, Message *msg) {
  Node *node = (Node *)self;

  while (1) {
    for (int node_id = 0; node_id < node->node_count; node_id++) {
      if (node_id == node->id)
        continue;

      int pipe =
          node->pipes[get_pipe_id(node->node_count, node_id, node->id, 0)];

      if (read(pipe, &(msg->s_header), sizeof(MessageHeader)) > 0) {
        // printf("(%d <- %d) %d!\n", node->id, node_id, msg->s_header.s_type);
        while (1) {
          if (read(pipe, msg->s_payload, msg->s_header.s_payload_len) >= 0) {
            // printf("(%d <- %d) %d!!\n", node->id, node_id,
            //        msg->s_header.s_type);
            set_lamport_time(msg->s_header.s_local_time);
            return 0;
          }
        }
      }
    }

    // sleep(1);
  }

  return 1;
}
