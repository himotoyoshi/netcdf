# Buffers and MemoryView

Variable data passes through a buffer, and a buffer is any object that supports the MemoryView protocol.  The gem passes the buffer's memory straight to libnetcdf, which reads or writes it in place; nothing is copied on the way in or out.

This page covers what a buffer has to satisfy, what to pass when you have no array library, how a format string is read, and where an array library fits.

Leaving the buffer off raises `ArgumentError: buffer is required`, except for `NC_STRING` variables, which take none (see [below](#types-that-are-not-buffers)).

## A buffer is a block of memory

A Ruby `Array` of Floats is a list of Ruby objects, not a block of memory, so it is not a buffer.  Put the values in an array library's array, or pack them into a `Fiddle::Pointer` ([below](#without-an-array-library)).

A `String` is a block of bytes, but it does not support MemoryView, so it is not a buffer either; copy the bytes into a `Fiddle::Pointer` instead.

Strings are taken elsewhere — attribute text, `NC_STRING` values, and a single `NC_CHAR` or opaque element through `nc_put_var1` — because there the data really is text or an opaque blob.  Only array I/O refuses them.

## What a buffer has to satisfy

- **Supports MemoryView.**  Anything else raises `TypeError: buffer must provide a MemoryView`.  A library's derived arrays may not support it even when its plain arrays do: a `CArray` does, but the array `CArray#transpose` returns does not — pass its `#copy`.
- **Row-major (C-contiguous).**  libnetcdf reads and writes a flat run of elements, so strided or column-major memory is refused with `ArgumentError: buffer must be row-major (C-contiguous)` rather than read wrongly.  Make a contiguous copy first (`CArray#copy`, for a CArray).
- **Large enough.**  The element count implied by `count` (or by the whole variable) must fit, or `ArgumentError: buffer too small` says how many bytes were expected.  Only the size is checked, not the shape: a 4 × 3 buffer serves a 3 × 4 read.
- **Writable, on a `get`.**  A buffer that cannot be written raises `TypeError` (`buffer must provide a writable MemoryView`, or `buffer is read-only`).  `Fiddle::Pointer` is read-only, so it serves as the source of a `put` but not as the destination of a `get`.

The element *type* need not match the variable's: libnetcdf converts.  Writing a float (`"f"`) buffer into an `NC_DOUBLE` variable widens on the way in, and reading that variable into a `"f"` buffer narrows on the way out.  What the buffer's format decides is how libnetcdf interprets your memory, not what ends up on disk.  A value that does not fit the type it is converted to raises `NetCDF::Error: NetCDF: Numeric conversion not representable`.

## Without an array library

`Fiddle::Pointer`, from the standard library, is a block of bytes with no format, and is read and written as the variable's own type (see [below](#format-strings)):

```ruby
require "fiddle"

src = Fiddle::Pointer.malloc(12 * 8, Fiddle::RUBY_FREE)   # 12 doubles
src[0, 12 * 8] = (1..12).map(&:to_f).pack("d*")
NetCDF.nc_put_vara(ncid, varid, [0, 0], [3, 4], src)
```

Nothing checks that the bytes are of the variable's type, so pack them as that type: doubles written into an `NC_FLOAT` variable are stored wrong, with no error.  It serves as the source of a `put`; it cannot be read into (see [what a buffer has to satisfy](#what-a-buffer-has-to-satisfy)).

## Format strings

A buffer describes its element type with a MemoryView `format` string.  The vocabulary is the one `ruby/memory_view.h` specifies — pack-template specifiers:

| format | NetCDF type |
|--------|-------------|
| `c` / `C` | `NC_BYTE` / `NC_UBYTE`, and `NC_CHAR` |
| `s` / `S` | `NC_SHORT` / `NC_USHORT` |
| `i` / `I` | `NC_INT` / `NC_UINT` |
| `q` / `Q` | `NC_INT64` / `NC_UINT64` |
| `f` | `NC_FLOAT` |
| `d` | `NC_DOUBLE` |

`l` / `L` are read as the 32-bit integers they are in that vocabulary, and `e` / `E` / `g` / `G` as the floats that carry their own byte order (below).  The `b` / `B` and `h` / `H` of PEP 3118 (Python's buffer format) are accepted too, for buffers that spell the 1- and 2-byte integers that way.

A format that disagrees with the buffer's element size — Python's 64-bit `l`, say — raises `ArgumentError` rather than being misread.

A format may name a byte order, in Ruby's spelling (`q<`, `s>`, and `e` / `E` / `g` / `G` for floats) or PEP 3118's (`<d`).  Native order is accepted; the foreign order raises `ArgumentError: buffer format "..." is not native-endian`, so swap the bytes before passing them.

Only a single numeric element is accepted.  Repeat counts, records (`T{...}`), and mixed formats (`dd`) have no NetCDF counterpart and raise `ArgumentError: unsupported buffer format`.

A buffer with no format at all is passed through as raw bytes of the variable's own type.  So is any buffer for an opaque variable, whose element size NetCDF already knows and whose data has no element type for a format to name — see [opaque I/O](03_reference.md#opaque-fixed-size-raw-bytes).  An enum variable is read and written as its base type whatever the buffer holds, so a formatted buffer for one must name that base type; another raises `ArgumentError` rather than being reinterpreted.  An `NC_CHAR` buffer holds bytes, so its format, when it has one, must name a 1-byte element.

## Array libraries

An array class that supports MemoryView is a buffer here.  Pass the array where a buffer goes; the example takes CArray as one such class:

```ruby
require "netcdf"
require "carray"   # an array library that supports MemoryView

src = CArray.double(3, 4).seq(1.0)
NetCDF.nc_put_vara(ncid, varid, [0, 0], [3, 4], src)

dst = CArray.double(3, 4)
NetCDF.nc_get_vara(ncid, varid, [0, 0], [3, 4], dst)     # filled in place
```

An array class that does not support it cannot be passed: copy its data into one that does before a `put`, and out of it after a `get`.

## Types that are not buffers

Two of NetCDF's types do not fit into a single block of memory, and the gem hands them to you as Ruby objects instead:

- **`NC_STRING`** — each element is a separately allocated C string, so a `get` takes no buffer and returns a `String` or a nested `Array<String>`, and a `put` accepts the same.  See [NC_STRING variables](03_reference.md#nc_string-variables).
- **Vlen** — a variable-length sequence per element.  Type definition works; data I/O is not wired through yet.

## Threads and the GVL

Ruby's GVL (Global VM Lock) lets only one thread run Ruby code at a time, and a C call that holds it on the way through stops every other thread until it returns.  Array I/O releases the GVL around the libnetcdf call, so while a large read or write waits on the disk or decompresses chunks, other Ruby threads keep running — one serving requests, one reporting progress, one working on data already read.  The buffer must not be touched from another thread while the call is in flight — the gem holds a raw pointer into it for the duration.

libnetcdf, and the HDF5 under it, are not thread-safe: two threads inside them at once corrupt process-wide state, even when they work on different files.  So every `NetCDF` call — array I/O, and inquiry, definition, open and close alike — takes one process-wide lock.  A thread that calls into NetCDF while another is in the middle of a read waits for it, with the GVL released; threads doing anything else keep running.  Threads therefore do not speed up NetCDF I/O itself, only the Ruby work around it.  The lock covers one call at a time: a sequence of calls that must not interleave with another thread's — define, then write, say — still needs a lock of your own.

`NetCDF` can be called only from the main Ractor.  A call from any other Ractor raises an error.  For NetCDF I/O that runs in parallel, use separate processes.
