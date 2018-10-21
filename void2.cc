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

class TopTracker {
 public:
  TopTracker(const Matrix& m)
      : r(m.r()) {
    top_.resize(r * r);
    for (int x = 0; x < r; x++) {
      for (int z = 0; z < r; z++) {
        top_[x * r + z] = r;
        updateTop(m, x, r, z);
      }
    }
  }

  void updateTop(const Matrix& m, int x, int y, int z) {
    if (top_[x * r + z] != y) {
      assert(top_[x * r + z] > y);
      return;
    }

    for (y--; y >= 0; y--) {
      if (m.full(x, y, z)) {
        break;
      }
    }
    top_[x * r + z] = y;
  }

  int top(int x, int z) const {
    if (x < 0 || x >= r || z < 0 || z >= r) return -1;
    return top_[x * r + z];
  }

  int roof() const {
    int r = 0;
    for (int x = 0; x < r; x++) {
      for (int z = 0; z < r; z++) {
        r = max(r, top(x, z));
      }
    }
    return r;
  }

 private:
  vector<int> top_;
  int r;
};

class NumVoidableTracker {
 public:
  NumVoidableTracker(const Matrix& m)
      : dep(m), r(m.r()) {
    num_.resize(r * r);
    for (int x = 0; x < r; x++) {
      for (int z = 0; z < r; z++) {
        for (int y = 0; y < r; y++) {
          if (m.full(x, y, z) && dep.canRemove(Pos(x, y, z))) {
            num_[x * r + z]++;
          }
        }
      }
    }
  }

  void updateNum(const Matrix& m, int x, int y, int z) {
    Pos p(x, y, z);
    for (int d = 0; d < 6; d++) {
      Pos n(inc(p, d));
      if (m.full(n) && dep.canRemove(n)) {
        num_[n.x * r + n.z]--;
      }
    }
    dep.remove(p);
    for (int d = 0; d < 6; d++) {
      Pos n(inc(p, d));
      if (m.full(n) && dep.canRemove(n)) {
        num_[n.x * r + n.z]++;
      }
    }
  }

  int num(int x, int z) const { return num_[x * r + z]; }

  DepTracker dep;

 private:
  vector<int> num_;
  int r;
};

__attribute__((unused))
static void simulate_dive(const Matrix& m, DepTracker dep, int x, int z, int r,
                          int* min_y, int* cnt) {
  for (int y = r; y >= 0; y--) {
    bool is_blocked = false;
    for (int i = 0; i < 9; i++) {
      int ix = i / 3 - 1;
      int iz = i % 3 - 1;

      int ty = y;
      if (ix == 0 || iz == 0) {
        ty--;
      }

      Pos p(x+ix, ty, z+iz);
      if (m.full(p)) {
        if (dep.canRemove(p)) {
          ++*cnt;
        } else {
          if (ix == 0 && iz == 0)
            is_blocked = true;
        }
      }
    }

    if (is_blocked) {
      *min_y = y;
      return;
    }
  }
  *min_y = 0;
  return;
}

class VoidTopScheduler : public Scheduler {
  struct Task {
    int x, y, z;
  };

 public:
  explicit VoidTopScheduler(int max_num_bots)
      : max_num_bots_(max_num_bots) {
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

    TopTracker top(st->model);
    NumVoidableTracker voidable(st->model);
    const DepTracker& dep = voidable.dep;
    int grand_total = st->model.total();
    assert(grand_total);

    bool prologue = true;
    //bool epilogue = false;
    int assignable_tasks = 99999;
    vector<Task*> tasks;
    vector<Task*> all_tasks;

    for (int turn = 0; !st->done; turn++) {
#if 0
      if (g_log) {
        fprintf(stderr, "turn=%d total=%d\n", turn, grand_total);
      }
#endif

      int num_bots = (int)st->bots.size();
      for (Bot* bot : st->bots) {
        if (bot->id >= (int)tasks.size()) {
          tasks.resize(bot->id + 1);
        }
      }

      for (Task* task : all_tasks) {
        delete task;
      }
      all_tasks.clear();

      for (Task*& task : tasks) {
        delete task;
        task = nullptr;
      }

      for (int x = bb.min_x-1; x <= bb.max_x+1; x++) {
        for (int z = bb.min_z-1; z <= bb.max_z+1; z++) {
          int y = top.top(x, z);
          if (y < 0) {
            continue;
          }

          if (dep.canRemove(Pos(x, y, z))) {
            Task* task = new Task;
            task->x = x;
            task->y = y;
            task->z = z;
            all_tasks.push_back(task);
          }
        }
      }

      assignable_tasks = (int)all_tasks.size();

      if (assignable_tasks == 0 && grand_total) {
        if (g_log) {
          for (int y = roof(); y >= 0; y--) {
            fprintf(stderr, "y=%d\n", y);
            for (int x = 0; x <= st->model.r(); x++) {
              for (int z = 0; z <= st->model.r(); z++) {
                char c = st->model.full(x, y, z) ? 'O' : '_';
                if (c == 'O' && dep.canRemove(Pos(x, y, z))) c = 'C';
                fprintf(stderr, "%c", c);
              }
              fprintf(stderr, "\n");
            }
          }
        }
        assert(false);
      }

      for (Bot* bot : st->bots) {
        int best_score = INT_MAX;
        size_t best_index = string::npos;
        for (size_t i = 0; i < all_tasks.size(); i++) {
          Task* task = all_tasks[i];
          if (!task) continue;
          int d = abs(bot->p.x - task->x) + abs(bot->p.z - task->z);
          if (best_score > d) {
            best_score = d;
            best_index = i;
          }
        }

        if (best_index != string::npos) {
          tasks[bot->id] = all_tasks[best_index];
          all_tasks[best_index] = nullptr;
        }
      }

      int fusion_p = -1;
      int fusion_s = -1;
      if (assignable_tasks < num_bots) {
        findFusion(&fusion_p, &fusion_s);
      }

      size_t orig_num_traces = st->trace.size();
      for (int bi = 0; bi < num_bots; bi++) {
        Bot* bot = st->bots[bi];
        int id = bot->id;

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

        if (!tasks[id] && assignable_tasks < num_bots) {
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
        }

        if (prologue && assignable_tasks >= num_bots &&
            bi == 0 && num_bots < max_num_bots_ &&
            bot->p.x == 0 && bot->p.y == 0 && bot->p.z == 0) {
          spawnToRoof(0);
          continue;
        }

        if (bi == 0) {
          prologue = false;
        }

        Task* task = tasks[id];
        if (!task) {
          st->step(bi, Wait());
          continue;
        }

        int ax = task->x;
        int ay = task->y;
        int az = task->z;
        Diff diff(ax - bot->p.x, ay - bot->p.y, az - bot->p.z);
        int diff_cnt = diff.nonZeroCount();
        if (diff.isSmall(1) && diff_cnt > 0 && diff_cnt < 3) {
          Command cmd = Void(diff);
          State::MoveSt mst = st->check_move(bi, cmd);
          if (mst == State::OK) {
            st->step(bi, cmd);
            top.updateTop(st->model, ax, ay, az);
            voidable.updateNum(st->model, ax, ay, az);
            grand_total--;
          } else {
            assert(false);
          }
          continue;
        }

        if (gotoXYZ(bi, ax, ay+1, az)) {
          continue;
        }

        assert(false);
      }

      int num_new_traces = st->trace.size() - orig_num_traces;
      if (g_log) {
        fprintf(stderr,
                "turn=%d num_bots=%d total=%d num_new_traces=%d tasks=%d\n",
                turn, num_bots, grand_total, num_new_traces,
                assignable_tasks);
        for (int i = 0; i < num_new_traces; i++) {
          int ax = 0;
          int ay = 0;
          int az = 0;
          Task* task = tasks[st->bots[i]->id];
          if (task) {
            ax = task->x;
            ay = task->y;
            az = task->z;
          }
          Command cmd = st->trace[st->trace.size() - num_new_traces + i];
          fprintf(stderr, "i=%d %s at %s ax=%d ay=%d az=%d\n",
                  i,
                  cmd.str().c_str(),
                  st->bots[i]->p.str().c_str(), ax, ay, az);

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

Scheduler* makeVoidTopScheduler(int nb) {
  return new VoidTopScheduler(nb);
}
