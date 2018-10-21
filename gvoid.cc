#include "command.h"
#include "dep.h"
#include "matrix.h"
#include "scheduler.h"
#include "state.h"
#include "util.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

extern bool g_log;

static Pos inc(Pos p, int d) {
  switch (d) {
    case 0: p.x++; break;
    case 1: p.x--; break;
    case 2: p.y++; break;
    case 3: p.y--; break;
    case 4: p.z++; break;
    case 5: p.z--; break;
    default: assert(false);
  }
  return p;
}

#define NO_MAIN
#include "gvoid_sim.cc"

class GVoidSliceScheduler : public Scheduler {
  struct Task {
    int x, y, z;
  };

 public:
  explicit GVoidSliceScheduler(int max_num_bots)
      : max_num_bots_(max_num_bots) {
    assert(max_num_bots == 4);
  }

  virtual void schedule(const Matrix& target0,
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

    vector<Plan> plans;
    GVoidSim sim(st->model);
    if (sim.makePlan(&plans) != 0) {
      assert(false);
    }

    int grand_total = st->model.total();
    assert(grand_total);

    bool prologue = true;

    for (int turn = 0; !st->done; turn++) {
      grand_total = st->model.total();
#if 0
      if (g_log) {
        fprintf(stderr, "turn=%d total=%d\n", turn, grand_total);
      }
#endif

      if (plans.empty() && grand_total) {
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
      if (plans.empty()) {
        findFusion(&fusion_p, &fusion_s);
      }

      size_t orig_num_traces = st->trace.size();

      bool issue_gvoid = false;
      if (!plans.empty()) {
        bool all_ok = true;
        for (int bi = 0; bi < 4; bi++) {
          Bot* bot = st->bots[bi];
          Plan plan = plans[0];
          Pos p(plan.x, plan.y, plan.z);
          if (bi == 1) {
            p.x += plan.dx;
            p.z += plan.dz;
          } else if (bi == 2) {
            p.x += plan.dx;
          } else if (bi == 3) {
            p.z += plan.dz;
          }
          p.y++;
          if (bot->p != p) {
            all_ok = false;
            break;
          }
        }

        if (all_ok) {
          issue_gvoid = true;
          for (int bi = 0; bi < 4; bi++) {
            Diff nd(0, -1, 0);
            int x = plans[0].dx;
            int z = plans[0].dz;
            if (bi == 1) {
              x = -plans[0].dx;
              z = -plans[0].dz;
            } else if (bi == 2) {
              x = -plans[0].dx;
            } else if (bi == 3) {
              z = -plans[0].dz;
            }
            st->step(bi, GVoid(nd, x, 0, z));
          }
          plans.erase(plans.begin());
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

        if (plans.empty()) {
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
          continue;
        }

        if (bi == 0) {
          prologue = false;
        }

        assert(!plans.empty());
        Plan plan = plans[0];
        Pos p(plan.x, plan.y, plan.z);
        if (bi == 1) {
          p.x += plan.dx;
          p.z += plan.dz;
        } else if (bi == 2) {
          p.x += plan.dx;
        } else if (bi == 3) {
          p.z += plan.dz;
        }

        if (gotoXYZ(bi, p.x, p.y+1, p.z))
          continue;

        st->step(bi, Wait());
      }

      int num_new_traces = st->trace.size() - orig_num_traces;
      if (g_log) {
        fprintf(stderr,
                "turn=%d num_bots=%d total=%d num_new_traces=%d tasks=%d\n",
                turn, num_bots, grand_total, num_new_traces,
                (int)plans.size());
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

      handleAllWait();

      st->next();
    }

 done:
    return;
  }

 private:
  void handleAllWait(/*vector<Task*>& tasks*/) {
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
        assert(false);
      }
    } else {
      all_wait_count = 0;
    }
  }


  int max_num_bots_;
};

Scheduler* makeGVoidSliceScheduler(int nb) {
  return new GVoidSliceScheduler(nb);
}
