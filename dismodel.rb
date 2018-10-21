#!/usr/bin/env ruby

class Model
  attr_reader :r

  def initialize(r)
    @r = r
    @m = '0' * (@r ** 3)
    @is_grounded = true
  end

  def get(x, y, z)
    if x < 0 || x >= @r || y < 0 || y >= @r || z < 0 || z >= @r
      return nil
    end

    i = x * @r * @r + y * @r + z
    @m[i] == '1'
  end

  def put(x, y, z)
    i = x * @r * @r + y * @r + z
    @m[i] = '1'

    if !will_be_grounded(x, y, z)
      @is_grounded = false
    end
  end

  def will_be_grounded(x, y, z)
    (y == 0 ||
     get(x-1, y, z) || get(x+1, y, z) ||
     get(x, y-1, z) || get(x, y+1, z) ||
     get(x, y, z-1) || get(x, y, z+1))
  end

  def dump
    puts "r=#{@r} is_grounded=#{@is_grounded}"
    @r.times do |y|
      puts "y=#{y}"
      @r.times do |z|
        @r.times do |x|
          ch = get(x, y, @r-z-1) ? 'O' : '_'
          print ch
        end
        puts
      end
    end
  end

end

def dismodel(c)
  r = c[0].ord
  m = Model.new(r)
  i = 8
  getb = proc{
    b = (c[i/8].ord >> (i%8)) & 1
    i += 1
    b
  }

  r.times do |x|
    r.times do |y|
      r.times do |z|
        if getb[] == 1
          m.put(x, y, z)
        end
      end
    end
  end
  m
end

if $0 == __FILE__
  c = open(ARGV[0], 'r:binary', &:read)
  m = dismodel(c)
  m.dump
end
