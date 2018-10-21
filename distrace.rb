#!/usr/bin/ruby

require_relative 'command'

def dec_ld(a, i, b)
  i -= b
  if a == 0b01
    [i, 0, 0]
  elsif a == 0b10
    [0, i, 0]
  elsif a == 0b11
    [0, 0, i]
  else
    raise "Unknown a: #{a}"
  end
end

def dec_lld(a, i)
  dec_ld(a, i, 15)
end

def dec_sld(a, i)
  dec_ld(a, i, 5)
end

def dec_ncd(d)
  [d / 9 - 1, d / 3 % 3 - 1, d % 3 - 1]
end

def distrace(c)
  i = 0
  getb = proc{
    b = c[i].ord
    i += 1
    b
  }

  while c[i]
    b = getb[]
    cmd =
      if b == 0b11111111
        [Halt]
      elsif b == 0b11111110
        [Wait]
      elsif b == 0b11111101
        [Flip]
      elsif b & 0b11001111 == 0b00000100
        [SMove, dec_lld((b >> 4) & 0b11, getb[] & 0b00011111)]
      elsif b & 0b00001111 == 0b00001100
        sld1a = (b >> 4) & 0b11
        sld2a = b >> 6
        i = getb[]
        sld1i = i & 0b1111
        sld2i = i >> 4
        [LMove, dec_sld(sdl1a, sdl1i), dec_sld(sdl2a, sdl2i)]
      elsif b & 0b111 == 0b111
        [FusionP, dec_ncd(b >> 3)]
      elsif b & 0b111 == 0b110
        [FusionS, dec_ncd(b >> 3)]
      elsif b & 0b111 == 0b101
        [Fission, dec_ncd(b >> 3), getb[]]
      elsif b & 0b111 == 0b011
        [Fill, dec_ncd(b >> 3)]
      elsif b & 0b111 == 0b010
        [Void, dec_ncd(b >> 3)]
      elsif b & 0b111 == 0b001
        x = getb[]
        y = getb[]
        z = getb[]
        [GFill, dec_ncd(b >> 3), x-30, y-30, z-30]
      elsif b & 0b111 == 0b000
        x = getb[]
        y = getb[]
        z = getb[]
        [GVoid, dec_ncd(b >> 3), x-30, y-30, z-30]
      else
        raise "Unknown command: #{b}"
      end

    yield cmd
  end
end

if $0 == __FILE__
  c = open(ARGV[0], 'r:binary', &:read)
  distrace(c) do |cmd|
    puts cmd * ' '
  end
end
