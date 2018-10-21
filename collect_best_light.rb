#!/usr/bin/env ruby

models = Dir.glob('models/*.mdl').sort
models.each do |mdl|
  name = mdl[/\/(.*)_tgt\.mdl$/, 1]
  puts name

  base = "traces/#{name}.nbt"
  traces = [base]
  sota = "sota/#{name}.nbt"
  traces += Dir.glob(sota)
  traces += Dir.glob("out2/#{name}.nbt")
  traces += Dir.glob("out/*/#{name}.nbt")

  results = []
  dflt_en = 1

  traces.each do |nbt|
    r = `./sim #{mdl} #{nbt} 2>&1`
    if r =~ /^energy=(\d+)/
      en = $1.to_i
      results << [en, nbt]
      if nbt == base
        dflt_en = en
      end
    else
      raise r
    end
  end

  results.sort!
  energy = results[0][0]
  fn = results[0][1]

  puts "#{name} min_energy=#{energy} score=#{1.0*dflt_en/energy} #{fn}"

  system("cp #{fn} sota")
end
