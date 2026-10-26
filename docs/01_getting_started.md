# Getting started

This page goes from an empty directory to a NetCDF file written and read back.  NetCDF itself is taken as known; what is shown is how a `nc_*` call is written in Ruby, and which of its edges are Ruby's rather than C's.

## Install

Install libnetcdf and its headers first, then the gem:

```
gem install netcdf
```

The build finds libnetcdf through `nc-config` or `pkg-config`, so whichever of those the install put on your `PATH` is what gets used.  Check what you ended up with:

```ruby
require "netcdf"
NetCDF.nc_inq_libvers      # => "4.9.2 of Mar 15 2024 ..."
```

The examples below use [CArray](https://github.com/himotoyoshi/carray), a multi-dimensional array library that can be both a producer and a consumer of MemoryView:

```
gem install carray
```

## What is Ruby rather than C

`NetCDF.nc_open`, `NetCDF.nc_def_var`, `NetCDF.nc_put_vara`: the names and the argument order are the ones you already know.  Four things are spelled Ruby's way instead of C's.

- **Status codes are exceptions.**  A call that fails raises `NetCDF::Error`; there is no return value to check.
- **Output parameters are return values.**  `nc_inq_var` hands back its five values as an Array instead of filling five pointers.
- **A lookup that finds nothing returns `nil`.**  `nc_inq_varid` on a name that is not in the file, for one — absence is not an error.
- **Bulk data moves through MemoryView.**  A buffer is memory libnetcdf reads or writes where it lies, so an array library's own storage is handed over as it stands.

## Define mode and data mode

`nc_enddef` and `nc_redef` are here as they are in C, and the gem does nothing to hide the two phases — but how strictly they are enforced depends on the format, which is worth seeing before the first example.  A classic (NetCDF-3) file refuses a write before `nc_enddef`:

```
NetCDF::Error: NetCDF: Operation not allowed in define mode   # status -39
```

A NetCDF-4 file does not — libnetcdf switches modes for you there, and the write goes through.  Call `nc_enddef` anyway, at the point where the schema is complete: it is what makes the same code work against both formats, and it marks the boundary for a reader.

Opening an existing file skips all of this — the file is already in data mode.

## Writing a file

```ruby
require "netcdf"
require "carray"   # a MemoryView producer/consumer

ncid = NetCDF.nc_create("temp.nc", NetCDF::NC_CLOBBER | NetCDF::NC_NETCDF4)

# --- define mode ---
y = NetCDF.nc_def_dim(ncid, "y", 3)
x = NetCDF.nc_def_dim(ncid, "x", 4)

v = NetCDF.nc_def_var(ncid, "temperature", NetCDF::NC_DOUBLE, [y, x])

NetCDF.nc_put_att(ncid, v, "units", "K")
NetCDF.nc_put_att(ncid, v, "valid_range", [200.0, 350.0])
NetCDF.nc_put_att(ncid, NetCDF::NC_GLOBAL, "title", "example field")

NetCDF.nc_enddef(ncid)

# --- data mode ---
values = CArray.double(3, 4).seq(271.0)
NetCDF.nc_put_var(ncid, v, values)

NetCDF.nc_close(ncid)
```

Four things in there are worth naming:

- Creation flags are OR-ed together as in C: `NC_CLOBBER | NC_NETCDF4` here.  A constant is `NetCDF::NC_*`, and every one libnetcdf defines is in the [reference](03_reference.md).
- Dimension ids come back from `nc_def_dim` and go into `nc_def_var` as an Array, in the order the variable is indexed.
- `nc_put_att` picks the on-disk type from the Ruby value — `String` becomes `NC_CHAR`, `Float` becomes `NC_DOUBLE`, an Array of Floats becomes an `NC_DOUBLE` vector.  When you need to pin the type down, the typed calls (`nc_put_att_float` and friends) are in the [reference](03_reference.md#typed-putget).
- The data goes in as a `CArray`, not a Ruby Array: libnetcdf reads the array's own memory.  Any other MemoryView producer would do; that is the subject of the [next page](02_buffers.md).

## Reading it back

```ruby
require "netcdf"
require "carray"   # a MemoryView producer/consumer

ncid = NetCDF.nc_open("temp.nc", NetCDF::NC_NOWRITE)

varid = NetCDF.nc_inq_varid(ncid, "temperature")
name, xtype, ndims, dimids, natts = NetCDF.nc_inq_var(ncid, varid)
shape = dimids.map { |d| NetCDF.nc_inq_dimlen(ncid, d) }
shape                                   # => [3, 4]

NetCDF.nc_get_att(ncid, varid, "units") # => "K"

buf = CArray.double(*shape)
NetCDF.nc_get_var(ncid, varid, buf)     # => buf, filled
buf[0, nil].to_a                        # => [271.0, 272.0, 273.0, 274.0]

NetCDF.nc_close(ncid)
```

A `get` reads into the buffer you pass, as a `put` writes from it, and returns that buffer.  The gem allocates none of its own: the shape comes from the dimensions, and the element type is yours to choose — libnetcdf converts from the variable's type to it.  The one exception is an `NC_STRING` variable, which comes back as Ruby Strings and takes no buffer.

## Reading part of a variable

`nc_get_vara` takes a corner and a shape as Ruby Arrays, zero-based as in C:

```ruby
rows = CArray.double(2, 4)
NetCDF.nc_get_vara(ncid, varid, [1, 0], [2, 4], rows)   # last two rows
```

`nc_get_vars` adds a stride, and `nc_get_varm` a mapping vector for transposing on the way out.  Each has a `put` twin taking the same arguments plus the buffer.

## When something fails

```ruby
begin
  NetCDF.nc_open("missing.nc")
rescue NetCDF::Error => e
  e.message      # => "No such file or directory" — libnetcdf's own text
  e.status       # => 2   the raw libnetcdf status
end
```

`#status` is what to match on when you care about a specific failure rather than any failure.  NetCDF's own conditions are negative (`-39` is "operation not allowed in define mode"); a positive status is an errno the C library passed through, as above.
