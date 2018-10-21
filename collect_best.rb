#!/usr/bin/env ruby

require '/home/i/wrk/drdr/drdr'

mode = 'FA'
model_glob = 'fmodels/FA*_tgt.mdl'
if ARGV[0] == 'FD'
  model_glob = 'fmodels/FD*_src.mdl'
  mode = 'FD'
elsif ARGV[0] == 'FR'
  model_glob = 'fmodels/FR*_src.mdl'
  mode = 'FR'
end

models = Dir.glob(model_glob).sort
models.each do |mdl|
  if mode == 'FA'
    name = mdl[/\/(.*)_tgt\.mdl$/, 1]
  else
    name = mdl[/\/(.*)_src\.mdl$/, 1]
  end

  traces = []
  base = "ftraces/#{name}.nbt"
  traces += [base]
  sota = "fsota/#{name}.nbt"
  traces += Dir.glob(sota)
  #traces += Dir.glob("out2/#{name}.nbt")
  #traces += Dir.glob("out/vanish/#{name}.nbt")
  traces += Dir.glob("out/*/#{name}.nbt")

  #traces = traces.reject{|t|t[/\/slice\//]}
  #if mode != 'FD'
  #  traces = traces.reject{|t|t[/slice/]}
  #end

  results = []
  dflt_en = 1
  sota_en = nil

  drdr(log: '') {
    traces.each do |nbt|
      #task {
        enf = "energy/" + nbt.gsub('/', '_')
        if mode == 'FR'
          mdl2 = "fmodels/#{name}_tgt.mdl"
          cmd = "./sim #{mdl} #{mdl2} #{nbt} 2>&1 > #{enf}"
        else
          cmd = "./sim #{mdl} #{nbt} 2>&1 > #{enf}"
        end
        system(cmd)
      #}
    end
  }

  traces.each do |nbt|
    enf = "energy/" + nbt.gsub('/', '_')
    r = File.read(enf)
    if r =~ /^energy=(\d+)/
      en = $1.to_i
      results << [en, nbt]
      if nbt == base
        dflt_en = en
      end
      if nbt == sota
        sota_en = en
      end
    else
      STDERR.puts "cmd=#{nbt}\n#{r}"
    end
  end

  results.sort!
  energy = results[0][0]
  fn = results[0][1]

  cat = '???'
  results.each do |en, nbt|
    if en == energy
      if nbt == base
        cat = 'dflt'
      elsif nbt =~ /out\/([^\/]+)/
        cat = "#$1"
      elsif nbt == sota
      else
        raise nbt
      end
    end
  end

  score = '%.2f'%(1.0*dflt_en/energy)
  puts "#{name} energy=#{energy} cat=#{cat} score=#{score} #{fn}"

  if energy != sota_en
    system("cp #{fn} fsota")
  end
end
