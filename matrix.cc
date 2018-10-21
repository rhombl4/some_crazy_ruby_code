#include "matrix.h"

#include <stdio.h>
#include <sstream>

string Pos::str() const {
  ostringstream oss;
  oss << "(" << x << "," << y << "," << z << ")";
  return oss.str();
}

Matrix::Matrix(int r) : r_(r) {
  m_.resize((r * r * r + 7) / 8);
}

Matrix::Matrix(const string& filename) {
  FILE* fp = fopen(filename.c_str(), "rb");
  int r = fgetc(fp);
  r_ = r;
  size_t sz = (r * r * r + 7) / 8;
  m_.resize(sz);
  size_t n = fread(&m_[0], 1, sz, fp);
  if (n != sz) {
    fprintf(stderr, "%zu e=%zu\n", n, sz);
    assert(false);
  }
  fclose(fp);
}
