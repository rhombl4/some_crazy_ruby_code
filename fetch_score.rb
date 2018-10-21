#!/usr/bin/ruby
# coding: utf-8

require 'open-uri'

Encoding.default_external = 'utf-8'
Encoding.default_internal = 'utf-8'

a = ARGV[0] || 'https://icfpcontest2018.github.io/full/live-standings.html'

c = open(a).read

c = c[/<h3 id="team-0062".*?\/table/m]
a = []
c.scan(/<pre>(.*?)<\/pre>/) do
  a << $1
end

loss_total = 0
loss_tbl = {
  'FA' => 0,
  'FD' => 0,
  'FR' => 0
}

(a.size/3).times do |i|
  n = a[i*3]
  s = a[i*3+2].to_i
  l = 1000 - (s % 1000)
  loss_total += l
  loss_tbl[n[0,2]] += l if loss_tbl[n[0,2]]
  puts "#{n} #{s} #{l}"
end

p loss_total
p loss_tbl
