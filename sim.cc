#include "command.h"
#include "matrix.h"
#include "state.h"

#include <stdio.h>
#include <stdlib.h>

bool g_stop_after_destruction = false;

enum Mode {
  FA, FD, FR
};

int main(int argc, const char* argv[]) {
  if (argc <= 2) {
    fprintf(stderr, "Usage: %s mdl nbt\n", argv[0]);
    exit(1);
  }

  if (getenv("STOP_AFTER_DESTRUCTION")) {
    g_stop_after_destruction = true;
  }

  Mode mode = FA;
  if (string(argv[1]).find("/FD") != string::npos) {
    mode = FD;
  } else if (string(argv[1]).find("/FR") != string::npos) {
    if (string(argv[2]).find(".mdl") != string::npos) {
      mode = FR;
    } else {
      if (string(argv[1]).find("_src") != string::npos) {
        mode = FD;
      } else {
        mode = FA;
      }
    }
  }

  int next_arg = 2;
  unique_ptr<Matrix> source;
  unique_ptr<Matrix> target;

  if (mode == FA) {
    target.reset(new Matrix(argv[1]));
    source.reset(new Matrix(target->r()));
  } else if (mode == FD) {
    source.reset(new Matrix(argv[1]));
    target.reset(new Matrix(source->r()));
  } else if (mode == FR) {
    source.reset(new Matrix(argv[1]));
    target.reset(new Matrix(argv[2]));
    next_arg++;
  } else {
    assert(false);
  }

  bool is_test = (argc == 4 && string(argv[3]) == "--test");

  vector<Command> trace;
  parseTrace(argv[next_arg], &trace);

#if 0
  for (Command c : trace) {
    printf("%s\n", c.str().c_str());
  }
#endif

  State st(*source);
  size_t trace_idx = 0;
  for (int turn = 0; !st.done; turn++) {
    int nb = st.num_bots();
    //fprintf(stderr, "turn=%d num_bots=%d\n", turn, nb);

    for (int bi = 0; bi < nb; bi++) {
      if (trace_idx >= trace.size()) {
        fprintf(stderr, "trace size overflow\n");
        abort();
      }

      st.step(bi, trace[trace_idx]);
      trace_idx++;
    }

    st.next();

    if (g_stop_after_destruction &&
        st.bots.size() == 1 &&
        st.bots[0]->p.x == 0 &&
        st.bots[0]->p.y == 0 &&
        st.bots[0]->p.z == 0) {
      break;
    }
  }

  printf("energy=%zd\n", st.energy);

#if 0
  int rv = 0;
  for (int y = 0; y < target->r(); y++) {
    for (int x = 0; x < target->r(); x++) {
      for (int z = 0; z < target->r(); z++) {
        if (st.model.full(x, y, z) != target->full(x, y, z)) {
        }
      }
    }
  }
#endif

  assert(st.model == *target);
  if (is_test) {
    assert(st.energy == 24892062);
  }
}
