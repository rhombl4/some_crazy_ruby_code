#include "command.h"

#include <stdio.h>
#include <sstream>

static const char* OP_STR[] = {
  "Halt", "Wait", "Flip", "SMove", "LMove",
  "FusionP", "FusionS", "Fission", "Fill",
  "Void", "GFill", "GVoid"
};

string Diff::str() const {
  ostringstream oss;
  oss << "<" << x << "," << y << "," << z << ">";
  return oss.str();
}

string Command::str() const {
  ostringstream oss;
  oss << OP_STR[op];
  if (op == S_MOVE || op == L_MOVE ||
      op == FUSION_P || op == FUSION_S || op == FISSION ||
      op == FILL || op == VOID || op == G_FILL || op == G_VOID) {
    oss << ' ' << d.str();
    if (op == L_MOVE || op == G_FILL || op == G_VOID) {
      oss << ' ' << d2.str();
    } else if (op == FISSION) {
      oss << ' ' << m;
    }
  }
  return oss.str();
}

static void enc_ld(Diff ld, int mx, int* a, int* i) {
  if (ld.x) {
    *a = 1;
    *i = ld.x;
  } else if (ld.y) {
    *a = 2;
    *i = ld.y;
  } else if (ld.z) {
    *a = 3;
    *i = ld.z;
  }

  if (*i < -mx || *i > mx) {
    fprintf(stderr, "Too much i %d\n", *i);
  }
  *i += mx;
}

static int enc_ncd(Diff d) {
  return (d.x + 1) * 9 + (d.y + 1) * 3 + (d.z + 1);
}

void Command::emit(FILE* fp) const {
  switch (op) {
    case HALT:
      fputc(0b11111111, fp);
      break;

    case WAIT:
      fputc(0b11111110, fp);
      break;

    case FLIP:
      fputc(0b11111101, fp);
      break;

    case S_MOVE: {
      int a, i;
      enc_ld(d, 15, &a, &i);
      fputc(0b00000100 | (a << 4), fp);
      fputc(i, fp);
      break;
    }

    case L_MOVE: {
      int a1, i1;
      int a2, i2;
      enc_ld(d, 5, &a1, &i1);
      enc_ld(d2, 5, &a2, &i2);
      fputc(0b00001100 | (a1 << 4) | (a1 << 6), fp);
      fputc(i1 | (i2 << 4), fp);
      break;
    }

    case FUSION_P: {
      fputc(0b111 | (enc_ncd(d) << 3), fp);
      break;
    }

    case FUSION_S: {
      fputc(0b110 | (enc_ncd(d) << 3), fp);
      break;
    }

    case FISSION: {
      fputc(0b101 | (enc_ncd(d) << 3), fp);
      fputc(m, fp);
      break;
    }

    case FILL: {
      fputc(0b011 | (enc_ncd(d) << 3), fp);
      break;
    }

    case VOID: {
      fputc(0b010 | (enc_ncd(d) << 3), fp);
      break;
    }

    case G_FILL: {
      fputc(0b001 | (enc_ncd(d) << 3), fp);
      fputc(d2.x + 30, fp);
      fputc(d2.y + 30, fp);
      fputc(d2.z + 30, fp);
      break;
    }

    case G_VOID: {
      fputc(0b000 | (enc_ncd(d) << 3), fp);
      fputc(d2.x + 30, fp);
      fputc(d2.y + 30, fp);
      fputc(d2.z + 30, fp);
      break;
    }

  }
}

static void checkLinear(const Diff& d, int mx) {
  int cnt = d.nonZeroCount();
  assert(cnt == 1);
  assert(d.x >= -mx);
  assert(d.x <= mx);
  assert(d.y >= -mx);
  assert(d.y <= mx);
  assert(d.z >= -mx);
  assert(d.z <= mx);
}

static void checkNear(const Diff& d) {
  int cnt = d.nonZeroCount();
  assert(cnt != 0);
  assert(cnt != 3);
  int mx = 1;
  assert(d.x >= -mx);
  assert(d.x <= mx);
  assert(d.y >= -mx);
  assert(d.y <= mx);
  assert(d.z >= -mx);
  assert(d.z <= mx);
}

Command Halt() {
  Command c;
  c.op = HALT;
  return c;
}

Command Wait() {
  Command c;
  c.op = WAIT;
  return c;
}

Command Flip() {
  Command c;
  c.op = FLIP;
  return c;
}

Command SMove(Diff d) {
  checkLinear(d, 15);
  Command c;
  c.op = S_MOVE;
  c.d = d;
  return c;
}

Command LMove(Diff d, Diff d2) {
  checkLinear(d, 5);
  Command c;
  c.op = L_MOVE;
  c.d = d;
  c.d2 = d2;
  return c;
}

Command FusionP(Diff d) {
  checkNear(d);
  Command c;
  c.op = FUSION_P;
  c.d = d;
  return c;
}

Command FusionS(Diff d) {
  checkNear(d);
  Command c;
  c.op = FUSION_S;
  c.d = d;
  return c;
}

Command Fission(Diff d, int m) {
  checkNear(d);
  assert(m >= 0);
  Command c;
  c.op = FISSION;
  c.d = d;
  c.m = m;
  return c;
}

Command Fill(Diff d) {
  checkNear(d);
  Command c;
  c.op = FILL;
  c.d = d;
  return c;
}

Command Void(Diff d) {
  checkNear(d);
  Command c;
  c.op = VOID;
  c.d = d;
  return c;
}

Command GFill(Diff d, int x, int y, int z) {
  checkNear(d);
  Command c;
  c.op = G_FILL;
  c.d = d;
  c.d2 = Diff(x, y, z);
  return c;
}

Command GVoid(Diff d, int x, int y, int z) {
  //assert(false);
  checkNear(d);
  Command c;
  c.op = G_VOID;
  c.d = d;
  c.d2 = Diff(x, y, z);
  return c;
}

static Diff dec_ld(int a, int i, int b) {
  i -= b;
  if (a == 0b01) {
    return Diff(i, 0, 0);
  } else if (a == 0b10) {
    return Diff(0, i, 0);
  } else if (a == 0b11) {
    return Diff(0, 0, i);
  } else {
    fprintf(stderr, "Unknown a: %d\n", a);
    abort();
  }
}

static Diff dec_lld(int a, int i) {
  return dec_ld(a, i, 15);
}

static Diff dec_sld(int a, int i) {
  return dec_ld(a, i, 5);
}

static Diff dec_ncd(int d) {
  assert(d >= 0);
  assert(d < 27);
  return Diff(d / 9 - 1, d / 3 % 3 - 1, d % 3 - 1);
}

void parseTrace(const string& filename, vector<Command>* trace) {
  FILE* fp = fopen(filename.c_str(), "rb");
  while (true) {
    int b = fgetc(fp);
    if (b == EOF)
      break;

    if (b == 0b11111111) {
      trace->push_back(Halt());
    } else if (b == 0b11111110) {
      trace->push_back(Wait());
    } else if (b == 0b11111101) {
      trace->push_back(Flip());
    } else if ((b & 0b11001111) == 0b00000100) {
      trace->push_back(SMove(dec_lld((b >> 4) & 0b11, fgetc(fp))));
    } else if ((b & 0b00001111) == 0b00001100) {
      int sld1a = (b >> 4) & 0b11;
      int sld2a = b >> 6;
      int i = fgetc(fp);
      assert(i != EOF);
      int sld1i = i & 0b1111;
      int sld2i = i >> 4;
      trace->push_back(LMove(dec_sld(sld1a, sld1i), dec_sld(sld2a, sld2i)));
    } else if ((b & 0b111) == 0b111) {
      trace->push_back(FusionP(dec_ncd(b >> 3)));
    } else if ((b & 0b111) == 0b110) {
      trace->push_back(FusionS(dec_ncd(b >> 3)));
    } else if ((b & 0b111) == 0b101) {
      trace->push_back(Fission(dec_ncd(b >> 3), fgetc(fp)));
    } else if ((b & 0b111) == 0b011) {
      trace->push_back(Fill(dec_ncd(b >> 3)));
    } else if ((b & 0b111) == 0b010) {
      trace->push_back(Void(dec_ncd(b >> 3)));
    } else if ((b & 0b111) == 0b001) {
      int x = fgetc(fp); assert(x != EOF);
      int y = fgetc(fp); assert(y != EOF);
      int z = fgetc(fp); assert(z != EOF);
      trace->push_back(GFill(dec_ncd(b >> 3), x - 30, y - 30, z - 30));
    } else if ((b & 0b111) == 0b000) {
      int x = fgetc(fp); assert(x != EOF);
      int y = fgetc(fp); assert(y != EOF);
      int z = fgetc(fp); assert(z != EOF);
      trace->push_back(GVoid(dec_ncd(b >> 3), x - 30, y - 30, z - 30));
    } else {
      fprintf(stderr, "Unknown command: %d\n", b);
      abort();
    }
  }
  fclose(fp);
}

void emitTrace(const string& filename, const vector<Command>& trace) {
  FILE* fp = fopen(filename.c_str(), "wb");
  for (const Command& c : trace) {
    c.emit(fp);
  }
  fclose(fp);
}
