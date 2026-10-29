# netcdf

A Ruby binding to the [NetCDF](https://www.unidata.ucar.edu/software/netcdf/) C library that passes data through Ruby's MemoryView protocol.

The binding is nearly one to one, each C function corresponding to a Ruby method of the same name, so [Unidata's C documentation](https://docs.unidata.ucar.edu/netcdf-c/current/) reads as documentation for it.

## Features

* Provides the same functions as the NetCDF C library, under the same names and with their arguments in the same order
* Variable data is read and written with any buffer object that supports MemoryView
* Ruby's GVL (Global VM Lock) is released while variable data is read or written
* Access to the libnetcdf API is serialized (libnetcdf itself is not thread-safe)

## Install

Requires:

- Ruby >= 3.1
- libnetcdf >= 4.1, with its headers (`nc-config` or `pkg-config` on PATH)
- A C compiler

With those in place, install the gem:

```
gem install netcdf
```

Or add it to your `Gemfile`:

```ruby
gem "netcdf"
```

## Example

Variable data is read and written through the [MemoryView](https://github.com/ruby/ruby/blob/master/include/ruby/memory_view.h) protocol, so any buffer object that supports MemoryView can be the other end of the binary I/O.  The example below uses [CArray](https://github.com/himotoyoshi/carray) to show a NetCDF variable being written and read back.

```ruby
require "netcdf"
require "carray"                                    # an array library that supports MemoryView

ncid = NetCDF.nc_create("out.nc", NetCDF::NC_CLOBBER | NetCDF::NC_NETCDF4)

y = NetCDF.nc_def_dim(ncid, "y", 3)
x = NetCDF.nc_def_dim(ncid, "x", 4)
v = NetCDF.nc_def_var(ncid, "T", NetCDF::NC_DOUBLE, [y, x])

NetCDF.nc_put_att(ncid, v, "units", "K")            # type inferred from the value
NetCDF.nc_put_att(ncid, v, "valid_range", [200.0, 350.0])

NetCDF.nc_enddef(ncid)

src = CArray.double(3, 4).seq(1.0)
NetCDF.nc_put_var(ncid, v, src)

dst = CArray.double(3, 4)                           # a read goes into memory you provide
NetCDF.nc_get_vara(ncid, v, [0, 0], [3, 4], dst)

NetCDF.nc_close(ncid)
```

## Documentation

* [Introduction](docs/00_introduction.md) — what the netcdf gem is, what it leaves to libraries built on it, and what is not implemented
* [Getting started](docs/01_getting_started.md) — install, the shape of the API, writing and reading your first file
* [Buffers and MemoryView](docs/02_buffers.md) — how data actually moves: what a buffer must satisfy, format strings, and handing an array library's memory straight over
* [Reference](docs/03_reference.md) — every function, grouped by subsystem, and every constant

## Contributing

Bug reports, feature requests and pull requests are welcome on [GitHub](https://github.com/himotoyoshi/netcdf/issues).

## Credits

netcdf was designed and reviewed by a human developer; the implementation was produced in collaboration with AI coding tools.

## License

[MIT](LICENSE)
