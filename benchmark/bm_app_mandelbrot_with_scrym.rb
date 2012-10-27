base_dir = File.expand_path(File.dirname(__FILE__))
top_dir = File.expand_path("..", base_dir)
$LOAD_PATH.unshift(File.join(top_dir, "lib"))

require 'scrym'
require 'complex'

include Scrym
GC::Profiler.enable

def mandelbrot? z
  i = 0
  while i<100
    i += 1
    z = z * z
    return false if z.abs > 2
  end
  true
end

ary = []

(0..1000).each{|dx|
  (0..1000).each{|dy|
    x = dx / 50.0
    y = dy / 50.0
    c = Complex(x, y)
    Mutator.mark(c)
    ary << c if mandelbrot?(c)
    Mutator.collect if (dy % 10) == 0
  }
}

puts "GC.count = #{GC.count} : total time #{GC::Profiler.total_time}(sec)"
