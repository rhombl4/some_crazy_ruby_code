#include "state.h"

#include <algorithm>
#include <sstream>

struct LinearDiffIterator {
  explicit LinearDiffIterator(Diff d) {
    i = 0;
    dx = dy = dz = 0;
    if (d.x > 0) n = d.x, dx = 1;
    else if (d.x < 0) n = -d.x, dx = -1;
    else if (d.y > 0) n = d.y, dy = 1;
    else if (d.y < 0) n = -d.y, dy = -1;
    else if (d.z > 0) n = d.z, dz = 1;
    else if (d.z < 0) n = -d.z, dz = -1;
    else assert(false);
  }

  bool next(Pos* p) {
    if (i == n)
      return false;
    p->x += dx;
    p->y += dy;
    p->z += dz;
    i++;
    return true;
  }

  int i;
  int n;
  int dx, dy, dz;
};

Pos applyNear(Pos p, Diff d) {
  p.x += d.x;
  p.y += d.y;
  p.z += d.z;
  return p;
}

void State::initBots() {
  Bot* b = new Bot(1, 0, 0, 0);
  for (int i = 2; i <= 40; i++) {
    b->seeds.insert(i);
  }
  bots.emplace_back(b);
}

State::State(int r)
    : energy(0), harmonics(false), done(false), model(r) {
  initBots();
}

State::State(const Matrix& m)
    : energy(0), harmonics(false), done(false), model(m) {
  initBots();
}

bool State::is_valid(int bi, Command cmd) const {
  return check_move(bi, cmd) == OK;
}

State::MoveSt State::check_move(int bi, Command cmd, int* ok_dist) const {
  assert(bi < (int)bots.size());
  Bot* bot = bots[bi];

  if (cmd.op == S_MOVE || cmd.op == L_MOVE) {
    Pos p = bot->p;
    for (LinearDiffIterator iter(cmd.d); iter.next(&p); ) {
      if (model.oob(p))
        return OOB;
      if (model.full(p.x, p.y, p.z))
        return BLOCKED;
      if (volat.count(p.code(model.r())))
        return TEMP;
      if (ok_dist) ++*ok_dist;
    }

    if (cmd.op == L_MOVE) {
      for (LinearDiffIterator iter(cmd.d2); iter.next(&p); ) {
        if (model.oob(p))
          return OOB;
        if (model.full(p.x, p.y, p.z))
          return BLOCKED;
        if (volat.count(p.code(model.r())))
          return TEMP;
        if (ok_dist) ++*ok_dist;
      }
    }
  } else if (cmd.op == FISSION || cmd.op == FILL) {
    Pos n = applyNear(bot->p, cmd.d);
    if (model.oob(n))
      return OOB;
    if (model.full(n.x, n.y, n.z))
      return BLOCKED;
    if (volat.count(n.code(model.r())))
      return TEMP;
  }

  return OK;
}

void State::step(int bi, Command cmd) {
  trace.push_back(cmd);
  assert(is_valid(bi, cmd));
  Bot* bot = bots[bi];

  switch (cmd.op) {
    case HALT:
      if (bots.size() != 1) {
        assert(false);
      }
      done = true;
      break;

    case WAIT:
      break;

    case FLIP:
      harmonics = !harmonics;
      break;

    case S_MOVE: {
      for (LinearDiffIterator iter(cmd.d); iter.next(&bot->p); ) {
        assert(!model.oob(bot->p));
        if (!volat.insert(bot->p.code(model.r())).second) {
          fprintf(stderr, "SMove conflict at %s\n", bot->p.str().c_str());
          assert(false);
        }
        energy += 2;
      }
      break;
    }

    case L_MOVE: {
      for (LinearDiffIterator iter(cmd.d); iter.next(&bot->p); ) {
        assert(!model.oob(bot->p));
        if (!volat.insert(bot->p.code(model.r())).second) {
          fprintf(stderr, "LMove conflict at %s\n", bot->p.str().c_str());
          assert(false);
        }
        energy += 2;
      }
      for (LinearDiffIterator iter(cmd.d2); iter.next(&bot->p); ) {
        assert(!model.oob(bot->p));
        if (!volat.insert(bot->p.code(model.r())).second) {
          fprintf(stderr, "LMove conflict at %s\n", bot->p.str().c_str());
          assert(false);
        }
        energy += 2;
      }
      energy += 4;
      break;
    }

    case FUSION_P: {
      Pos n = applyNear(bot->p, cmd.d);
      Bot* sbot = nullptr;
      for (Bot* b : bots) {
        if (n == b->p) {
          sbot = b;
        }
      }
      assert(sbot);
      actual_secondaries.insert(sbot->id);
      fusing_bots.emplace_back(bot, sbot);

      energy -= 24;
      break;
    }

    case FUSION_S: {
      expected_secondaries.insert(bot->id);
      break;
    }

    case FISSION: {
      Pos n = applyNear(bot->p, cmd.d);
      if (!volat.insert(n.code(model.r())).second) {
        fprintf(stderr, "FISSION conflict at %s\n", n.str().c_str());
        assert(false);
      }
      forking_bots.emplace_back(bot, cmd);
      break;
    }

    case FILL: {
      Pos n = applyNear(bot->p, cmd.d);
      if (!volat.insert(n.code(model.r())).second) {
        fprintf(stderr, "FILL conflict at %s\n", n.str().c_str());
        assert(false);
      }
      if (model.full(n.x, n.y, n.z)) {
        fprintf(stderr, "warning: fill waste at %s\n", n.str().c_str());
        energy += 6;
      } else {
        model.fill(n.x, n.y, n.z);
        energy += 12;
      }
      break;
    }

    case VOID: {
      Pos n = applyNear(bot->p, cmd.d);
      if (!volat.insert(n.code(model.r())).second) {
        fprintf(stderr, "VOID conflict at %s\n", n.str().c_str());
        assert(false);
      }
      if (model.full(n.x, n.y, n.z)) {
        model.reset(n.x, n.y, n.z);
        energy -= 12;
      } else {
        fprintf(stderr, "warning: void waste at %s\n", n.str().c_str());
        energy += 3;
      }
      break;
    }

    case G_FILL: {
      Command c = cmd;
      c.d.x += bot->p.x;
      c.d.y += bot->p.y;
      c.d.z += bot->p.z;

      int x1 = c.d.x;
      int y1 = c.d.y;
      int z1 = c.d.z;
      int x2 = c.d.x + c.d2.x;
      int y2 = c.d.y + c.d2.y;
      int z2 = c.d.z + c.d2.z;
      if (x1 > x2) swap(x1, x2);
      if (y1 > y2) swap(y1, y2);
      if (z1 > z2) swap(z1, z2);
      //fprintf(stderr, "(%d,%d,%d)-(%d,%d,%d)\n", x1, y1, z1, x2, y2, z2);
      for (int x = x1; x <= x2; x++) {
        for (int y = y1; y <= y2; y++) {
          for (int z = z1; z <= z2; z++) {
            //if (model.full(x, y, z)) {
              //model.reset(x, y, z);
              //energy -= 12;
            //} else {
            //  energy += 3;
            //}
            model.fill(x, y, z);
            energy += 6;
          }
        }
      }

      break;
    }

    case G_VOID: {
      Command c = cmd;
      c.d.x += bot->p.x;
      c.d.y += bot->p.y;
      c.d.z += bot->p.z;
      gvoids.push_back(c);
      break;
    }

    default:
      assert(false);

  }
}

void State::next() {
  if (expected_secondaries != actual_secondaries) {
    fprintf(stderr, "fusion mismatch!\n");
    assert(false);
  }

  if (!gvoids.empty()) {
    assert(gvoids.size() == 4 || gvoids.size() == 8);
    Command c = gvoids[0];
    int x1 = c.d.x;
    int y1 = c.d.y;
    int z1 = c.d.z;
    int x2 = c.d.x + c.d2.x;
    int y2 = c.d.y + c.d2.y;
    int z2 = c.d.z + c.d2.z;
    if (x1 > x2) swap(x1, x2);
    if (y1 > y2) swap(y1, y2);
    if (z1 > z2) swap(z1, z2);
    //fprintf(stderr, "(%d,%d,%d)-(%d,%d,%d)\n", x1, y1, z1, x2, y2, z2);
    if (gvoids.size() == 4)
      assert(y1 == y2);
    for (int x = x1; x <= x2; x++) {
      for (int y = y1; y <= y2; y++) {
        for (int z = z1; z <= z2; z++) {
          if (model.full(x, y, z)) {
            model.reset(x, y, z);
            energy -= 12;
          } else {
            energy += 3;
          }
        }
      }
    }
    gvoids.clear();
  }

  for (const auto& p : forking_bots) {
    Bot* bot = p.first;
    Command cmd = p.second;
    Pos n = applyNear(bot->p, cmd.d);

    assert(!bot->seeds.empty());
    int nid = -1;
    set<int> nseeds;
    set<int> oseeds;
    for (int id : bot->seeds) {
      if (nid == -1) {
        nid = id;
      } else if ((int)nseeds.size() < cmd.m) {
        nseeds.insert(id);
      } else {
        oseeds.insert(id);
      }
    }

    Bot* nbot = new Bot(nid, n.x, n.y, n.z);
    bot->seeds = oseeds;
    nbot->seeds = nseeds;
    bots.push_back(nbot);
    sort(bots.begin(), bots.end(),
         [](const Bot* a, const Bot* b) {
           return a->id < b->id;
         });

    energy += 24;
  }
  forking_bots.clear();

  for (const auto& p : fusing_bots) {
    Bot* bot = p.first;
    Bot* sbot = p.second;
    bot->seeds.insert(sbot->id);
    for (int id : sbot->seeds) {
      bot->seeds.insert(id);
    }
    bots.erase(find(bots.begin(), bots.end(), sbot));
    delete sbot;
  }
  fusing_bots.clear();

  ulong r = model.r();
  energy += (harmonics ? 30 : 3) * r * r * r;
  energy += bots.size() * 20;
  volat.clear();
  expected_secondaries.clear();
  actual_secondaries.clear();

  for (const auto& bot : bots) {
    volat.insert(bot->p.code(r));
  }
}
