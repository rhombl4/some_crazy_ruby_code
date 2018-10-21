#include "dep.h"

int main() {
  {
    Matrix m(9);
    m.fill(0, 0, 0);
    m.fill(0, 1, 0);
    m.fill(0, 2, 0);
    m.fill(1, 2, 0);
    m.fill(2, 2, 0);
    DepTracker dep(m);
    assert(!dep.canRemove(Pos(0, 0, 0)));
    assert(!dep.canRemove(Pos(0, 1, 0)));
    assert(!dep.canRemove(Pos(0, 2, 0)));
    assert(!dep.canRemove(Pos(1, 2, 0)));
    assert(dep.canRemove(Pos(2, 2, 0)));
  }

  {
    Matrix m(9);
    m.fill(0, 0, 0);
    m.fill(0, 1, 0);
    m.fill(0, 2, 0);
    m.fill(0, 2, 1);
    m.fill(0, 2, 2);
    m.fill(0, 1, 1);
    DepTracker dep(m);
    assert(!dep.canRemove(Pos(0, 0, 0)));
    assert(!dep.canRemove(Pos(0, 1, 0)));
    assert(dep.canRemove(Pos(0, 2, 0)));
    assert(!dep.canRemove(Pos(0, 2, 1)));
    assert(dep.canRemove(Pos(0, 2, 2)));
    assert(dep.canRemove(Pos(0, 1, 1)));

    dep.remove(Pos(0, 1, 1));
    assert(!dep.canRemove(Pos(0, 2, 0)));
  }
}
