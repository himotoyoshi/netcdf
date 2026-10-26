Gem::Specification.new do |spec|
  spec.name        = "netcdf"
  spec.version     = File.read(File.expand_path("lib/netcdf/version.rb", __dir__))[/VERSION\s*=\s*"([^"]+)"/, 1]
  spec.summary     = "Ruby binding to the NetCDF C library, one Ruby method per C function"
  spec.description = <<~TEXT
    A literal translation of the NetCDF C library into Ruby: every function
    under its own name, with libnetcdf's own documentation to read it by.
    Bulk data is passed through Ruby's MemoryView protocol: an array
    library's own memory is handed to libnetcdf, which reads or writes it
    where it lies, and array I/O runs with the GVL released.
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
