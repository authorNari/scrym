base_dir = File.expand_path(File.dirname(__FILE__))
top_dir = File.expand_path("..", base_dir)
$LOAD_PATH.unshift(File.join(top_dir, "lib"))

require "scrym"
include Scrym

a = "a"
10_000.times do |i|
  a = a.succ
  Mutator.mark(a)
  Mutator.collect if (i % 10).zero?
end
p a #=> "ntq"
