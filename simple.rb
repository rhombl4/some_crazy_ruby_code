#!/usr/bin/env ruby

require_relative 'asm'
require_relative 'command'
require_relative 'dismodel'

mdl = dismodel(open(ARGV[0], 'r:binary', &:read))
st = Model.new(mdl.r)

bx = 0
by = 0
bz = 0
todos = []
cmds = []
flipped = false

cmds << [Fission, [0, 1, 0], 3]
cmds << [Wait]
cmds << [SMove, [1, 0, 0]]
cmds << [FusionP, [1, 1, 0]]
cmds << [FusionS, [-1, -1, 0]]

goto_xz = proc{|gx, gz|
  while gx != bx
    d = gx - bx
    if d > 0
      d = [d, 15].min
    else
      d = [d, -15].max
    end
    cmds << [SMove, [d, 0, 0]]
    bx += d
  end

  while gz != bz
    d = gz - bz
    if d > 0
      d = [d, 15].min
    else
      d = [d, -15].max
    end
    cmds << [SMove, [0, 0, d]]
    bz += d
  end

}

while true
  #STDERR.puts "pos=(#{bx},#{by},#{bz}) todos=#{todos.size}"

  if todos.empty?
    mdl.r.times do |z|
      mdl.r.times do |x|
        if mdl.get(x, by, z)
          todos << [x, z]
        end
      end
    end

    if todos.empty?
      goto_xz[0, 0]
      while by > 0
        d = [15, by].min
        cmds << [SMove, [0, -d, 0]]
        by -= d
      end
      if flipped
        cmds << [Flip]
      end
      cmds << [Halt]
      break
    end

    cmds << [SMove, [0, 1, 0]]
    by += 1
  end

  oks = todos.filter do |x, z|
    flipped || st.will_be_grounded(x, by-1, z)
  end

  if oks.empty?
    cmds << [Flip]
    flipped = true
    oks = todos
  end

  _, nx, nz = oks.map do |x, z|
    [(bx-x).abs + (bz-z).abs, x, z]
  end.min

  goto_xz[nx, nz]
  cmds << [Fill, [0, -1, 0]]
  st.put(bx, by-1, bz)
  todos.delete([nx, nz])

end

if ARGV[1]
  assemble(cmds, ARGV[1])
else
  cmds.each do |cmd|
    puts cmd * ' '
  end
end
