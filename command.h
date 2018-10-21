#ifndef COMMAND_H_
#define COMMAND_H_

#include "matrix.h"
#include "types.h"

#include <stdio.h>

enum Op {
  HALT, WAIT, FLIP, S_MOVE, L_MOVE, FUSION_P, FUSION_S, FISSION, FILL,
  VOID, G_FILL, G_VOID
};

struct Diff {
  Diff() : x(0), y(0), z(0) {}
  Diff(int x0, int y0, int z0)
      : x(x0), y(y0), z(z0) {
  }
  Diff(Dir d, int l)
      : x(0), y(0), z(0) {
    switch (d) {
      case X: x = l; break;
      case Y: y = l; break;
      case Z: z = l; break;
      default:
        assert(false);
    }
  }

  string str() const;

  int nonZeroCount() const {
    int cnt = 0;
    if (x) cnt++;
    if (y) cnt++;
    if (z) cnt++;
    return cnt;
  }

  bool isSmall(int mx) const {
    return (x >= -mx && x <= mx &&
            y >= -mx && y <= mx &&
            z >= -mx && z <= mx);
  }

  int x, y, z;
};

struct Command {
  string str() const;

  void emit(FILE* fp) const;

  Op op;
  Diff d;
  Diff d2;
  int m;
};

Command Halt();
Command Wait();
Command Flip();
Command SMove(Diff d);
Command LMove(Diff d, Diff d2);
Command FusionP(Diff d);
Command FusionS(Diff d);
Command Fission(Diff d, int m);
Command Fill(Diff d);
Command Void(Diff d);
Command GFill(Diff d, int x, int y, int z);
Command GVoid(Diff d, int x, int y, int z);

void parseTrace(const string& filename, vector<Command>* trace);
void emitTrace(const string& filename, const vector<Command>& trace);

#endif
