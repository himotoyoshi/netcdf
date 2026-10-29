Gem::Specification.new do |spec|
  spec.name        = "netcdf"
  spec.version     = File.read(File.expand_path("lib/netcdf/version.rb", __dir__))[/VERSION\s*=\s*"([^"]+)"/, 1]
  spec.summary     = "Ruby binding to the NetCDF C library that passes data through Ruby's MemoryView protocol"
  spec.description = <<~TEXT
    A Ruby binding to the NetCDF C library. The binding is nearly one to
    one, each nc_* function corresponding to a Ruby method of the same name
    with its arguments in the same order, so Unidata's C documentation
    reads as documentation for it. Variable data is read and written with
    any buffer object that supports MemoryView.
  TEXT
  spec.authors     = ["himotoyoshi"]
  spec.email       = ["himotoyoshi@users.noreply.github.com"]
  spec.license     = "MIT"
  spec.homepage    = "https://github.com/himotoyoshi/netcdf"

  spec.required_ruby_version = ">= 3.1.0"

  spec.files = Dir[
    "lib/**/*.rb",
    "ext/netcdf/*.{c,h,rb}",
    "README.md",
    "CHANGELOG.md",
    "docs/*.md",
    "LICENSE",
    "netcdf.gemspec",
  ]
  spec.require_paths = ["lib"]
  spec.extensions    = ["ext/netcdf/extconf.rb"]
end
