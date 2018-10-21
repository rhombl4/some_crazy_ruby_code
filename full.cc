#include "command.h"
#include "matrix.h"
#include "scheduler.h"
#include "state.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>

#include <map>
#include <queue>

extern bool g_log;

static int g_t_off_x = 1;
static int g_t_off_z = 1;

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

class LiftScheduler : public Scheduler {
  struct Task {
    int x, z;
    int total;
    int bottom;
  };

 public:
  explicit LiftScheduler(int max_num_bots)
      : max_num_bots_(max_num_bots) {
    if (getenv("G_T_OFF_X")) g_t_off_x = atoi(getenv("G_T_OFF_X"));
    if (getenv("G_T_OFF_Z")) g_t_off_z = atoi(getenv("G_T_OFF_Z"));
  }

  virtual void schedule(const Matrix& target0,
                        State* st0) {
    target = &target0;
    st = st0;
    target->bound(&bb);
    //roof_ = target->r() - 1;
    roof_ = bb.max_y + 1;

    // 0: not flipped, 1: flipping, 2: flipped
    int flipped = 0;
    Assignable assignable(target0);
    int grand_total = 0;

    queue<Task*> pending_tasks;
    for (int x = bb.min_x + g_t_off_x; x <= bb.max_x + 1; x += 3) {
      for (int z = bb.min_z + g_t_off_z; z <= bb.max_z + 1; z += 3) {
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

    if (g_log) {
      fprintf(stderr, "grand_total=%d\n", grand_total);
    }

    int has_finished_bot = 0;
    bool epilogue = false;

    vector<Task*> tasks;
    for (int turn = 0; !st->done; turn++) {
      int num_bots = (int)st->bots.size();
      tasks.resize(num_bots);

      int fusion_p = -1;
      int fusion_s = -1;
      if (pending_tasks.empty()) {
        findFusion(&fusion_p, &fusion_s);
      }

      int fusing_task_index = -1;
      size_t orig_num_traces = st->trace.size();
      for (int bi = 0; bi < num_bots; bi++) {
        if (flipped == 1) {
          st->step(bi, Flip());
          flipped = 2;
          continue;
        }

        if (!has_finished_bot && bi == 0 && num_bots < max_num_bots_) {
          spawnToRoof(0);
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
          return Scheduler::gotoXZAtRoof(bi, gx, gz);
        };

        // Merging.
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
              goto done;
            } else {
              st->step(bi, Wait());
              continue;
            }
          }
        }

        auto find_assignable = [&](Task* task, int& ax, int& ay, int& az) {
          ax = 0;
          ay = -1;
          az = 0;
          int has_remaining_y = roof() + 1;
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
                  int ry = y + (ix && iz ? 0 : 1);
                  has_remaining_y = min(has_remaining_y, ry);
                }
              }
            }

            int x = task->x;
            int z = task->z;
            if (ay < 0 &&
                target->full(x, y, z) &&
                !st->model.full(x, y, z)) {
              if (assignable.full(x, y, z)) {
                if (has_remaining_y <= y)
                  continue;
                ax = x;
                ay = y;
                az = z;
                break;
              } else {
                has_remaining_y = min(has_remaining_y, y);
              }
            }

            if (ay >= 0)
              break;
          }
        };

        // Assign a task.
        if (!tasks[bi]) {
          for (size_t c = 0; c < pending_tasks.size(); c++) {
            Task* task = pending_tasks.front();
            pending_tasks.pop();
            int ax, ay, az;
            find_assignable(task, ax, ay, az);
            if (ay >= 0) {
              tasks[bi] = task;
              break;
            } else {
              pending_tasks.push(task);
            }
          }

          if (!tasks[bi]) {
            st->step(bi, Wait());
            continue;
          }
        }
        Task* task = tasks[bi];

        // Find assignable.
        int ax, ay, az;
        find_assignable(task, ax, ay, az);

        if (ay == -1) {
          tasks[bi] = nullptr;
          // TODO: waste.
          if (g_log) {
            fprintf(stderr, "no assignable\n");
          }
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

        Diff diff(ax - bot->p.x, ay - bot->p.y, az - bot->p.z);
        int diff_cnt = diff.nonZeroCount();
        if (diff.isSmall(1) &&
            diff_cnt > 0 && diff_cnt < 3 &&
            !(diff.x == 0 && diff.z == 0 && diff.y != -1)) {
          Command cmd = Fill(diff);
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

        int gy = ay;
        if (ax == task->x || az == task->z)
          gy++;

        if (goStraight(Y, bi, gy)) {
          continue;
        }

#if 0
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
#endif

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

      handleAllWait(tasks, pending_tasks, assignable);

      st->next();

      if (fusing_task_index >= 0) {
        int idx = fusing_task_index;
        assert(idx < (int)tasks.size());
        tasks.erase(tasks.begin() + idx);
      }
    }

 done:
    return;
  }

 private:
  void handleAllWait(vector<Task*>& tasks,
                     queue<Task*>& pending_tasks,
                     const Assignable& assignable) {
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
          swap(tasks[bi1], tasks[bi2]);
        }
      }

#if 0
      if (all_wait_count > 3 && flipped == 0) {
        //flipped = 1;
      }
#endif

      if (all_wait_count++ > 15) {
        if (g_log) {
          while (!pending_tasks.empty()) {
            Task* task = pending_tasks.front();
            pending_tasks.pop();
            fprintf(stderr, "TASK: %d %d\n", task->x, task->z);
          }
          for (int x = 0; x < target->r(); x++) {
            for (int y = 0; y < target->r(); y++) {
              for (int z = 0; z < target->r(); z++) {
                if (target->full(x, y, z) &&
                    !st->model.full(x, y, z)) {
                  fprintf(stderr, "TODO: (%d,%d,%d) %d\n",
                          x, y, z, assignable.full(x, y, z));
                }
              }
            }
          }

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

Scheduler* makeLiftScheduler(int nb) {
  return new LiftScheduler(nb);
}
