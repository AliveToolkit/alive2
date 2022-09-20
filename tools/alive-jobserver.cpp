// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include <cassert>
#include <csignal>
#include <cstdlib>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace std;

// max_procs = min(Linux default fifo buffer size, OS X default fifo buffer
// size)
static const int max_procs = 16384;

static char fifo_filename[1024];

static void remove_fifo() {
  if (unlink(fifo_filename) != 0) {
    perror("unlink");
    exit(-1);
  }
}

static void sigint_handler(int) {
  remove_fifo();
  exit(-1);
}

static void add_token(int pipefd) {
  char c = 0;
  if (write(pipefd, &c, 1) != 1) {
    perror("alive-jobserver: write");
    exit(-1);
  }
}

#ifdef __linux__
static int count_runnable() {
  const int BUFSIZE = 1024;
  char buf[BUFSIZE];
  int fd = open("/proc/loadavg", O_RDONLY);
  if (fd == -1) {
    perror("open");
    exit(-1);
  }
  int len = read(fd, buf, BUFSIZE);
  if (len == -1) {
    perror("read");
    exit(-1);
  }
  int spaces = 0, pos;
  for (pos = 0; pos < len; ++pos) {
    if (buf[pos] == ' ')
      ++spaces;
    if (spaces == 3)
      break;
  }
  assert(spaces == 3);
  ++pos;
  char *endp;
  int runnable = strtol(&buf[pos], &endp, 10);
  assert(*endp == '/');
  return runnable - 1;
}

static int count_tokens(int pipefd) {
  int flags = fcntl(pipefd, F_GETFL, 0);
  if (flags == -1) {
    perror("alive-jobserver: fcntl");
    exit(-1);
  }
  flags |= O_NONBLOCK;
  if (fcntl(pipefd, F_SETFL, flags) != 0) {
    perror("alive-jobserver: fcntl");
    exit(-1);
  }
  int toks = 0;
  char c;
  while (read(pipefd, &c, 1) != -1)
    ++toks;
  assert(errno == EWOULDBLOCK);
  for (int i = 0; i < toks; ++i)
    add_token(pipefd);
  return toks;
}
#endif

static void refill_tokens(int nprocs, int pipefd) {
#ifdef __linux__
  /*
   * so we end up sometimes leaking tokens; here we fill them back
   * in, with the goal of there always being a number of tokens in
   * the FIFO that's at least as many as the gap between the current
   * number of runnable processes and the desired concurrency
   * level. this would be a really bad strategy for non-CPU-bound
   * workloads, but it works well enough here
   */
  int runnable = count_runnable();
  int desired_tokens = nprocs - runnable;
  if (desired_tokens > 0) {
    int current_tokens = count_tokens(pipefd);
    int tokens_to_add = desired_tokens - current_tokens;
    if (tokens_to_add > 0)
      for (int i = 0; i < tokens_to_add; ++i)
        add_token(pipefd);
  }
#endif
}

static void usage() {
  cerr << "usage: alive-jobserver -jN [command [args]]\n"
          "where N is in 1.."
       << max_procs << "\n";
  exit(-1);
}

int main(int argc, char *const argv[]) {
  std::signal(SIGINT, sigint_handler);

  if (argc < 2)
    usage();
  int nprocs = -1;
  string_view arg(argv[1]);
  if (arg.compare(0, 2, "-j") == 0 && arg.size() > 2)
    nprocs = strtol(arg.substr(2).data(), nullptr, 10);
  if (nprocs < 1 || nprocs > max_procs)
    usage();
  /*
   * process that we initially exec gets a token for free, so put one
   * fewer tokens into the fifo
   */
  if (argc > 2)
    --nprocs;

  srand(getpid() + time(nullptr));
  do {
    snprintf(fifo_filename, sizeof(fifo_filename), "/tmp/alive2_fifo_%lx",
             (unsigned long)rand());
  } while (access(fifo_filename, F_OK) == 0);

  if (mkfifo(fifo_filename, 0666) != 0) {
    perror("alive-jobserver: mkfifo");
    exit(-1);
  }

  int pipefd = open(fifo_filename, O_RDWR);
  if (pipefd < 0) {
    perror("alive-jobserver: open");
    exit(-1);
  }

  for (int i = 0; i < nprocs; ++i)
    add_token(pipefd);

  std::fflush(nullptr);
  pid_t pid = fork();
  if (pid == -1) {
    perror("alive-jobserver: fork");
    exit(-1);
  }
  if (pid == 0) {
    std::signal(SIGINT, SIG_DFL);
    if (setenv("ALIVE_JOBSERVER_FIFO", fifo_filename, true) != 0) {
      perror("setenv");
      exit(-1);
    }
    if (setenv("ALIVECC_PARALLEL_FIFO", "1", true) != 0) {
      perror("setenv");
      exit(-1);
    }
    execvp(argv[2], &argv[2]);
    perror("alive-jobserver: exec");
    exit(-1);
  }

  while (true) {
    if (waitpid(-1, nullptr, WNOHANG) != 0)
      break;
    refill_tokens(nprocs, pipefd);
    sleep(1);
  }

  remove_fifo();

  return 0;
}
