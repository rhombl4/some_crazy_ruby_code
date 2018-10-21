#include "dep.h"

#include <limits.h>

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

DepTracker::DepTracker(const Matrix& m)
    : r_(m.r()) {
  links_.resize(r_ * r_ * r_);
  ref_cnts_.resize(r_ * r_ * r_);
  can_remove_.resize(r_ * r_ * r_);

  set<Pos> cur;
  for (int x = 0; x < r_; x++) {
    for (int z = 0; z < r_; z++) {
      if (m.full(x, 0, z)) {
        ref_cnts_[idx(x, 0, z)] = 255;
        if (!cur.emplace(x, 0, z).second)
          assert(false);
      }
    }
  }

  set<Pos> seen;
  while (!cur.empty()) {
    for (Pos p : cur) {
      if (!seen.emplace(p).second)
        assert(false);
    }

    set<Pos> nxt;
    for (Pos p : cur) {
      for (int d = 0; d < 6; d++) {
        Pos n = inc(p, d);
        if (m.full(n) && seen.count(n) == 0) {
          ref_cnts_[idx(n)]++;
          links_[idx(p)] |= 1 << d;
          nxt.emplace(n);
        }
      }
    }
    cur.swap(nxt);
  }

  for (int x = 0; x < r_; x++) {
    for (int y = 0; y < r_; y++) {
      for (int z = 0; z < r_; z++) {
        can_remove_[idx(x, y, z)] = canRemoveImpl(Pos(x, y, z));
      }
    }
  }
}

bool DepTracker::canRemoveImpl(Pos p) const {
  byte l = links_[idx(p)];
  for (int d = 0; d < 6; d++) {
    if ((l & (1 << d))) {
      Pos n = inc(p, d);
      if (ref_cnts_[idx(n)] == 1)
        return false;
    }
  }
  return true;
}

bool DepTracker::canRemoveCluster(const set<Pos>& cluster) const {
  for (Pos p : cluster) {
    byte l = links_[idx(p)];
    for (int d = 0; d < 6; d++) {
      if ((l & (1 << d))) {
        Pos n = inc(p, d);
        if (!cluster.count(n) && ref_cnts_[idx(n)] == 1) {
          return false;
        }
      }
    }
  }
  return true;
}

void DepTracker::remove(Pos p) {
  byte l = links_[idx(p)];
  for (int d = 0; d < 6; d++) {
    if ((l & (1 << d))) {
      Pos n = inc(p, d);
      ref_cnts_[idx(n)]--;
    }
  }
  ref_cnts_[idx(p)] = 0;

  for (int ix = -2; ix <= 2; ix++) {
    for (int iy = -2; iy <= 2; iy++) {
      int d = abs(ix) + abs(iy);
      if (d > 2)
        continue;
      for (int iz = -2; iz <= 2; iz++) {
        if (d + abs(iz) > 2)
          continue;
        Pos n(p.x + ix, p.y + iy, p.z + iz);
        if (n.x < 0 || n.x >= r_ ||
            n.y < 0 || n.y >= r_ ||
            n.z < 0 || n.z >= r_) {
          continue;
        }
        can_remove_[idx(n)] = canRemoveImpl(n);
      }
    }
  }
}
