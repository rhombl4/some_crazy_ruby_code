#include "dep.h"
#include "matrix.h"

#include <queue>
#include <map>
#include <set>

#ifndef NO_MAIN
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
#endif

struct Plan {
  int x, y, z;
  int dx, dy, dz;
};

class GVoidSim {
 public:
  explicit GVoidSim(const Matrix& m0)
      : m(m0), r(m0.r()), n(m0.total()), dep(m0) {
    m.bound(&bb);
  }

  bool isGrounded() const {
    set<Pos> seen;
    queue<Pos> q;
    for (int x = bb.min_x; x <= bb.max_x; x++) {
      for (int z = bb.min_z; z <= bb.max_z; z++) {
        if (m.full(x, 0, z)) {
          q.push(Pos(x, 0, z));
        }
      }
    }

    int c = 0;
    while (!q.empty()) {
      Pos p = q.front();
      q.pop();
      if (!seen.emplace(p).second)
        continue;
      c++;
      for (int d = 0; d < 6; d++) {
        Pos n = inc(p, d);
        if (m.full(n))
          q.push(n);
      }
    }

    return c == n;
  }

  bool isGroundedFrom(const set<Pos>& from) const {
    return dep.canRemoveCluster(from);
  }

  int run() {
    for (int y = bb.max_y; y >= 0; y--) {
      fprintf(stderr, "y=%d\n", y);
      set<Pos> from;
      for (int x = bb.min_x; x <= bb.max_x; x++) {
        for (int z = bb.min_z; z <= bb.max_z; z++) {
          if (m.full(x, y, z)) {
            //m.reset(x, y, z);
            //n--;
            from.emplace(Pos(x, y, z));
          }
        }
      }

      if (!isGroundedFrom(from))
        return y + 1;

      for (Pos f : from) {
        m.reset(f.x, f.y, f.z);
        dep.remove(f);
        n--;
      }

      //if (!isGrounded())
      //  return y + 1;
    }
    return 0;
  }

  int makePlan(vector<Plan>* plan) {
    for (int y = bb.max_y; y >= 0; y--) {
      RowBB rbb;
      m.rowBound(y, &rbb);

      set<pair<int, int>> todos;
      for (int rx = rbb.min_x; rx <= rbb.max_x; rx += 30) {
        for (int rz = rbb.min_z; rz <= rbb.max_z; rz += 30) {
          todos.emplace(rx, rz);
        }
      }

      fprintf(stderr, "y=%d todos=%zu\n", y, todos.size());

      while (!todos.empty()) {
        int cnt = 0;

        for (auto iter = todos.begin(); iter != todos.end(); ) {
          int rx = iter->first;
          int rz = iter->second;

          set<Pos> from;
          for (int x = rx; x < rx + 30; x++) {
            for (int z = rz; z < rz + 30; z++) {
              if (m.full(x, y, z)) {
                from.emplace(Pos(x, y, z));
              }
            }
          }

          if (isGroundedFrom(from)) {
            int c = 0;
            for (Pos f : from) {
              m.reset(f.x, f.y, f.z);
              dep.remove(f);
              n--;
              c++;
            }
            if (c) {
              cnt++;
              Plan p;
              p.x = rx;
              p.y = y;
              p.z = rz;
              p.dx = min(rx + 30, r) - rx - 1;
              p.dz = min(rz + 30, r) - rz - 1;
              assert(p.dx >= 0);
              assert(p.dz >= 0);
              plan->push_back(p);
            }
            iter = todos.erase(iter);
          } else {
            ++iter;
          }
        }

        if (cnt == 0) {
          return y + 1;
        }
      }

    }
    return 0;
  }

  int makePlanZ(vector<Plan>* plan) {
#if 0
    for (int x = bb.min_x; x <= bb.max_x; x++) {
      fprintf(stderr, "x=%d\n", x);
      for (int y = 0; y < r; y++) {
        for (int z = 0; z < r; z++) {
          char c = m.full(x, y, z) ? 'O' : '_';
          fprintf(stderr, "%c", c);
        }
        fprintf(stderr, "\n");
      }
    }

    for (int x = bb.min_x; x <= bb.max_x; x++) {
      fprintf(stderr, "x=%d\n", x);
      for (int y = 0; y < r; y++) {
        for (int z = 0; z < r; z++) {
          char c = m.full(x, y, z) ? 'O' : '_';
          fprintf(stderr, "%c", c);
        }
        fprintf(stderr, "\n");
      }
    }

    set<Pos> base;
    for (int x = bb.min_x; x <= bb.max_x; x++) {
      set<Pos> c;
      for (int z = 0; z < r; z++) {
        for (int y = 0; y < r; y++) {
          if (m.full(x, y, z)) {
            c.emplace(Pos(0, y, z));
          }
        }
      }
      fprintf(stderr, "x=%d n=%zu\n", x, c.size());

      if (base.empty()) {
        base = c;
      } else {
        if (base != c)
          return 0;
      }
    }

    return 1;
#endif

    for (int z = bb.min_z; z <= bb.max_z; z++) {
#if 0
      RowBB rbb;
      m.rowBound(y, &rbb);
#endif

      vector<pair<int, int>> todos;
      for (int ry = bb.max_y; ry >= 0; ry -= 30) {
        for (int rx = bb.min_x; rx <= bb.max_x; rx += 30) {
          todos.emplace_back(rx, ry);
        }
      }

      fprintf(stderr, "z=%d (%d-%d) todos=%zu\n",
              z, bb.min_z, bb.max_z, todos.size());

      while (!todos.empty()) {
        vector<pair<int, int>> ntodos;
        int cnt = 0;

        for (auto p : todos) {
          int rx = p.first;
          int ry = p.second;

          set<Pos> from;
          for (int x = rx; x < rx + 30; x++) {
            for (int y = ry; y > ry - 30; y--) {
              if (m.full(x, y, z)) {
                from.emplace(Pos(x, y, z));
              }
            }
          }

#if 1
          fprintf(stderr, "from_size=%zu\n", from.size());
          for (Pos p : from) {
            fprintf(stderr, "%d,%d,%d\n", p.x,p.y,p.z);
          }
#endif

          if (isGroundedFrom(from)) {
            int c = 0;
            for (Pos f : from) {
              m.reset(f.x, f.y, f.z);
              dep.remove(f);
              n--;
              c++;
            }
            if (c) {
              cnt++;
              Plan p;
              p.x = rx;
              p.y = ry;
              p.z = z;
              p.dx = min(rx + 30, r) - rx - 1;
              p.dy = -max(ry, 29);
              assert(p.dx >= 0);
              assert(p.dy <= 0);
              plan->push_back(p);
            }
          } else {
            ntodos.push_back(p);
          }
        }

        if (cnt == 0) {
          fprintf(stderr, "fail\n");
          return z + 1;
        }
        todos.swap(ntodos);
      }

    }
    return 0;
  }

 private:
  Matrix m;
  int r;
  int n;
  BoundingBox bb;
  DepTracker dep;
};

#ifndef NO_MAIN

int main(int argc, const char* argv[]) {
  if (argc <= 1) {
    fprintf(stderr, "Usage: %s mdl\n", argv[0]);
    exit(1);
  }

  Matrix m(argv[1]);

  {
    GVoidSim sim(m);
    //assert(sim.isGrounded());
    printf("%d\n", sim.run());
  }

  {
    GVoidSim sim(m);
    //assert(sim.isGrounded());
    vector<Plan> plan;
    printf("%d\n", sim.makePlan(&plan));
    for (const Plan& p : plan) {
      fprintf(stderr, "%d,%d,%d %d,%d\n", p.x, p.y, p.z, p.dx, p.dz);
    }
  }

  {
    GVoidSim sim(m);
    //assert(sim.isGrounded());
    vector<Plan> plan;
    fprintf(stderr, "\n=== make plan2 ===\n");
    printf("%d\n", sim.makePlanZ(&plan));
    for (const Plan& p : plan) {
      fprintf(stderr, "%d,%d,%d %d,%d\n", p.x, p.y, p.z, p.dx, p.dz);
    }
  }

}

#endif
