# netcdf

A Ruby binding to the [NetCDF](https://www.unidata.ucar.edu/software/netcdf/) C library that passes data through Ruby's MemoryView protocol.  An array library's own memory is handed to libnetcdf, which reads or writes it where it lies: nothing is copied, and no Ruby Array stands in between.

The binding is nearly one to one, for code that wants to be written the way NetCDF itself is written; [Unidata's C documentation](https://docs.unidata.ucar.edu/netcdf-c/current/) reads as documentation for it.

## Features

* Programs are written the way NetCDF's own are: every `nc_*` function under its own name, taking its arguments in its own order, so a call in a C example is written here as it stands
* Any MemoryView producer is a buffer — CArray, an Arrow array, `Fiddle::Pointer` — with no adapter written for it, and libnetcdf reads or writes its memory where it lies
* Array I/O releases the GVL, so other threads keep running through a long read or write; netCDF calls themselves are serialized, since libnetcdf is not thread-safe

What libnetcdf offers is bound and covered by the test suite, apart from a handful of features listed at the end of the [reference](docs/03_reference.md#not-yet-implemented).

## Install

Install libnetcdf and its headers first, then the gem:

```
gem install netcdf
```

Or add it to your `Gemfile`:

```ruby
gem "netcdf"
```

Requires:

- Ruby >= 3.1
- libnetcdf >= 4.1, with its headers (`nc-config` or `pkg-config` on PATH)
- A C compiler

## Example

Arrays are passed to and from this gem through [MemoryView](https://github.com/ruby/ruby/blob/master/include/ruby/memory_view.h), the protocol Ruby 3.0 added for sharing an array's memory between libraries.  An array library that implements it works here as it is, with no adapter written on either side; the gem itself depends on no array library.  The example below takes [CArray](https://github.com/himotoyoshi/carray) as one such library: it writes a 3 × 4 variable with two attributes, and reads it back.

```ruby
require "netcdf"
require "carray"

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

libnetcdf reads and writes the array's own memory, with nothing copied in between.

## Documentation

* [Introduction](docs/00_introduction.md) — what this gem is, what it leaves to a layer above, and what is not implemented
* [Getting started](docs/01_getting_started.md) — install, the shape of the API, writing and reading your first file
* [Buffers and MemoryView](docs/02_buffers.md) — how data actually moves: what a buffer must satisfy, format strings, and handing an array library's memory straight over
* [Reference](docs/03_reference.md) — every function, grouped by subsystem, plus every constant

## Contributing

Bug reports, feature requests and pull requests are welcome.

## Credits

netcdf was designed and reviewed by a human developer; the implementation was produced in collaboration with AI coding tools.

## License

MIT
