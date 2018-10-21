#!/usr/bin/env ruby

def run_sim(args, nbt)
  cmd = "./sim #{args * ' '} #{nbt}"
  #puts cmd
  `#{cmd}`[/energy=(\d+)/, 1].to_i
end

Dir.glob('fsota/*').sort.each do |sota|
  name = sota.sub('fsota/', '').sub('.nbt', '')
  args = []
  if name =~ /^FA/
    args << "fmodels/#{name}_tgt.mdl"
  elsif name =~ /^FD/
    args << "fmodels/#{name}_src.mdl"
  elsif name =~ /^FR/
    args << "fmodels/#{name}_src.mdl"
    args << "fmodels/#{name}_tgt.mdl"
  else
    raise
  end

  e = run_sim(args, sota)
  puts "#{name}: #{e}"
end
