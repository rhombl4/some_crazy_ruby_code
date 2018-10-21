#ifndef MATRIX_H_
#define MATRIX_H_

#include "types.h"

enum Dir {
  X = 1, Y = 2, Z = 3
};

struct Pos {
  Pos() : x(-1), y(-1), z(-1) {}
  Pos(const Pos& p) = default;
  Pos(int x0, int y0, int z0)
      : x(x0), y(y0), z(z0) {
  }

  bool operator==(const Pos& p) const {
    return x == p.x && y == p.y && z == p.z;
  }

  bool operator!=(const Pos& p) const {
    return !(*this == p);
  }

  bool operator<(const Pos& p) const {
    if (x < p.x) return true;
    if (x > p.x) return false;
    if (y < p.y) return true;
    if (y > p.y) return false;
    return z < p.z;
  }

  string str() const;

  ulong code(int r) const {
    return x * r * r + y * r + z;
  }

  int val(Dir d) {
    switch (d) {
      case X: return x;
      case Y: return y;
      case Z: return z;
      default:
        assert(false);
    }
  }

  int x, y, z;
};

struct BoundingBox {
  int min_x, min_y, min_z;
  int max_x, max_y, max_z;
};

struct RowBB {
  int min_x, min_z;
  int max_x, max_z;
};

class Matrix {
 public:
  explicit Matrix(int r);

  explicit Matrix(const string& filename);

  bool operator==(const Matrix& m) const { return m_ == m.m_; }

  int r() const { return r_; }

  int idx(int x, int y, int z) const {
    return x * r_ * r_ + y * r_ + z;
  }

  bool oob(int x, int y, int z) const {
    if (x < 0 || x >= r_ || y < 0 || y >= r_ || z < 0 || z >= r_)
      return true;
    return false;
  }

  bool oob(const Pos& p) const {
    return oob(p.x, p.y, p.z);
  }

  bool full(int x, int y, int z) const {
    if (oob(x, y, z))
      return false;
    int i = idx(x, y, z);
    return (m_[i / 8] >> (i % 8)) & 1;
  }

  bool full(const Pos& p) const {
    return full(p.x, p.y, p.z);
  }

  void fill(int x, int y, int z) {
    assert(!oob(x, y, z));
    int i = idx(x, y, z);
    m_[i / 8] |= 1 << (i % 8);
  }

  void reset(int x, int y, int z) {
    assert(!oob(x, y, z));
    int i = idx(x, y, z);
    m_[i / 8] &= ~(1 << (i % 8));
  }

  void bound(BoundingBox* bb) const {
    bb->min_x = r_;
    bb->min_y = r_;
    bb->min_z = r_;
    bb->max_x = -1;
    bb->max_y = -1;
    bb->max_z = -1;
    for (int x = 0; x < r_; x++) {
      for (int y = 0; y < r_; y++) {
        for (int z = 0; z < r_; z++) {
          if (full(x, y, z)) {
            bb->min_x = min(bb->min_x, x);
            bb->min_y = min(bb->min_y, y);
            bb->min_z = min(bb->min_z, z);
            bb->max_x = max(bb->max_x, x);
            bb->max_y = max(bb->max_y, y);
            bb->max_z = max(bb->max_z, z);
          }
        }
      }
    }
  }

  void rowBound(int y, RowBB* rbb) const {
    rbb->min_x = r_;
    rbb->min_z = r_;
    rbb->max_x = -1;
    rbb->max_z = -1;
    for (int x = 0; x < r_; x++) {
      for (int z = 0; z < r_; z++) {
        if (full(x, y, z)) {
          rbb->min_x = min(rbb->min_x, x);
          rbb->min_z = min(rbb->min_z, z);
          rbb->max_x = max(rbb->max_x, x);
          rbb->max_z = max(rbb->max_z, z);
        }
      }
    }
  }

  void rowBound(vector<RowBB>* rbbs) const {
    for (int y = 0; y < r_; y++) {
      RowBB rbb;
      rowBound(y, &rbb);
      rbbs->push_back(rbb);
    }
  }

  int total() const {
    int t = 0;
    for (int x = 0; x < r_; x++) {
      for (int y = 0; y < r_; y++) {
        for (int z = 0; z < r_; z++) {
          if (full(x, y, z))
            t++;
        }
      }
    }
    return t;
  }

 private:
  vector<byte> m_;
  int r_;
};

#endif
