#!/usr/bin/env rake
require "bundler/gem_tasks"
require 'rake/clean'

require 'rake/extensiontask'
Rake::ExtensionTask.new('scrym_ext') do |ext|
  ext.ext_dir = 'ext/scrym'
  if ENV["EXT_TEST"]
    ext.config_options << '--with-test'
  end
end

task :default => %w(test)

task :test => [:clean, :compile] do
  sh "/usr/bin/env ruby test/run-test.rb"
end

CLEAN.include "**/*.o", "**/*.so", "**/*.bundle"
