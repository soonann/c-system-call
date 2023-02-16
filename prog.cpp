#include <cstdio>
#include <fstream>
#include <iostream>
#include <unistd.h>

enum { ARG_FILENAME = 1, ARG_COUNT = 2, ARGS_COUNT = ARG_COUNT + 1 };

enum { SECONDS = 1 };

int main(int argc, char *argv[]) {
  if (argc != ARGS_COUNT) {
    std::cout << "The program must be run with two arguments.\n";
    return EXIT_FAILURE;
  }
  const int count = atoi(argv[ARG_COUNT]);
  if (count <= 0) {
    std::cout << "The first argument must be a positive integer.\n";
    return EXIT_FAILURE;
  }
  const char *const filename = argv[ARG_FILENAME];

  for (int i = 1; i <= count; ++i) {
    sleep(1 * SECONDS);
    std::ofstream file(filename);
    file << "Process ran " << i << " out of " << count << " secs\n";
  }

  return EXIT_SUCCESS;
}
