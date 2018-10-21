
class FlippableScheduler : public Scheduler {
  struct Task {
    int x, y, z;
    vector<Pos> p;
    size_t i;

    Task() : x(-1), y(-1), z(-1), i(0) {}
    bool valid() const { return y >= 0; }
  };

 public:
  FlippableScheduler(int max_num_bots, int stride)
      : max_num_bots_(max_num_bots), stride_(stride) {
  }

  virtual void schedule(const Matrix& target0,
                        State* st0,
                        vector<Command>* trace0) {
    target = &target0;
    st = st0;
    trace = trace0;
    target->bound(&bb);
    target->rowBound(&row_bbs);
    flipped = false;

    for (int y = 0; y <= bb.max_y + 1; y++) {
      for (int z = 1; z < target->r(); z += stride_) {
        unique_ptr<Task> task(new Task());
        task->y = y;
        task->z = z;
        for (int x = 0; x < target->r(); x++) {
          for (int dz = -1; dz < stride_ - 1; dz++) {
            if (dz != 0) {
              Pos p(x, y, z+dz);
              if (target->full(p)) {
                task->p.push_back(p);
              }
            }
          }
          if (y > 0) {
            Pos p(x, y - 1, z);
            if (target->full(p)) {
              task->p.push_back(p);
            }
          }
        }
        if (!task->p.empty()) {
          Task* t = task.release();
          pending_tasks.push(t);
          task_by_lane.emplace(make_pair(t->y, t->z), t);
        }
      }

    }

    /*
    int lx = bb.max_x - bb.min_x + 1;
    int ly = bb.max_y - bb.min_y + 1;
    int lz = bb.max_z - bb.min_z + 1;
    */

    for (int turn = 0; !st->done; turn++) {
      int num_bots = (int)st->bots.size();
      tasks.resize(num_bots);

      size_t orig_num_traces = st->trace.size();
      for (int bi = 0; bi < num_bots; bi++) {
        if (bi == 0 && num_bots < max_num_bots_) {
          if (num_bots % 2 == 1) {
            st->step(bi, Fission(Diff(1, 0, 1), 0));
          } else {
            st->step(bi, Fission(Diff(0, 0, 1), 0));
          }
          continue;
        }

        if (!tasks[bi]) {
          if (pending_tasks.empty()) {
          } else {
            Task* task = pending_tasks.front();
            pending_tasks.pop();
            tasks[bi] = task;
          }
        }

        Bot* bot = st->bots[bi];
        if (tasks[bi]) {
          Task* task = tasks[bi];
          if (bot->p.y < task->y) {
            int dy = max(-15, min(15, task->y - bot->p.y));
            Command cmd = SMove(Diff(0, dy, 0));
            State::MoveSt mst = st->check_move(bi, cmd);
            if (mst == State::TEMP) {
              st->step(bi, Wait());
            } else if (mst == State::OK) {
              st->step(bi, cmd);
            } else {
              assert(false);
            }
            continue;
          }

          if (bot->p.z != task->z) {
            int dz = max(-15, min(15, task->z - bot->p.z));
            Command cmd = SMove(Diff(0, 0, dz));
            State::MoveSt mst = st->check_move(bi, cmd);
            if (mst == State::OK) {
              st->step(bi, cmd);
            } else if (mst == State::TEMP && rand() % 5 > 0) {
              st->step(bi, Wait());
            } else {
              assert(mst == State::BLOCKED || mst == State::TEMP);
              int dy = min(15, st->model.r() - bot->p.y - 1);
              assert(dy > 0);
              cmd = SMove(Diff(0, dy, 0));
              if (st->check_move(bi, cmd) == State::TEMP) {
                st->step(bi, Wait());
              } else {
                st->step(bi, cmd);
              }
            }
            continue;
          }

          if (bot->p.y > task->y) {
            int dy = max(-15, min(15, task->y - bot->p.y));
            Command cmd = SMove(Diff(0, dy, 0));
            State::MoveSt mst = st->check_move(bi, cmd);
            if (mst == State::TEMP) {
              st->step(bi, Wait());
            } else if (mst == State::OK) {
              st->step(bi, cmd);
            } else {
              assert(false);
            }
            continue;
          }

          if (bot->p.x == task->x) {
            Pos fp = task->p[task->i];
            int dx = fp.x - bot->p.x;
            if (dx <= 1 && dx >= -1) {
              //assert(dx >= -1);
              Command cmd = Fill(Diff(dx, fp.y - bot->p.y, fp.z - bot->p.z));
              State::MoveSt mst = st->check_move(bi, cmd);
              if (mst == State::TEMP) {
                st->step(bi, Wait());
              } else if (mst == State::OK) {
                if (flipped || fp.y == 0 ||
                    st->model.full(fp.x-1, fp.y, fp.z) ||
                    st->model.full(fp.x, fp.y-1, fp.z) ||
                    st->model.full(fp.x, fp.y, fp.z-1) ||
                    st->model.full(fp.x, fp.y, fp.z+1)) {
                  st->step(bi, cmd);
                  task->i++;

                  if (task->i == task->p.size()) {
                    task->x = target->r();
                    tasks[bi] = nullptr;
                  }

                } else if (!(target->full(fp.x-1, fp.y, fp.z) ||
                             target->full(fp.x, fp.y-1, fp.z) ||
                             target->full(fp.x, fp.y, fp.z-1) ||
                             target->full(fp.x, fp.y, fp.z+1))) {
                  st->step(bi, Flip());
                  flipped = true;
                } else {
                  //assert(num_bots > 1);
                  //st->step(bi, Wait());
                  st->step(bi, Flip());
                  flipped = true;
                }
              }
              continue;
            }
          }

          task->x = task->p[task->i].x + 1;
          if (task->y > 0) {
            auto found = task_by_lane.find(make_pair(task->y-1, task->z));
            if (found != task_by_lane.end()) {
              task->x = max(min(task->x, found->second->x - 1), 0);
            }
          }

          if (bot->p.x == task->x) {
            st->step(bi, Wait());
            continue;
          }

          assert(bot->p.x != task->x);
          int dx = max(-15, min(15, task->x - bot->p.x));
          Command cmd = SMove(Diff(dx, 0, 0));
          State::MoveSt mst = st->check_move(bi, cmd);
          if (mst == State::TEMP) {
            st->step(bi, Wait());
          } else if (mst == State::OK) {
            st->step(bi, cmd);
          } else {
            fprintf(stderr, "mst=%d\n", mst);
            assert(false);
          }
          continue;

          assert(false);

        } else {
          assert(false); // TODO
        }
      }

      int num_new_traces = st->trace.size() - orig_num_traces;
#if 1
      fprintf(stderr, "num_bots=%d num_new_traces=%d\n",
              num_bots, num_new_traces);
      for (int i = 0; i < num_new_traces; i++) {
        Task* task = tasks[i];
        fprintf(stderr, "i=%d %s at %s todo=%s\n",
                i,
                st->trace[st->trace.size() - num_new_traces + i].str().c_str(),
                st->bots[i]->p.str().c_str(),
                task && task->i < task->p.size()
                ? task->p[task->i].str().c_str() : "");
      }
#endif
      assert(num_bots == num_new_traces);

      st->next();
    }

  }

 private:
  int max_num_bots_;
  int stride_;

  const Matrix* target;
  vector<Command>* trace;
  BoundingBox bb;
  vector<RowBB> row_bbs;

  map<pair<int, int>, Task*> task_by_lane;
  vector<Task*> tasks;
  queue<Task*> pending_tasks;
  bool flipped;
};
