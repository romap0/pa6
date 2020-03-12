int get_pipe_id(int node_count, int from, int to, int is_write) {
  return (node_count * 2 * from) + (2 * to) + is_write;
}
