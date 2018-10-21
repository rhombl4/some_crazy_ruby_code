#ifndef STATE_H_
#define STATE_H_

#include "command.h"
#include "matrix.h"
#include "types.h"

#include <unordered_set>

Pos applyNear(Pos p, Diff d);

struct Bot {
  explicit Bot(int id0, int x0, int y0, int z0)
      : id(id0), p(x0, y0, z0) {
  }
  int id;
  Pos p;
  set<int> seeds;
};

struct State {
 public:
  enum MoveSt {
    OK, TEMP, BLOCKED, OOB
  };

  explicit State(int r);
  explicit State(const Matrix& m);

  int num_bots() const { return (int)bots.size(); }

  MoveSt check_move(int bi, Command cmd, int* ok_dist=nullptr) const;
  bool is_valid(int bi, Command cmd) const;
  void step(int bi, Command cmd);
  void next();

  ulong energy;
  bool harmonics;
  bool done;
  Matrix model;
  vector<Bot*> bots;
  vector<Command> trace;

 private:
  void initBots();

  unordered_set<ulong> volat;
  vector<pair<Bot*, Command>> forking_bots;
  vector<pair<Bot*, Bot*>> fusing_bots;
  unordered_set<int> expected_secondaries;
  unordered_set<int> actual_secondaries;
  vector<Command> gvoids;
};

#endif
