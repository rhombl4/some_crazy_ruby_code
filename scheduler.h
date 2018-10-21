#ifndef SCHEDULER_H_
#define SCHEDULER_H_

#include "matrix.h"
#include "state.h"

class Scheduler {
 public:
  virtual void schedule(const Matrix& target,
                        State* st) = 0;

  bool goStraight(Dir dir, int bi, int g, bool wait=true);

  State::MoveSt goStraightBlockable(Dir dir, int bi, int g, bool wait);

  bool gotoXZAtRoof(int bi, int gx, int gz);

  bool gotoXYZ(int bi, int gx, int gy, int gz);

  bool gotoXYZNoAvoid(int bi, int gx, int gy, int gz);

  void spawnToRoof(int bi);
  void spawnToRoof2(int bi);
  void spawnRandomly(int bi);

  void findFusion(int* fusion_p, int* fusion_s);

 protected:
  int roof() const { return roof_; }

  State* st;
  const Matrix* target;
  BoundingBox bb;
  int roof_;

};

#endif
