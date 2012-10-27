require 'complex'

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
    ary << c if mandelbrot?(c)
  }
}

puts "GC.count = #{GC.count} : total time #{GC::Profiler.total_time}(sec)"
