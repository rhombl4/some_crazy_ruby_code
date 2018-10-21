#!/usr/bin/env ruby

def run_sim(sm, tm, nbt)
  cmd = "./sim #{sm} #{tm} #{nbt}"
  puts cmd
  `#{cmd}`[/energy=(\d+)/, 1].to_i
end

names = []
Dir.glob('fmodels/FR*_src.mdl').sort.each do |mdl|
  name = mdl[/(FR.*)_src\.mdl/, 1]
  names << name
end

names.each do |name|
  a = []
  a << "fmodels/#{name}_src.mdl"
  a << "fmodels/#{name}_tgt.mdl"
  ssota = "fr_out/src_sota/#{name}.nbt"
  if !File.exist?(ssota)
    ssota = "fsota/#{name}.nbt"
    raise if !File.exist?(ssota)
  end
  a << ssota
  a << "fr_out/tgt_sota/#{name}.nbt"
  if !a.all?{|fn|
       if File.exist?(fn)
         true
       else
         puts "#{fn} not exist"
         false
       end
     }
    next
  end

  a << "fr_out/sota/#{name}.nbt"

  if !system((['./merge_fr'] + a) * ' ')
    raise
  end

  new_s = run_sim(a[0], a[1], a[4])
  old_s = run_sim(a[0], a[1], "fsota/#{name}.nbt")
  puts "#{name}: #{old_s} => #{new_s}"

  if old_s > new_s
    puts "new record!"
    system("cp #{a[4]} fsota/#{name}.nbt")
  end

end
