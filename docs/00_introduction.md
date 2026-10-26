# Introduction

netcdf is a Ruby binding to the [NetCDF](https://www.unidata.ucar.edu/software/netcdf/) C library.  It takes NetCDF itself as known: what this guide has to explain is the Ruby side of the boundary — how a `nc_*` function reads as a Ruby call, what it returns, what it raises, and how bulk data crosses without being copied.

## A literal translation of libnetcdf

The gem translates libnetcdf into Ruby word for word.  Each `nc_*` function is a `NetCDF.nc_*` method with the same name, the same arguments in the same order, and the same meaning.  Only the grammar changes: what C returns through a pointer argument comes back as a Ruby value, and what C reports as a non-zero status is raised.  The vocabulary does not — nothing is grouped, renamed, or given a friendlier spelling — so a call you read in C documentation, in an example, or in someone's Fortran-shaped code is written here as it stands.  The few places where a Ruby call does more than its C counterpart — `nc_put_att` taking the type from the value, an `NC_STRING` read returning Ruby Strings — are marked in the [reference](03_reference.md) where they occur.

Translating rather than redesigning settles three things:

- **Ids stay the integers libnetcdf uses.**  `ncid`, `dimid`, `varid` and `grpid` are passed to you and back unchanged, so a file's identity is the same value on both sides of the boundary and yours to keep wherever you like.
- **Data stays where your array library holds it.**  A buffer here is a block of memory the library reads or writes in place; element access, arithmetic, slicing and type coercion stay with the array class that owns the data.
- **Attributes are stored and returned verbatim.**  `_FillValue`, `scale_factor`, `add_offset` and CF time come back exactly as they were written — a convention is yours to apply, at the point where you know which one the file follows.

A translation this close can be checked against the C API function by function, and it never stands between you and a feature it was not designed for.  The conveniences — an object-oriented surface, masks, CF/COARDS decoding — belong to a layer written above this one.

The practical consequence is that [Unidata's C documentation](https://docs.unidata.ucar.edu/netcdf-c/current/) is documentation for this gem.  When you want to know what `nc_def_var_chunking` does to a file, that is where the answer is; what this guide covers is what the Ruby call returns, what it raises, and how the buffer works.

## Passing data through the MemoryView protocol

Metadata crosses as ordinary Ruby values: names are Strings, ids and lengths are Integers, an attribute goes in as a Ruby value and comes back as one.  Bulk numeric data does not — it is handed over through Ruby's MemoryView protocol, which is what lets a whole array cross the boundary as memory rather than as a converted Ruby object.

A buffer, in this gem, is any object that exposes MemoryView: the gem asks it for the pointer, checks that the memory is row-major contiguous and that its format agrees with the variable, and lets libnetcdf read or write it where it lies.  The gem copies nothing in either direction — what libnetcdf does with the bytes after that, type conversion included, is libnetcdf's business — and the call runs with the GVL released.  An array class that exposes the protocol is therefore usable as a buffer with no adapter written for it.

The gem allocates no buffers of its own: a `get` reads into memory you hand it, as a `put` writes from it.  The gem depends on no array library; this guide's examples take CArray as one that implements MemoryView.

## What is not implemented

- Compound types
- Vlen variable I/O (defining and inquiring vlen types works)
- Parallel I/O
- DAP / pNetCDF backend tuning, log control, and other rarely used knobs

These are deferred rather than out of scope.  The function names are listed at the end of the [reference](03_reference.md#not-yet-implemented).
