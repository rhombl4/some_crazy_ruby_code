require_relative 'command'

def enc_ld(ld, mx)
  if ld.count(0) != 2
    raise "Invalid ld: #{ld}"
  end

  a, i = if ld[0] != 0
           [1, ld[0]]
         elsif ld[1] != 0
           [2, ld[1]]
         else
           [3, ld[2]]
         end

  if i < -mx || i > mx
    raise "Too much i #{i}"
  end

  [a, i + mx]
end

def enc_ncd(d)
  d.each do |v|
    if v < -1 || v > 1
      raise "Too much ncd: #{d}"
    end
  end

  (d[0] + 1) * 9 + (d[1] + 1) * 3 + (d[2] + 1)
end

def assemble(cmds, out)
  File.open(out, 'w:binary') do |of|
    cmds.each do |cmd|
      case cmd[0]
      when Halt
        of.putc(0b11111111)

      when Wait
        of.putc(0b11111110)

      when Flip
        of.putc(0b11111101)

      when SMove
        a, i = enc_ld(cmd[1], 15)
        of.putc(0b00000100 | (a << 4))
        of.putc(i)

      when LMove
        a1, i1 = enc_ld(cmd[1], 5)
        a2, i2 = enc_ld(cmd[2], 5)
        of.putc(0b00001100 | (a1 << 4) | (a1 << 6))
        of.putc(i1 | (i2 << 4))

      when FusionP
        of.putc(0b111 | (enc_ncd(cmd[1]) << 3))

      when FusionS
        of.putc(0b110 | (enc_ncd(cmd[1]) << 3))

      when Fission
        of.putc(0b101 | (enc_ncd(cmd[1]) << 3))
        of.putc(cmd[2])

      when Fill
        of.putc(0b011 | (enc_ncd(cmd[1]) << 3))

      end
    end
  end
end
