#include "sim_env.h"
#include <stdio.h>
#include <stdlib.h>

#define COLOR_GREEN "\033[32m"
#define COLOR_RED   "\033[31m"
#define COLOR_RESET "\033[0m"

int main(int argc, char** argv) {
  SimEnv sim_env;

  if (!sim_env.init(argc, argv)) {
    printf(COLOR_RED "[ERROR] Simulation initialization failed!" COLOR_RESET "\n");
    return 1;
  }

  int exit_code = sim_env.run();

  if (exit_code == 0)
    printf(COLOR_GREEN "[PASS] Simulation finished successfully (a0 == 0)." COLOR_RESET "\n");
  else if (exit_code == 1)
    printf(COLOR_RED "[FAIL] Simulation finished with (a0 != 0)." COLOR_RESET "\n");
  else if (exit_code == 2)
    printf(COLOR_RED "[FAIL] Simulation timed out." COLOR_RESET "\n");
  else
    printf(COLOR_RED "[FAIL] Simulation ended unexpectedly." COLOR_RESET "\n");

  sim_env.cleanup();
  return exit_code;
}
