#pragma once

#define PORT 3038
#define MAXLINE 16388

// Configure various options of client.
struct Options {
  // number of thread used for upload, default is zero.
  int thread_num = 0;
  // size of a block (basic unit for transmission), default is 16K.
  int block_size = 16384;
  // number of times to try get an ACK, default is 5.
  int retry_count = 5;
  // time interval between each sweep, default is 2.
  int sweep_period = 2;
};

enum Status {
  Ok = 1,
  IO_ERROR,
  BAD_ARGUMENT
};