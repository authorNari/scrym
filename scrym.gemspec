# -*- encoding: utf-8 -*-
require File.expand_path('../lib/scrym/version', __FILE__)

Gem::Specification.new do |gem|
  gem.authors       = ["Narihiro Nakamura"]
  gem.email         = ["authornari@gmail.com"]
  gem.description   = %q{A explicit Ruby's memory management system on programmer-controlled time without malloc/free way.}
  gem.summary       = %q{A explicit Ruby's memory management system on programmer-controlled time without malloc/free way.}
  gem.homepage      = "https://github.com/authorNari/scrym"

  gem.files         = `git ls-files`.split($\)
  gem.executables   = gem.files.grep(%r{^bin/}).map{ |f| File.basename(f) }
  gem.test_files    = gem.files.grep(%r{^(test|spec|features)/})
  gem.name          = "scrym"
  gem.require_paths = ["lib"]
  gem.version       = Scrym::VERSION

  gem.add_development_dependency "rake-compiler", "~> 0.8.1"
end
