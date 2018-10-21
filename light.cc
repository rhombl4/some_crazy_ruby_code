#include "command.h"
#include "matrix.h"
#include "state.h"

#include <stdio.h>
#include <stdlib.h>

#include <map>
#include <queue>

bool g_log = false;

unsigned long xor128() {
  static unsigned long x=123456789, y=362436069, z=521288629, w=88675123;
  unsigned long t=(x^(x<<11));
  x=y; y=z; z=w;
  return w=(w^(w>>19))^(t^(t>>8));
}

int rnd(int v) {
  return xor128() % v;
}

class Assignable : public Matrix {
 public:
  explicit Assignable(const Matrix& m)
      : Matrix(m.r()), ref(m) {
    for (int x = 0; x < r(); x++) {
      for (int z = 0; z < r(); z++) {
        if (m.full(x, 0, z)) {
          fill(x, 0, z);
        }
      }
    }
  }

  void ref_filled(int x, int y, int z) {
    for (int i = -1; i <= 1; i++) {
      if (ref.full(x+i, y, z)) fill(x+i, y, z);
      if (ref.full(x, y+i, z)) fill(x, y+i, z);
      if (ref.full(x, y, z+i)) fill(x, y, z+i);
    }
  }

 private:
  const Matrix& ref;
};

class Scheduler {
 public:
  virtual void schedule(const Matrix& target,
                        State* st,
                        vector<Command>* trace) = 0;

  bool goStraight(Dir dir, int bi, int g, bool wait=true) {
    Bot* bot = st->bots[bi];
    int c = bot->p.val(dir);
    if (c == g) {
      return false;
    }

    int d = max(-15, min(15, g - c));
    Command cmd = SMove(Diff(dir, d));
    int ok_dist = 0;
    State::MoveSt mst = st->check_move(bi, cmd, &ok_dist);
    if (mst == State::TEMP) {
      if (wait) {
        if (ok_dist) {
          d = d >= 0 ? ok_dist : -ok_dist;
          Command cmd = SMove(Diff(dir, d));
          st->step(bi, cmd);
        } else {
          st->step(bi, Wait());
        }
      } else {
        return false;
      }
    } else if (mst == State::OK) {
      st->step(bi, cmd);
    } else {
      fprintf(stderr, "mst=%d\n", mst);
      assert(false);
    }
    return true;
  }

 protected:
  State* st;

};

class LiftScheduler : public Scheduler {
  struct Task {
    int x, z;
    int total;
    int bottom;
  };

 public:
  explicit LiftScheduler(int max_num_bots)
      : max_num_bots_(max_num_bots) {
  }

  virtual void schedule(const Matrix& target0,
                        State* st0,
                        vector<Command>* trace0) {
    target = &target0;
    st = st0;
    trace = trace0;
    target->bound(&bb);

    Assignable assignable(target0);
    int grand_total = 0;

    queue<Task*> pending_tasks;
    for (int x = bb.min_x + 1; x <= bb.max_x + 1; x += 3) {
      for (int z = bb.min_z + 1; z <= bb.max_z + 1; z += 3) {
        int total = 0;
        for (int y = 0; y <= bb.max_y; y++) {
          for (int i = 0; i < 9; i++) {
            int ix = i / 3 - 1;
            int iz = i % 3 - 1;
            if (target->full(x + ix, y, z + iz)) {
              total++;
              grand_total++;
            }
          }
        }
        if (total == 0)
          continue;

        Task* task = new Task;
        task->x = x;
        task->z = z;
        task->total = total;
        task->bottom = 0;

#if 0
        int base[9];
        int cur[9];
        for (int i = 0; i < 9; i++) {
          cur[i] = 2;
        }

        for (int y = 0; y <= bb.max_y; y++) {
          for (int i = 0; i < 9; i++) {
            base[i] = cur[i];
            cur[i] = 0;
          }
        }
#endif

        pending_tasks.push(task);
      }
    }

    fprintf(stderr, "grand_total=%d\n", grand_total);

    int has_finished_bot = 0;
    bool epilogue = false;

    vector<Task*> tasks;
    for (int turn = 0; !st->done; turn++) {
      int num_bots = (int)st->bots.size();
      tasks.resize(num_bots);

      int fusion_p = -1;
      int fusion_s = -1;
      if (pending_tasks.empty()) {
        for (int bi = 0; bi < num_bots; bi++) {
          Bot* bot = st->bots[bi];
          int x = bot->p.x;
          int y = bot->p.y;
          int z = bot->p.z;
          if (x == 0 && y == roof() && z == 0) {
            fusion_p = bi;
          } else if ((x == 0 && y == roof() && z == 1) ||
                     (x == 1 && y == roof() && z == 0) ||
                     (x == 0 && y == roof() - 1 && z == 0)) {
            fusion_s = bi;
          }
        }
      }

      int fusing_task_index = -1;
      size_t orig_num_traces = st->trace.size();
      for (int bi = 0; bi < num_bots; bi++) {
        if (!has_finished_bot && bi == 0 && num_bots < max_num_bots_) {
          Command cmd = Wait();
          for (int i = 0; i < 3; i++) {
            Command c = Wait();
            if (i == 0) {
              c = Fission(Diff(1, 1, 0), 0);
            } else if (i == 1) {
              c = Fission(Diff(0, 1, 0), 0);
            } else {
              c = Fission(Diff(0, 1, 1), 0);
            }
            if (c.op != WAIT && st->check_move(bi, c) == State::OK) {
              cmd = c;
              break;
            }
          }
          st->step(bi, cmd);
          continue;
        }

        Bot* bot = st->bots[bi];

        if (bi == fusion_p && fusion_s >= 0) {
          Bot* sbot = st->bots[fusion_s];
          st->step(bi, FusionP(Diff(sbot->p.x, sbot->p.y-roof(), sbot->p.z)));
          continue;
        }
        if (bi == fusion_s && fusion_p >= 0) {
          st->step(bi, FusionS(Diff(-bot->p.x, roof()-bot->p.y, -bot->p.z)));
          fusing_task_index = bi;
          continue;
        }

        // Head to the roof.
        if (!epilogue && !tasks[bi] && goStraight(Y, bi, roof())) {
          continue;
        }

        auto gotoXZAtRoof = [&](int gx, int gz) {
          if (gx == bot->p.x && gz == bot->p.z)
            return false;

          if (goStraight(Y, bi, roof()))
            return true;

          //assert(bot->p.y == roof());
          if (rnd(2) == 0) {
            if (gx != bot->p.x && goStraight(X, bi, gx, false)) {
              return true;
            }
            if (gz != bot->p.z && goStraight(Z, bi, gz, false)) {
              return true;
            }
            if (gx != bot->p.x && goStraight(X, bi, gx)) {
              return true;
            }
            if (gz != bot->p.z && goStraight(Z, bi, gz)) {
              return true;
            }
          } else {
            if (gz != bot->p.z && goStraight(Z, bi, gz, false)) {
              return true;
            }
            if (gx != bot->p.x && goStraight(X, bi, gx, false)) {
              return true;
            }
            if (gz != bot->p.z && goStraight(Z, bi, gz)) {
              return true;
            }
            if (gx != bot->p.x && goStraight(X, bi, gx)) {
              return true;
            }
          }
          assert(false);
          st->step(bi, Wait());
          return true;
        };

        // Assign a task.
        if (!tasks[bi]) {
          if (pending_tasks.empty()) {
            if (has_finished_bot && has_finished_bot != bot->id) {
              if (gotoXZAtRoof(0, 0)) {
                continue;
              }
              st->step(bi, Wait());
              continue;
            } else {
              if (gotoXZAtRoof(0, 0)) {
                continue;
              }
            }

            if (num_bots == 1) {
              epilogue = true;
              if (goStraight(Y, bi, 0)) {
                continue;
              }
              assert(bot->p.x == 0);
              assert(bot->p.y == 0);
              assert(bot->p.z == 0);
              st->step(bi, Halt());
              break;

            } else {
              st->step(bi, Wait());
              continue;
            }

          } else {
            Task* task = pending_tasks.front();
            pending_tasks.pop();
            tasks[bi] = task;
          }
        }
        Task* task = tasks[bi];

        // Find assignable.
        int ax = 0;
        int ay = -1;
        int az = 0;
        bool has_remaining = false;
        for (int y = task->bottom; y < target->r(); y++) {
          for (int i = 0; i < 9; i++) {
            int ix = i / 3 - 1;
            int iz = i % 3 - 1;
            if (ix == 0 && iz == 0)
              continue;
            int x = task->x + ix;
            int z = task->z + iz;
            if (target->full(x, y, z) &&
                !st->model.full(x, y, z)) {
              if (assignable.full(x, y, z)) {
                ax = x;
                ay = y;
                az = z;
                break;
              } else {
                has_remaining = true;
              }
            }
          }

          int x = task->x;
          int z = task->z;
          if (ay < 0 &&
              target->full(x, y, z) &&
              !st->model.full(x, y, z)) {
            if (assignable.full(x, y, z)) {
              if (has_remaining)
                continue;
              ax = x;
              ay = y;
              az = z;
              break;
            } else {
              has_remaining = true;
            }
          }

          if (ay >= 0)
            break;
        }

        if (ay == -1) {
          tasks[bi] = nullptr;
          // TODO: waste.
          //fprintf(stderr, "no assignable\n");
          st->step(bi, Wait());
          if (task->total == 0) {
            //fprintf(stderr, "task x=%d z=%d done\n", task->x, task->z);
            delete task;
          } else {
            pending_tasks.push(task);
          }
          continue;
        }

        // Adjust X-Z coordinates.
        if (gotoXZAtRoof(task->x, task->z)) {
          continue;
        }

        int gy = ay;
        if (ax == task->x && az == task->z)
          gy++;

        if (goStraight(Y, bi, gy, false)) {
          continue;
        }

        // Fill it now.
        //if (ay - bot->p.y == 1 &&
        //    !(ax == task->x && az == task->z && ay == bot->p.y)) {
        //if (ay == bot->p.y) {
        if (true) {
          Command cmd = Fill(Diff(ax - bot->p.x, ay - bot->p.y, az - bot->p.z));
          State::MoveSt mst = st->check_move(bi, cmd);
          if (mst == State::OK) {
            st->step(bi, cmd);
            assignable.ref_filled(ax, ay, az);
            task->total--;
            grand_total--;
          } else {
            assert(false);
          }
          continue;
        }

        assert(false);

      }

      //request_merge--;

      for (Bot* bot : st->bots) {
        if (bot->p.x == 0 && bot->p.z == 0 && pending_tasks.empty()) {
#if 0
          fprintf(stderr, "No more pending tasks! id=%d grand_total=%d\n",
                  bot->id, grand_total);
#endif
          has_finished_bot = bot->id;
        }
      }

      int num_new_traces = st->trace.size() - orig_num_traces;
      if (g_log) {
        fprintf(stderr,
                "turn=%d num_bots=%d num_new_traces=%d num_pending=%zu\n",
                turn, num_bots, num_new_traces, pending_tasks.size());
        for (int i = 0; i < num_new_traces; i++) {
          int ax = 0;
          int az = 0;
          Task* task = tasks[i];
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

      static int all_wait_count = 0;
      bool all_wait = true;
      for (int i = 0; i < num_new_traces; i++) {
        Command cmd = st->trace[st->trace.size() - num_new_traces + i];
        if (cmd.op != WAIT) {
          all_wait = false;
        }
      }
      if (all_wait) {
        for (int c = 0; c < 10; c++) {
          int bi1 = rnd(num_bots);
          int bi2 = rnd(num_bots);
          if (bi1 == bi2) continue;
          Bot* b1 = st->bots[bi1];
          Bot* b2 = st->bots[bi2];
          if (b1->p.y == roof() && b2->p.y == roof()) {
            swap(tasks[bi1], tasks[bi2]);
          }
        }

        if (all_wait_count++ > 10) {
          assert(false);
        }
      } else {
        all_wait_count = 0;
      }

      st->next();

      if (fusing_task_index >= 0) {
        int idx = fusing_task_index;
        assert(idx < (int)tasks.size());
        tasks.erase(tasks.begin() + idx);
      }
    }

    *trace = st->trace;
    fprintf(stderr, "energy=%zd\n", st->energy);
  }

 private:
  int roof() const { return target->r() - 1; }

  int max_num_bots_;
  const Matrix* target;
  vector<Command>* trace;
  BoundingBox bb;
};

int main(int argc, const char* argv[]) {
  if (argc <= 1) {
    fprintf(stderr, "Usage: %s mdl\n", argv[0]);
    exit(1);
  }

  int num_bots = 1;
  if (argc >= 4) {
    num_bots = atoi(argv[3]);
  }
  if (argc >= 5 && string(argv[4]) == "--log") {
    g_log = true;
  }
  LiftScheduler scheduler(num_bots);

  Matrix target(argv[1]);
  State st(target.r());
  vector<Command> trace;
  scheduler.schedule(target, &st, &trace);

  if (argc >= 3) {
    FILE* fp = fopen(argv[2], "wb");
    for (const Command& c : trace) {
      c.emit(fp);
    }
    fclose(fp);
  } else {
    for (const Command& c : trace) {
      printf("%s\n", c.str().c_str());
    }
  }
}
