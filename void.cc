#include "command.h"
#include "dep.h"
#include "matrix.h"
#include "scheduler.h"
#include "state.h"
#include "util.h"

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

class VoidLiftScheduler : public Scheduler {
  struct Task {
    int x, z;
  };

 public:
  explicit VoidLiftScheduler(int max_num_bots)
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
    bool epilogue = false;
    vector<Task*> tasks;
    int assignable_tasks = 99999;

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
            epilogue = true;
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

        // Head to the roof.
        if (!epilogue && !tasks[id] && goStraight(Y, bi, roof())) {
          continue;
        }

        bool is_assigned_now = false;
        if (!tasks[id]) {
          is_assigned_now = true;
          assignable_tasks = 0;
          int best_score = 0;
          unique_ptr<Task> best_task;

#if 0
          for (int x = bb.min_x-1; x <= bb.max_x+1; x++) {
            for (int z = bb.min_z-1; z <= bb.max_z+1; z++) {
              bool has_someone = false;
              for (Task* task : tasks) {
                if (task && task->x == x && task->z == z) {
                  has_someone = true;
                  break;
                }
              }
              if (has_someone)
                continue;

              int min_y = 0;
              int cnt = 0;
              simulate_dive(st->model, dep, x, z, roof() + 1, &min_y, &cnt);

              if (g_log) {
                fprintf(stderr, "SIM x=%d z=%d min_y=%d cnt=%d\n",
                        x, z, min_y, cnt);
              }

              if (cnt) {
                assignable_tasks++;
                int score = (roof() + 1 - min_y) * 1000 + cnt;
                if (x % 3 == 1 && z % 3 == 1) {
                  score += 500;
                }
                if (best_score < score) {
                  best_score = score;
                  Task* task = new Task;
                  task->x = x;
                  task->z = z;
                  best_task.reset(task);
                }
              }
            }
          }

#else
          for (int x = bb.min_x-1; x <= bb.max_x+1; x++) {
            for (int z = bb.min_z-1; z <= bb.max_z+1; z++) {
              bool has_someone = false;
              for (Task* task : tasks) {
                if (task && task->x == x && task->z == z) {
                  has_someone = true;
                  break;
                }
              }
              if (has_someone)
                continue;

              int num_removable = 0;
              int ty = top.top(x, z);
              if (ty >= 0) {
                assert(st->model.full(x, ty, z));
                if (dep.canRemove(Pos(x, ty, z))) {
                  num_removable++;
                }
              }
              for (int i = 0; i < 9; i++) {
                int ix = i / 3 - 1;
                int iz = i % 3 - 1;
                if (ix == 0 && iz == 0)
                  continue;
                if (voidable.num(x+ix, z+iz) == 0)
                  continue;

                int y = top.top(x, z);
                if (ix && iz)
                  y++;
                int ty = top.top(x+ix, z+iz);
                for (; y <= ty; y++) {
                  assert(y < 999);
                  if (st->model.full(x+ix, y, z+iz) &&
                      dep.canRemove(Pos(x+ix, y, z+iz))) {
                    num_removable++;
                    break;
                  }
                }
              }

              if (num_removable) {
                assignable_tasks++;
              }

              int score = num_removable;
              if (x % 3 == 1 && z % 3 == 1) {
                score += 50;
              }

              if (num_removable && best_score < score) {
                best_score = score;
                Task* task = new Task;
                task->x = x;
                task->z = z;
                best_task.reset(task);
              }
            }
          }
#endif

          if (best_task.get()) {
            tasks[id] = best_task.release();

            if (g_log) {
              fprintf(stderr, "assign task=(%d,%d) for %d\n",
                      tasks[id]->x, tasks[id]->z, bi);

              for (int y = roof(); y >= 0; y--) {
                fprintf(stderr, "y=%d\n", y);
                for (int x = tasks[id]->x - 1; x <= tasks[id]->x + 1; x++) {
                  for (int z = tasks[id]->z - 1; z <= tasks[id]->z + 1; z++) {
                    char c = st->model.full(x, y, z) ? 'O' : '_';
                    if (c == 'O' && dep.canRemove(Pos(x, y, z))) c = 'C';
                    fprintf(stderr, "%c", c);
                  }
                  fprintf(stderr, "\n");
                }
              }
            }
          } else {
            st->step(bi, Wait());
            continue;
          }
        }

        Task* task = tasks[id];
        int ax = 0, ay = -1, az = 0;
        for (int y = roof()+1; y >= 0; y--) {
          if (st->model.full(task->x, y, task->z))
            break;
          for (int i = 0; i < 9; i++) {
            int ix = i / 3 - 1;
            int iz = i % 3 - 1;
            int x = task->x + ix;
            int z = task->z + iz;

            int ty = y;
            if (ix == 0 || iz == 0) {
              ty--;
            }

            if (st->model.full(x, ty, z) && dep.canRemove(Pos(x, ty, z))) {
              ax = x;
              ay = ty;
              az = z;
              break;
            }

          }
        }

        if (ay == -1) {
          assert(!is_assigned_now);
          delete task;
          tasks[id] = nullptr;
          // TODO: waste.
          st->step(bi, Wait());
          continue;
        }

        // Adjust X-Z coordinates.
        if (gotoXZAtRoof(task->x, task->z)) {
          continue;
        }

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

        int gy = ay;
        if (ax == task->x && az == task->z) {
          gy++;
        } else if (ax == task->x || az == task->z) {
          gy++;
          if (!st->model.full(task->x, gy-1, task->z))
            gy--;
          if (!st->model.full(task->x, gy-1, task->z))
            gy--;
          if (gy < 0)
            gy = 0;
        }

        if (gotoXYZ(bi, task->x, gy, task->z))
          continue;

        if (g_log) {
          fprintf(stderr, "bi=%d x=%d z=%d ax=%d gy=%d az=%d\n",
                  bi, task->x, task->z, ax, gy, az);
        }
        if (st->model.full(task->x, gy, task->z)) {
          for (int y = roof(); y >= 0; y--) {
            fprintf(stderr, "y=%d\n", y);
            for (int x = tasks[id]->x - 1; x <= tasks[id]->x + 1; x++) {
              for (int z = tasks[id]->z - 1; z <= tasks[id]->z + 1; z++) {
                char c = st->model.full(x, y, z) ? 'O' : '_';
                if (c == 'O' && dep.canRemove(Pos(x, y, z))) c = 'C';
                fprintf(stderr, "%c", c);
              }
              fprintf(stderr, "\n");
            }
          }
          assert(false);
        }

        if (goStraight(Y, bi, gy)) {
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
          int az = 0;
          Task* task = tasks[st->bots[i]->id];
          if (task) {
            ax = task->x;
            az = task->z;
          }
          Command cmd = st->trace[st->trace.size() - num_new_traces + i];
          fprintf(stderr, "i=%d %s at %s ax=%d az=%d\n",
                  i,
                  cmd.str().c_str(),
                  st->bots[i]->p.str().c_str(), ax, az);

          if (cmd.op == FILL) {
            Pos n = applyNear(st->bots[i]->p, cmd.d);
            fprintf(stderr, "FILL! at %s by %d\n",
                    n.str().c_str(), i);
          }
        }
      }
      assert(num_bots == num_new_traces);

      handleAllWait(tasks);

      st->next();
    }

 done:
    return;
  }

 private:
  void handleAllWait(vector<Task*>& tasks) {
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
      for (int c = 0; c < 10; c++) {
        int bi1 = rnd(num_bots);
        int bi2 = rnd(num_bots);
        if (bi1 == bi2) continue;
        Bot* b1 = st->bots[bi1];
        Bot* b2 = st->bots[bi2];
        if (b1->p.y == roof() && b2->p.y == roof()) {
          swap(tasks[b1->id], tasks[b2->id]);
        }
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

Scheduler* makeVoidLiftScheduler(int nb) {
  return new VoidLiftScheduler(nb);
}
