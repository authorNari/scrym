# Scrym: Self-collecting Ruby Mutators

A explicit Ruby's memory management system on programmer-controlled time without malloc/free way.

## Usage

Usually Ruby's garbage collector manages all object on the heap.
However, if you use ``Scrym::Mutator.mark(obj)``, GC doesn't care ``obj`` at all time.
Instead, we explicitly collect those on a mutator at one's own risk.

```ruby
require "scrym"
include Scrym

a = "a"
10_000.times do |i|
  a = a.succ
  Mutator.mark(a)
  Mutator.collect if (i % 10).zero?
end
p a #=> "ntq"
```
Scrym collector's targets are only marked objects by Scrym.

## Improvement of performance

```zsh
% time ruby benchmark/bm_app_mandelbrot.rb
GC.count = 1785 : total time 0.7400470000000003(sec)
2.14s user 0.04s system 99% cpu 2.185 total

% time ruby benchmark/bm_app_mandelbrot_with_scrym.rb
GC.count = 1092 : total time 0.40402999999999956(sec)
1.92s user 0.04s system 99% cpu 1.963 total
```

## Installation

Add this line to your application's Gemfile:

    gem 'scrym'

And then execute:

    $ bundle

Or install it yourself as:

    $ gem install scrym

## Supported or Not supported

* Ruby 1.9.3 or later
* Finalizer (All programmers should not use any finalizer)

## Acknowledgment

This product's idea is based on [Self-collecting Mutators](http://tiptoe.cs.uni-salzburg.at/short-term-memory/).
Thanks a lot!

## Contributing

1. Fork it
2. Create your feature branch (`git checkout -b my-new-feature`)
3. Commit your changes (`git commit -am 'Added some feature'`)
4. Push to the branch (`git push origin my-new-feature`)
5. Create new Pull Request
