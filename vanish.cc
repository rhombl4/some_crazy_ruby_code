#include "command.h"
#include "dep.h"
#include "matrix.h"
#include "scheduler.h"
#include "state.h"
#include "util.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <random>

extern bool g_log;

class VanishScheduler : public Scheduler {
  struct Task {
    int gx, gy, gz;
    int x, y, z;
    int dx, dy, dz;
  };

 public:
  explicit VanishScheduler(int max_num_bots)
      : max_num_bots_(max_num_bots) {
    assert(max_num_bots == 8);
  }

  virtual void schedule(const Matrix& target0,
                        State* st0) {
    Matrix m(st0->model);
    std::random_device seed_gen;
    std::mt19937 engine(seed_gen());
    ulong best_energy = INT_MAX;
    vector<Command> best_trace;
    for (int t = 0; t < 1000; t++) {
      State st(m);
      if (scheduleImpl(target0, t > 0, engine, &st)) {
        //puts("OK");
        if (best_energy > st.energy) {
          best_energy = st.energy;
          best_trace = st.trace;
          *st0 = st;
        }
      }
    }

#if 0
    printf("best_trace_size=%zu\n", best_trace.size());
    for (const Command& c : best_trace) {
      puts(c.str().c_str());
    }
#endif

    assert(!best_trace.empty());
  }

  bool scheduleImpl(const Matrix& target0,
                    bool use_shuffle,
                    std::mt19937& engine,
                    State* st0) {
    target = &target0;
    st = st0;
    st->model.bound(&bb);
    roof_ = bb.max_y + 1;
    if (roof_ <= 1)
      roof_ = 2;
    if (g_log) {
      fprintf(stderr, "roof=%d (%d,%d,%d)-(%d,%d,%d)\n",
              roof_, bb.min_x, bb.min_y, bb.min_z,
              bb.max_x, bb.max_y, bb.max_z);
    }

    int dx = bb.max_x - bb.min_x;
    int dy = bb.max_y - bb.min_y;
    int dz = bb.max_z - bb.min_z;
    fprintf(stderr, "D: dx=%d dy=%d dz=%d\n", dx, dy, dz);
    assert(dx <= 30);
    assert(dy <= 30);
    assert(dz <= 30);

    int grand_total = st->model.total();
    assert(grand_total);

    bool prologue = true;

    vector<Task*> tasks;
    for (int i = 0; i < 8; i++) {
      Task* task = new Task;
      if (i % 2 == 0) {
        task->x = bb.min_x;
        task->dx = dx;
      } else {
        task->x = bb.max_x;
        task->dx = -dx;
      }
      if (i / 2 % 2 == 0) {
        task->y = bb.min_y;
        task->dy = dy;
      } else {
        task->y = bb.max_y;
        task->dy = -dy;
      }
      if (i / 4 % 2 == 0) {
        task->z = bb.min_z;
        task->dz = dz;
      } else {
        task->z = bb.max_z;
        task->dz = -dz;
      }

      task->gx = task->x;
      task->gy = task->y;
      task->gz = task->z;

      if (task->y == bb.max_y) {
        task->gy++;
      } else {
        if (task->x == bb.min_x) {
          task->gx--;
        } else {
          task->gx++;
        }
      }

      assert(!st->model.full(task->gx, task->gy, task->gz));

      tasks.push_back(task);
    }

    reverse(tasks.begin() + 1, tasks.end());
    if (use_shuffle) {
      shuffle(tasks.begin() + 1, tasks.end(), engine);
    }

    for (int turn = 0; !st->done; turn++) {
      grand_total = st->model.total();
#if 0
      if (g_log) {
        fprintf(stderr, "turn=%d total=%d\n", turn, grand_total);
      }
#endif

      if (tasks.empty() && grand_total) {
        if (g_log) {
          for (int y = roof(); y >= 0; y--) {
            fprintf(stderr, "y=%d\n", y);
            for (int x = 0; x <= st->model.r(); x++) {
              for (int z = 0; z <= st->model.r(); z++) {
                char c = st->model.full(x, y, z) ? 'O' : '_';
                fprintf(stderr, "%c", c);
              }
              fprintf(stderr, "\n");
            }
          }
        }
        assert(false);
      }

      int num_bots = (int)st->bots.size();

      int fusion_p = -1;
      int fusion_s = -1;
      if (tasks.empty()) {
        findFusion(&fusion_p, &fusion_s);
      }

      size_t orig_num_traces = st->trace.size();

      bool issue_gvoid = false;
      if (!tasks.empty() && num_bots == 8) {
        bool all_ok = true;
        for (int bi = 0; bi < 8; bi++) {
          Bot* bot = st->bots[bi];
          Task* task = tasks[bi];
          assert(task);
          if (bot->p != Pos(task->gx, task->gy, task->gz)) {
            all_ok = false;
            if (g_log) {
              fprintf(stderr, "%d not ready\n", bi);
            }
          } else {
            if (g_log) {
              fprintf(stderr, "%d ready\n", bi);
            }
          }
        }

        if (all_ok) {
          issue_gvoid = true;
          for (int bi = 0; bi < 8; bi++) {
            Task* task = tasks[bi];
            Diff nd(task->x - task->gx,
                    task->y - task->gy,
                    task->z - task->gz);
            st->step(bi, GVoid(nd, task->dx, task->dy, task->dz));
          }
          for (Task* task : tasks) delete task;
          tasks.clear();
        }
      }

      for (int bi = 0; bi < num_bots && !issue_gvoid; bi++) {
        Bot* bot = st->bots[bi];
        auto gotoXZAtRoof = [&](int gx, int gz) {
          return Scheduler::gotoXZAtRoof(bi, gx, gz);
        };

        if (bi == fusion_p && fusion_s >= 0) {
          Bot* sbot = st->bots[fusion_s];
          st->step(bi, FusionP(Diff(sbot->p.x, sbot->p.y-roof(), sbot->p.z)));
          continue;
        }
        if (bi == fusion_s && fusion_p >= 0) {
          st->step(bi, FusionS(Diff(-bot->p.x, roof()-bot->p.y, -bot->p.z)));
          continue;
        }

        if (tasks.empty()) {
          if (gotoXZAtRoof(0, 0))
            continue;
          if (num_bots == 1) {
            //epilogue = true;
            if (goStraight(Y, bi, 0)) {
              continue;
            }
            assert(bot->p.x == 0);
            assert(bot->p.y == 0);
            assert(bot->p.z == 0);
            goto done;
          }
          st->step(bi, Wait());
          continue;
        }

        if (prologue &&
            bi == 0 && num_bots < max_num_bots_ &&
            bot->p.x == 0 && bot->p.y == 0 && bot->p.z == 0) {
          spawnToRoof(0);
          //spawnRandomly(0);
          continue;
        }

        if (bi == 0) {
          prologue = false;
        }

        assert(!tasks.empty());
        Task* task = tasks[bi];

        if (bot->p.y < task->gy) {
          if (!goStraight(Y, bi, task->gy)) {
            st->step(bi, Wait());
          }
          continue;
        }

        if (gotoXYZNoAvoid(bi, task->gx, task->gy, task->gz))
          continue;

        st->step(bi, Wait());
      }

      int num_new_traces = st->trace.size() - orig_num_traces;
      if (g_log) {
        fprintf(stderr,
                "turn=%d num_bots=%d total=%d num_new_traces=%d tasks=%d\n",
                turn, num_bots, grand_total, num_new_traces,
                (int)tasks.size());
        for (int i = 0; i < num_new_traces; i++) {
          Command cmd = st->trace[st->trace.size() - num_new_traces + i];
          fprintf(stderr, "i=%d %s at %s\n",
                  i,
                  cmd.str().c_str(),
                  st->bots[i]->p.str().c_str());

          if (cmd.op == FILL) {
            Pos n = applyNear(st->bots[i]->p, cmd.d);
            fprintf(stderr, "FILL! at %s by %d\n",
                    n.str().c_str(), i);
          }
        }
      }
      assert(num_bots == num_new_traces);

      if (!handleAllWait())
        return false;

      st->next();
    }

 done:
    return true;
  }

 private:
  bool handleAllWait(/*vector<Task*>& tasks*/) {
    int num_bots = (int)st->bots.size();
    static int all_wait_count = 0;
    bool all_wait = true;
    for (int i = 0; i < num_bots; i++) {
      Command cmd = st->trace[st->trace.size() - num_bots + i];
      if (cmd.op != WAIT) {
        all_wait = false;
      }
    }
    if (all_wait) {
      if (g_log) {
        fprintf(stderr, "all_wait!\n");
      }

      if (all_wait_count++ > 15) {
        if (g_log) {
          emitTrace("fail.nbt", st->trace);
        }
        return false;
        //assert(false);
      }
    } else {
      all_wait_count = 0;
    }
    return true;
  }


  int max_num_bots_;
};

Scheduler* makeVanishScheduler(int nb) {
  return new VanishScheduler(nb);
}



#include "command.h"
#include "dep.h"
#include "matrix.h"
#include "scheduler.h"
#include "state.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>

bool g_log = false;

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

  //unique_ptr<Scheduler> scheduler(makeVoidLiftScheduler(num_bots));
  unique_ptr<Scheduler> scheduler(makeVanishScheduler(num_bots));

  Matrix source(argv[1]);
  State st(source);
  Matrix target(source.r());
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
    assert(1650005602 == st.energy);
    //assert(1772619790 == st.energy);
    //assert(1958141722 == st.energy);
  }
}
