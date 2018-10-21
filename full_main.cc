#include "command.h"
#include "matrix.h"
#include "scheduler.h"
#include "state.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>

#include <map>
#include <queue>

bool g_log = false;

Scheduler* makeLiftScheduler(int nb);

int main(int argc, const char* argv[]) {
  if (argc <= 1) {
    fprintf(stderr, "Usage: %s mdl\n", argv[0]);
    exit(1);
  }

  int num_bots = 1;
  if (argc >= 4) {
    num_bots = atoi(argv[3]);
  }
  bool is_test = false;
  if (argc >= 5) {
    if (string(argv[4]) == "--log") {
      g_log = true;
    }
    if (string(argv[4]) == "--test") {
      is_test = true;
    }
  }
  unique_ptr<Scheduler> scheduler(makeLiftScheduler(num_bots));

  Matrix target(argv[1]);
  State st(target.r());
  scheduler->schedule(target, &st);

  if (st.harmonics) {
    st.step(0, Flip());
    st.next();
  }
  st.step(0, Halt());
  st.next();

  fprintf(stderr, "energy=%zd\n", st.energy);
  vector<Command> trace = st.trace;

  if (argc >= 3) {
    emitTrace(argv[2], trace);
  } else {
    for (const Command& c : trace) {
      printf("%s\n", c.str().c_str());
    }
  }

  if (is_test) {
    //assert(2408890 == st.energy);
    assert(1512028 == st.energy);
  }
}
