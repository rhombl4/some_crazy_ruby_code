#ifndef DEP_H_
#define DEP_H_

#include "matrix.h"

#include <set>

class DepTracker {
 public:
  explicit DepTracker(const Matrix& m);

  bool canRemove(Pos p) const {
    return can_remove_[idx(p)];
  }
  void remove(Pos p);

  bool canRemoveCluster(const set<Pos>& cluster) const;

 private:
  bool canRemoveImpl(Pos p) const;

  int idx(int x, int y, int z) const {
    return x * r_ * r_ + y * r_ + z;
  }

  int idx(Pos p) const {
    return idx(p.x, p.y, p.z);
  }

  vector<byte> links_;
  vector<byte> ref_cnts_;
  vector<bool> can_remove_;
  int r_;
};

#endif
