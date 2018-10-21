#include "scheduler.h"

#include "util.h"

State::MoveSt Scheduler::goStraightBlockable(
    Dir dir, int bi, int g, bool wait) {
  Bot* bot = st->bots[bi];
  int c = bot->p.val(dir);
  int d = max(-15, min(15, g - c));
  Command cmd = SMove(Diff(dir, d));
  int ok_dist = 0;
  State::MoveSt mst = st->check_move(bi, cmd, &ok_dist);
  if (mst == State::TEMP) {
    if (wait) {
      if (ok_dist) {
        d = d >= 0 ? ok_dist : -ok_dist;
        Command cmd = SMove(Diff(dir, d));
        st->step(bi, cmd);
      } else {
        st->step(bi, Wait());
      }
      return State::OK;
    } else {
      return mst;
    }
  } else if (mst == State::OK) {
    st->step(bi, cmd);
    return mst;
  } else if (mst == State::BLOCKED) {
    return mst;
  } else {
    fprintf(stderr, "mst=%d\n", mst);
    assert(false);
  }
  assert(false);
}

bool Scheduler::goStraight(Dir dir, int bi, int g, bool wait) {
  Bot* bot = st->bots[bi];
  int c = bot->p.val(dir);
  if (c == g) {
    return false;
  }

  int d = max(-15, min(15, g - c));
  Command cmd = SMove(Diff(dir, d));
  int ok_dist = 0;
  State::MoveSt mst = st->check_move(bi, cmd, &ok_dist);
  if (mst == State::TEMP) {
    if (wait) {
      if (ok_dist) {
        d = d >= 0 ? ok_dist : -ok_dist;
        Command cmd = SMove(Diff(dir, d));
        st->step(bi, cmd);
      } else {
        st->step(bi, Wait());
      }
    } else {
      return false;
    }
  } else if (mst == State::OK) {
    st->step(bi, cmd);
  } else {
    fprintf(stderr, "bi=%d dir=%d g=%d mst=%d from=%s\n",
            bi, dir, g, mst, bot->p.str().c_str());
    assert(false);
  }
  return true;
}

bool Scheduler::gotoXZAtRoof(int bi, int gx, int gz) {
  Bot* bot = st->bots[bi];
  if (gx == bot->p.x && gz == bot->p.z)
    return false;

  if (st->bots[bi]->p.y < roof())
    if (goStraight(Y, bi, roof()))
      return true;

  //assert(bot->p.y == roof());
  if (rnd(2) == 0) {
    if (gx != bot->p.x && goStraight(X, bi, gx, false)) {
      return true;
    }
    if (gz != bot->p.z && goStraight(Z, bi, gz, false)) {
      return true;
    }
    if (gx != bot->p.x && goStraight(X, bi, gx)) {
      return true;
    }
    if (gz != bot->p.z && goStraight(Z, bi, gz)) {
      return true;
    }
  } else {
    if (gz != bot->p.z && goStraight(Z, bi, gz, false)) {
      return true;
    }
    if (gx != bot->p.x && goStraight(X, bi, gx, false)) {
      return true;
    }
    if (gz != bot->p.z && goStraight(Z, bi, gz)) {
      return true;
    }
    if (gx != bot->p.x && goStraight(X, bi, gx)) {
      return true;
    }
  }
  assert(false);
  st->step(bi, Wait());
  return true;
}

bool Scheduler::gotoXYZ(int bi, int gx, int gy, int gz) {
  Bot* bot = st->bots[bi];
  if (gx == bot->p.x && gy == bot->p.y && gz == bot->p.z)
    return false;

  if (st->bots[bi]->p.y < gy)
    if (goStraight(Y, bi, gy))
      return true;

  auto goX = [&](bool wait) {
    if (gx != bot->p.x) {
      State::MoveSt mst = goStraightBlockable(X, bi, gx, wait);
      if (mst == State::BLOCKED) {
        if (goStraight(Y, bi, roof()))
          return true;
        assert(false);
      }
      return mst == State::OK;
    }
    return false;
  };

  auto goZ = [&](bool wait) {
    if (gz != bot->p.z) {
      State::MoveSt mst = goStraightBlockable(Z, bi, gz, wait);
      if (mst == State::BLOCKED) {
        if (goStraight(Y, bi, roof()))
          return true;
        assert(false);
      }
      return mst == State::OK;
    }
    return false;
  };

  if (rnd(2) == 0) {
    if (goX(false)) {
      return true;
    }
    if (goZ(false)) {
      return true;
    }
    if (goX(true)) {
      return true;
    }
    if (goZ(true)) {
      return true;
    }
  } else {
    if (goZ(false)) {
      return true;
    }
    if (goX(false)) {
      return true;
    }
    if (goZ(true)) {
      return true;
    }
    if (goX(true)) {
      return true;
    }
  }

  if (goStraight(Y, bi, gy))
    return true;

  assert(false);
  st->step(bi, Wait());
  return true;
}


bool Scheduler::gotoXYZNoAvoid(int bi, int gx, int gy, int gz) {
  Bot* bot = st->bots[bi];
  if (gx == bot->p.x && gy == bot->p.y && gz == bot->p.z)
    return false;

  if (st->bots[bi]->p.y < gy)
    if (goStraight(Y, bi, gy))
      return true;

  auto goX = [&](bool wait) {
    if (gx != bot->p.x) {
      State::MoveSt mst = goStraightBlockable(X, bi, gx, wait);
      return mst == State::OK;
    }
    return false;
  };

  auto goZ = [&](bool wait) {
    if (gz != bot->p.z) {
      State::MoveSt mst = goStraightBlockable(Z, bi, gz, wait);
      return mst == State::OK;
    }
    return false;
  };

  if (rnd(2) == 0) {
    if (goX(false)) {
      return true;
    }
    if (goZ(false)) {
      return true;
    }
    if (goX(true)) {
      return true;
    }
    if (goZ(true)) {
      return true;
    }
  } else {
    if (goZ(false)) {
      return true;
    }
    if (goX(false)) {
      return true;
    }
    if (goZ(true)) {
      return true;
    }
    if (goX(true)) {
      return true;
    }
  }

  if (goStraight(Y, bi, gy))
    return true;

  //assert(false);
  st->step(bi, Wait());
  return true;
}

void Scheduler::spawnToRoof(int bi) {
  Command cmd = Wait();
  for (int i = 0; i < 3; i++) {
    Command c = Wait();
    if (i == 0) {
      c = Fission(Diff(1, 1, 0), 0);
    } else if (i == 1) {
      c = Fission(Diff(0, 1, 0), 0);
    } else {
      c = Fission(Diff(0, 1, 1), 0);
    }
    if (c.op != WAIT && st->check_move(bi, c) == State::OK) {
      cmd = c;
      break;
    }
  }
  st->step(bi, cmd);
}

void Scheduler::spawnToRoof2(int bi) {
  Command cmd = Wait();
  for (int i = 0; i < 3; i++) {
    Command c = Wait();
    c = Fission(Diff(0, 1, 0), 0);
    if (c.op != WAIT && st->check_move(bi, c) == State::OK) {
      cmd = c;
      break;
    }
  }
  st->step(bi, cmd);
}

void Scheduler::spawnRandomly(int bi) {
  Command cmd = Wait();
  for (int i = 0; i < 50; i++) {
    int dx = rnd(2);
    int dy = rnd(2);
    int dz = rnd(2);
    if ((dx && dy && dz) ||
        (!dx && !dy && !dz))
      continue;
    Command c = Fission(Diff(dx, dy, dz), 0);
    if (c.op != WAIT && st->check_move(bi, c) == State::OK) {
      cmd = c;
      break;
    }
  }
  st->step(bi, cmd);
}

void Scheduler::findFusion(int* fusion_p, int* fusion_s) {
  for (int bi = 0; bi < (int)st->bots.size(); bi++) {
    Bot* bot = st->bots[bi];
    int x = bot->p.x;
    int y = bot->p.y;
    int z = bot->p.z;
    if (x == 0 && y == roof() && z == 0) {
      *fusion_p = bi;
    } else if ((x == 0 && y == roof() && z == 1) ||
               (x == 1 && y == roof() && z == 0) ||
               (x == 0 && y == roof() - 1 && z == 0)) {
      *fusion_s = bi;
    }
  }
}
