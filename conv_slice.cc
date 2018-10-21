#include "command.h"
#include "dep.h"
#include "matrix.h"
#include "scheduler.h"
#include "state.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>

bool g_log = false;

Scheduler* makeGVoidSliceScheduler(int nb);

int main(int argc, const char* argv[]) {
  if (argc <= 1) {
    fprintf(stderr, "Usage: %s ndt\n", argv[0]);
    exit(1);
  }

  vector<Command> trace;
  parseTrace(argv[1], &trace);

  bool need_fix = false;
  for (const Command& c : trace) {
    if (c.op == G_VOID && c.d2.x > 200) {
      need_fix = true;
    }
  }
  if (need_fix) {
    fprintf(stderr, "fix trace...\n");
    for (Command& c : trace) {
      if (c.op == G_VOID) {
        c.d2.x += 30;
        c.d2.y += 30;
        c.d2.z += 30;
        if (c.d2.x > 150) c.d2.x -= 256;
        if (c.d2.y > 150) c.d2.y -= 256;
        if (c.d2.z > 150) c.d2.z -= 256;
      }
    }
  }

  if (argc >= 3) {
    emitTrace(argv[2], trace);
  } else {
    for (const Command& c : trace) {
      printf("%s\n", c.str().c_str());
    }
  }
}
