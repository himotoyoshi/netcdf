# Introduction

The netcdf gem is a Ruby binding to the [NetCDF](https://www.unidata.ucar.edu/software/netcdf/) C library.  This guide covers the Ruby side of the boundary: how a `nc_*` function reads as a Ruby call, what it returns, what it raises, and how variable data crosses without being copied.

## A literal translation of libnetcdf

The gem translates libnetcdf into Ruby word for word.  Each `nc_*` function in the NetCDF C library is a `NetCDF.nc_*` method with the same name and the same arguments in the same order.  Only the grammar changes: what C returns through a pointer argument comes back as a Ruby value, and what C reports as a non-zero status is raised as a `NetCDF::Error`.  The vocabulary does not, so a call you read in C documentation or in an example is written here as it stands.  The few places where a Ruby call differs from its C counterpart — `nc_put_att` taking the type from the value, an `NC_STRING` read returning Ruby Strings, a lookup that finds nothing returning `nil` — are marked in the [reference](03_reference.md) where they occur.

Translating rather than redesigning settles three things:

- **Ids stay the integers libnetcdf uses.**  `ncid`, `dimid`, `varid` and `grpid` are plain Integers, passed to you and back unchanged, with no Ruby object wrapped around them.
- **Data stays where your array library holds it.**  libnetcdf reads or writes its memory in place; element access, arithmetic, slicing and type coercion stay with the array class that owns the data.
- **Attributes are stored and returned verbatim.**  `_FillValue`, `scale_factor`, `add_offset` and CF time come back exactly as they were written — a convention is yours to apply.

A translation this close can be checked against the C API function by function.  The conveniences — an object-oriented API, masks, CF/COARDS decoding — belong in a separate library built on this gem.

The practical consequence is that [Unidata's C documentation](https://docs.unidata.ucar.edu/netcdf-c/current/) is documentation for this gem.  When you want to know what `nc_def_var_chunking` does to a file, that is where the answer is.

## Passing data through the MemoryView protocol

Metadata crosses as ordinary Ruby values: names are Strings, ids and lengths are Integers, an attribute goes in as a Ruby value and comes back as one.  Variable data does not — it is handed over through the MemoryView protocol, which is what lets a whole array cross the boundary as memory rather than as a converted Ruby object.

A buffer, in this gem, is any object that supports MemoryView: the gem asks it for the pointer, checks that the memory is row-major contiguous and that its format is one the gem can read, and lets libnetcdf read or write it where it lies.  The gem copies nothing in either direction — what libnetcdf does with the bytes after that, type conversion included, is libnetcdf's business.

The gem allocates no buffers of its own: a `get` reads into memory you hand it, as a `put` writes from it.  The gem depends on no array library; this guide's examples take [CArray](https://github.com/himotoyoshi/carray) as one that supports MemoryView.  What a buffer must satisfy is in [Buffers and MemoryView](02_buffers.md).

## What is not implemented

- Compound types
- Vlen variable I/O (defining and inquiring vlen types works)
- Parallel I/O
- DAP / pNetCDF backend tuning, log control, and other rarely used knobs

These are deferred rather than out of scope.  The function names are listed at the end of the [reference](03_reference.md#not-yet-implemented).
