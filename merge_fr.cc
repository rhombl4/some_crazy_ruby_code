#include "command.h"
#include "matrix.h"
#include "state.h"

#include <stdio.h>
#include <stdlib.h>

bool g_log = true;

int main(int argc, const char* argv[]) {
  if (argc <= 5) {
    fprintf(stderr, "Usage: %s mdl mdl nbt nbt onbt\n", argv[0]);
    exit(1);
  }

  vector<Command> total_trace;
  ulong d_energy = 0, a_energy = 0;

  {
    if (g_log) {
      fprintf(stderr, "=== destruction ===\n");
    }
    Matrix source(argv[1]);
    Matrix target(source.r());
    State st(source);

    vector<Command> trace;
    parseTrace(argv[3], &trace);

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

      if (st.bots.size() == 1 &&
          st.bots[0]->p.x == 0 &&
          st.bots[0]->p.y == 0 &&
          st.bots[0]->p.z == 0) {
        break;
      }
    }

    assert(st.model == target);

    d_energy += st.energy;
    for (Command cmd : st.trace)
      total_trace.push_back(cmd);

    if (st.bots[0]->id != 1) {
      if (g_log) {
        fprintf(stderr, "swap bot ID %d\n", st.bots[0]->id);
      }

      total_trace.push_back(Fission(Diff(1, 0, 0), 0));
      total_trace.push_back(FusionP(Diff(-1, 0, 0)));
      total_trace.push_back(FusionS(Diff(1, 0, 0)));
      total_trace.push_back(SMove(Diff(-1, 0, 0)));
    }
  }

  {
    if (g_log) {
      fprintf(stderr, "=== construction ===\n");
    }
    Matrix target(argv[2]);
    State st(target.r());

    vector<Command> trace;
    parseTrace(argv[4], &trace);

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
    }

    assert(st.model == target);

    a_energy += st.energy;
    for (Command cmd : st.trace)
      total_trace.push_back(cmd);
  }

  ulong energy = d_energy + a_energy;
  fprintf(stderr, "energy=%zd %zd+%zd\n", energy, d_energy, a_energy);

  emitTrace(argv[5], total_trace);

  string cmd = (string("./sim ") +
                argv[1] + " " +
                argv[2] + " " +
                argv[5]);
  puts(cmd.c_str());
  system(cmd.c_str());
}
