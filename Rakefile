require "rake/testtask"
require "rake/extensiontask"

Rake::ExtensionTask.new("netcdf") do |ext|
  ext.ext_dir = "ext/netcdf"
  ext.lib_dir = "lib/netcdf"
end

Rake::TestTask.new(:test) do |t|
  t.libs << "test" << "lib"
  t.test_files = FileList["test/test_*.rb"]
end

task test: :compile

task default: :test
