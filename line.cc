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
#include <queue>

extern bool g_log;

class LineScheduler : public Scheduler {
  struct Line {
    bool is_line;
    int z;
    int x;
    int ox;

    bool canDo(const Matrix& m, int y) {
      if (y == 0)
        return true;

      if (is_line) {
        int min_x = x;
        int max_x = ox;
        if (min_x > max_x) swap(min_x, max_x);
        for (int x = min_x; x <= max_x; x++) {
          if (m.full(x, y-1, z) || m.full(x, y, z+1) || m.full(x, y, z-1)) {
            return true;
          }
        }
        return false;
      } else {
        if ((m.full(x, y-1, z) || m.full(x, y, z+1) || m.full(x, y, z-1)) &&
            (m.full(ox, y-1, z) || m.full(ox, y, z+1) || m.full(ox, y, z-1))) {
          return true;
        }
        return false;
      }
    }
  };

  struct Task {
    int y;
    int z;
    bool is_max;
    vector<Line*> lines;

    int min_z() const {
      for (int z = 0; z < (int)lines.size(); z++) {
        if (lines[z])
          return z;
      }
      return 999999;
    }

    bool done() const {
      return min_z() == 999999;
    }
  };

 public:
  explicit LineScheduler(int max_num_bots)
      : max_num_bots_(max_num_bots) {
    //assert(max_num_bots == 8);
  }

  virtual void schedule(const Matrix& target0,
                        State* st0) {
    target = &target0;
    st = st0;
    target->bound(&bb);
    roof_ = min(bb.max_y + 2, target->r() - 1);
    //roof_ = target->r() - 1;
    if (roof_ <= 1)
      roof_ = 2;
    if (g_log) {
      fprintf(stderr, "roof=%d (%d,%d,%d)-(%d,%d,%d)\n",
              roof_, bb.min_x, bb.min_y, bb.min_z,
              bb.max_x, bb.max_y, bb.max_z);
    }
    max_num_bots_ = bb.max_y * 2 + 2;
    assert(max_num_bots_ < 40);

    int expected_grand_total = target->total();
    assert(expected_grand_total);
    int grand_total = 0;

    bool prologue = true;

    int r = target->r();

    vector<Task*> task_by_y(max_num_bots_);

    queue<Task*> pending_tasks;
    for (int i = 0; i < max_num_bots_/2; i++) {
      for (int j = 0; j < 2; j++) {
        Task* task = new Task;
        task->y = i;
        task->z = 0;
        task->is_max = j == 0;
        pending_tasks.push(task);

        int num_line = 0;
        int y = i;
        task_by_y[y] = task;
        for (int z = 0; z < r; z++) {
          int min_x = INT_MAX;
          int max_x = 0;
          for (int x = 0; x < r; x++) {
            if (target->full(x, y, z)) {
              min_x = min(min_x, x);
              max_x = max(max_x, x);
            }
          }

          assert(max_x - min_x < 30);

          Line* line = nullptr;
          if (max_x) {
            num_line++;
            line = new Line;
            line->x = task->is_max ? max_x : min_x;
            line->ox = task->is_max ? min_x : max_x;
            line->z = z;
            bool is_line = target->full(min_x+1, y, z);
            for (int x = min_x+1; x < max_x; x++) {
              assert(is_line == target->full(x, y, z));
            }
            line->is_line = is_line;
            if (task->z == 0)
              task->z = z;
          }

          task->lines.push_back(line);

        }

        fprintf(stderr, "i=%d j=%d num_line=%d\n", i, j, num_line);
      }
    }
    assert(pending_tasks.size() == max_num_bots_);

    vector<Task*> tasks;
    for (int turn = 0; !st->done; turn++) {
      int num_bots = (int)st->bots.size();
      for (Bot* bot : st->bots) {
        if (bot->id >= (int)tasks.size()) {
          tasks.resize(bot->id + 1);
        }
      }

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

      int fusion_p = -1;
      int fusion_s = -1;
      if (pending_tasks.empty()) {
      //if (grand_total == expected_grand_total) {
        findFusion(&fusion_p, &fusion_s);
      }

      size_t orig_num_traces = st->trace.size();

      vector<bool> should_fill(num_bots);
      vector<Line*> todos(num_bots);

      for (int bi = 0; bi < num_bots; bi++) {
        Bot* bot = st->bots[bi];

        if (bi == 0 && num_bots != max_num_bots_)
          continue;

        if (!tasks[bot->id]) {
          if (bi == 0) {
            Task* t = pending_tasks.front();
            pending_tasks.pop();
            pending_tasks.push(t);
          }

          tasks[bot->id] = pending_tasks.front();
          Task* t = tasks[bot->id];
          fprintf(stderr, "task fetch bi=%d y=%d pt=%zu\n",
                  bi, t->y, pending_tasks.size());
          pending_tasks.pop();
        }

        Task* task = tasks[bot->id];
        if (!task) continue;

        Line* todo = nullptr;
        for (Line* line : task->lines) {
          if (!line)
            continue;
          if (task->y && task_by_y[task->y-1]->min_z() <= line->z)
            continue;
          if (!line->canDo(st->model, task->y))
            continue;
          todo = line;
          break;
        }

        if (!todo) {
          continue;
        }

        todos[bi] = todo;

        Bot* another = nullptr;
        for (int i = 0; i < (int)st->bots.size(); i++) {
          Task* t = tasks[st->bots[i]->id];
          if (t && t != task && task->y == t->y) {
            another = st->bots[i];
            break;
          }
        }

        fprintf(stderr, "should fill? bi=%d z=%d\n",
                bi, todo->z);

        if (another &&
            bot->p.x == todo->x &&
            bot->p.y == task->y+1 &&
            bot->p.z == todo->z &&
            another->p.x == todo->ox &&
            another->p.y == task->y+1 &&
            another->p.z == todo->z) {
          fprintf(stderr, "should fill! bi=%d todo=%p\n", bi, todo);
          should_fill[bi] = true;
        }
      }

      for (int bi = 0; bi < num_bots; bi++) {
        Bot* bot = st->bots[bi];

#if 0
        auto gotoXZAtRoof = [&](int gx, int gz) {
          return Scheduler::gotoXZAtRoof(bi, gx, gz);
        };
#endif

        if (bi == fusion_p && fusion_s >= 0) {
          Bot* sbot = st->bots[fusion_s];
          st->step(bi, FusionP(Diff(sbot->p.x, sbot->p.y-roof(), sbot->p.z)));
          continue;
        }
        if (bi == fusion_s && fusion_p >= 0) {
          st->step(bi, FusionS(Diff(-bot->p.x, roof()-bot->p.y, -bot->p.z)));
          continue;
        }

#if 0
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
#endif

        if (prologue &&
            bi == 0 && num_bots < max_num_bots_ &&
            bot->p.x == 0 && bot->p.y == 0 && bot->p.z == 0) {
          spawnToRoof2(0);
          //spawnRandomly(0);
          continue;
        }

        if (bi == 0) {
          prologue = false;
        }

        if (!tasks[bot->id]) {
          tasks[bot->id] = pending_tasks.front();
          Task* t = tasks[bot->id];
          fprintf(stderr, "task fetch bi=%d y=%d pt=%zu\n",
                  bi, t->y, pending_tasks.size());
          pending_tasks.pop();
        }

        assert(!tasks.empty());
        Task* task = tasks[bot->id];

        auto gotoXZAtRoof = [&](int gx, int gz) {
          return Scheduler::gotoXZAtRoof(bi, gx, gz);
        };

        if (task->done()) {
          if (gotoXZAtRoof(0, 0)) {
            continue;
          }
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

        if (task->y+1 != bot->p.y) {
          if (goStraight(Y, bi, task->y+1))
            continue;
          assert(false);
        }

        Line* todo = todos[bi];

#if 0
        for (Line* line : task->lines) {
          if (!line)
            continue;
          if (task->y && task_by_y[task->y-1]->min_z() <= line->z)
            continue;
          if (!line->canDo(st->model, task->y))
            continue;
          todo = line;
          break;
        }
#endif

        if (!todo) {
          st->step(bi, Wait());
          continue;
        }

        fprintf(stderr, "bi=%d todo-x,z=%d,%d\n", bi, todo->x, todo->z);

        Bot* another = nullptr;
        for (int i = 0; i < (int)st->bots.size(); i++) {
          Task* t = tasks[st->bots[i]->id];
          if (t && t != task && task->y == t->y) {
            another = st->bots[i];
            break;
          }
        }

        fprintf(stderr, "bi=%d should_fill=%d another=%p\n",
                bi, (int)should_fill[bi], another);

        if (should_fill[bi]) {
          assert(another);
          if (todo->is_line) {
            st->step(bi, GFill(Diff(0, -1, 0), todo->ox - todo->x, 0, 0));
          } else {
            st->step(bi, Fill(Diff(0, -1, 0)));
          }

          assert(task->lines[todo->z] == todo);
          task->lines[todo->z] = nullptr;

          continue;
        }

        if (!gotoXYZ(bi, todo->x, task->y+1, todo->z)) {
          st->step(bi, Wait());
        }
        continue;

#if 0
        if (bot->p.y < task->gy) {
          if (!goStraight(Y, bi, task->gy)) {
            st->step(bi, Wait());
          }
          continue;
        }

        if (gotoXYZNoAvoid(bi, task->gx, task->gy, task->gz))
          continue;
#endif

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

      if (!handleAllWait(tasks))
        return;

      st->next();
    }

 done:
    return;
  }

 private:
  bool handleAllWait(vector<Task*>& tasks) {
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

      if (all_wait_count++ > 15) {
        if (g_log) {
          int r = target->r();
          for (int y = 0; y < r; y++) {
            fprintf(stderr, "y=%d\n", y);
            for (int z = r-1; z >= 0; z--) {
              for (int x = 0; x < r; x++) {
                char c = st->model.full(x, y, z) ? 'O' : '_';
                fprintf(stderr, "%c", c);
              }
              fprintf(stderr, "\n");
            }
          }
          emitTrace("fail.nbt", st->trace);
        }
        assert(false);
      }
    } else {
      all_wait_count = 0;
    }
    return true;
  }


  int max_num_bots_;
};

Scheduler* makeLineScheduler(int nb) {
  return new LineScheduler(nb);
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
  __attribute__((unused)) bool is_test = false;
  if (argc >= 5) {
    if (string(argv[4]) == "--log") {
      g_log = true;
    }
    if (string(argv[4]) == "--test") {
      is_test = true;
    }
  }

  unique_ptr<Scheduler> scheduler(makeLineScheduler(num_bots));

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
}
