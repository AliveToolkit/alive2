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

// max_procs = min(Linux default fifo buffer size, OS X default fifo buffer size)
static const int max_procs = 16384;

static char fifo_filename[1024];

static void count_tokens(int fd, int nprocs) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
    perror("alive-jobserver: fcntl");
    exit(-1);
  }
  flags |= O_NONBLOCK;
  int res = fcntl(fd, F_SETFL, flags);
  if (res != 0) {
    perror("alive-jobserver: fcntl");
    exit(-1);
  }
  int toks = 0;
  char c;
  while (read(fd, &c, 1) != -1)
    ++toks;
  assert(errno == EWOULDBLOCK);
  if (toks != nprocs) {
    cerr << "alive-jobserver: expected " << nprocs
         << " jobserver tokens "
            "but instead it found "
         << toks << "\n";
    exit(-1);
  }
}

static void remove_fifo() {
  int res = unlink(fifo_filename);
  if (res != 0) {
    perror("unlink");
    exit(-1);
  }
}

static void sigint_handler(int) {
  remove_fifo();
  exit(-1);
}

static void usage() {
  cerr << "usage: alive-jobserver -jN [command [args]]\n"
          "where N is in 1.."
       << max_procs
       << "\n"
          "\n"
          "alive-jobserver supports two modes of operation:\n"
          "\n"
          "if a command is specified, the jobserver executes it, passing it\n"
          "information about the jobserver fifo in an environment variable.\n"
          "\n"
          "if no command is specified, the jobserver hangs forever, and you\n"
          "must manually use the ALIVE_JOBSERVER_FIFO environment variable\n"
          "to pass this information to the Alive2 LLVM plugin.\n";
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

  srand(getpid() + time(nullptr));
  do {
    sprintf(fifo_filename, "/tmp/alive2_fifo_%lx", (unsigned long)rand());
  } while (access(fifo_filename, F_OK) == 0);

  int res = mkfifo(fifo_filename, 0666);
  if (res != 0) {
    perror("alive-jobserver: mkfifo");
    exit(-1);
  }

  int pipefd = open(fifo_filename, O_RDWR);
  if (pipefd < 0) {
    perror("alive-jobserver: open");
    exit(-1);
  }

  for (int i = 0; i < nprocs; ++i) {
    char c = 0;
    res = write(pipefd, &c, 1);
    if (res != 1) {
      perror("alive-jobserver: write");
      exit(-1);
    }
  }

  if (argc == 2) {
    cerr << "Alive2 jobserver is running.\n";
    cerr << "to use it from a different shell:\n";
    cerr << "\n";
    cerr << "export ALIVE_JOBSERVER_FIFO=" << fifo_filename << "\n";
    cerr << "export ALIVECC_PARALLEL_FIFO=1\n";
    cerr << "\n";
    cerr << "kill this jobserver using ^C when finished.\n";
    while (true)
      sleep(1000000);
  } else {
    std::fflush(nullptr);
    pid_t pid = fork();
    if (pid == -1) {
      perror("alive-jobserver: fork");
      exit(-1);
    }
    if (pid == 0) {
      std::signal(SIGINT, SIG_DFL);
      res = setenv("ALIVE_JOBSERVER_FIFO", fifo_filename, true);
      if (res != 0) {
        perror("setenv");
        exit(-1);
      }
      res = setenv("ALIVECC_PARALLEL_FIFO", "1", true);
      if (res != 0) {
        perror("setenv");
        exit(-1);
      }
      execvp(argv[2], &argv[2]);
      perror("alive-jobserver: exec");
      exit(-1);
    }
    wait(nullptr);
  }

  count_tokens(pipefd, nprocs);
  remove_fifo();

  return 0;
}
