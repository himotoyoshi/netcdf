# Buffers and MemoryView

Every numeric value this gem reads or writes passes through a buffer, and a buffer is any object implementing Ruby's MemoryView protocol.  That is the whole interop story: an array library that exposes MemoryView hands libnetcdf its own memory, and the gem copies nothing on the way in or out.

This page is that surface: what a buffer has to satisfy, what to pass when you have no array library, how a format string is read, and where an array library fits.

The gem allocates no buffers itself.  A `get` reads into the buffer you pass and returns it, just as a `put` writes from one; leaving the buffer off raises `ArgumentError: buffer is required`.  `NC_STRING` variables are the exception (see [the end of this page](#types-that-are-not-buffers)).

## A buffer is a block of memory

Not a collection.  A Ruby `Array` of Floats is a list of boxed objects; there is nothing in it to hand to a C function that wants twelve contiguous 8-byte values, and packing one is a copy plus a per-element conversion — work that belongs to whatever owns the data, not to this boundary.

A `String` *is* a block of bytes, but it carries no shape, no element type, and no writability guarantee, so the array I/O surface does not take one.  Hand the bytes over through a producer instead — an array library's array, or a `Fiddle::Pointer` ([below](#without-an-array-library)).

Strings do appear elsewhere — `NC_CHAR` and `NC_STRING` values, opaque scalars, attribute text — because there the data really is text or an opaque blob.  It is only the numeric array surface that refuses them.

## What a buffer has to satisfy

- **A MemoryView producer.**  Anything else raises `TypeError: buffer must provide a MemoryView`.  Note that a library's view objects are often not producers even when its arrays are — a `CArray` is one, `CArray#transpose` is not.
- **Row-major (C-contiguous).**  libnetcdf reads and writes a flat run of elements, so a strided or column-major view has no meaning at this boundary.  A producer that hands over a strided view — including a 1-D one that gives strides without a shape — is refused with `ArgumentError: buffer must be row-major (C-contiguous)` rather than read wrongly.  Make a contiguous copy first — `CArray#copy`, for a CArray view.
- **Large enough.**  The element count implied by `count` (or by the whole variable) must fit, or `ArgumentError: buffer too small` says how many bytes were expected.
- **Writable, on a `get`.**  A producer that refuses a writable view raises `TypeError: buffer must provide a writable MemoryView`.  One that hands back a view marked read-only anyway — `rb_memory_view_get` does not hold a producer to the request — raises `TypeError: buffer is read-only`, rather than being written into.  `Fiddle::Pointer` is one up to fiddle 1.1.8: it marks every view read-only, so it serves as the source of a `put` but not as the destination of a `get`.  fiddle 1.1.9 hands out a writable view when asked.

The element *type* need not match the variable's: libnetcdf converts.  Writing a `"f"` buffer into an `NC_DOUBLE` variable widens on the way in, and reading that variable into a `"f"` buffer narrows on the way out.  What the buffer's format decides is how libnetcdf interprets your memory, not what ends up on disk.

## Without an array library

`Fiddle::Pointer`, from the standard library, is a block of bytes with no format, and is read and written as the variable's own type (see [below](#format-strings)):

```ruby
require "fiddle"

src = Fiddle::Pointer.malloc(12 * 8, Fiddle::RUBY_FREE)   # 12 doubles
src[0, 12 * 8] = (1..12).map(&:to_f).pack("d*")
NetCDF.nc_put_vara(ncid, varid, [0, 0], [3, 4], src)
```

It serves as the source of a `put`.  It can be read into only from fiddle 1.1.9 on (see [what a buffer has to satisfy](#what-a-buffer-has-to-satisfy)); `#to_str(nbytes)` then copies the bytes out.

## Format strings

A buffer describes its element type with a MemoryView `format` string.  The vocabulary is the one `ruby/memory_view.h` specifies — pack-template specifiers — and this gem expects it:

| format | NetCDF type |
|--------|-------------|
| `c` / `C` | `NC_BYTE` / `NC_UBYTE`, and `NC_CHAR` |
| `s` / `S` | `NC_SHORT` / `NC_USHORT` |
| `i` / `I` | `NC_INT` / `NC_UINT` |
| `q` / `Q` | `NC_INT64` / `NC_UINT64` |
| `f` | `NC_FLOAT` |
| `d` | `NC_DOUBLE` |

`l` / `L` are read as the 32-bit integers they are in that vocabulary, and `e` / `E` / `g` / `G` as the floats that carry their own byte order (below).  PEP 3118's `b` / `B` and `h` / `H` are accepted too, for producers that spell the 1- and 2-byte integers PEP's way.

A format that disagrees with the buffer's own `item_size` — numpy's 64-bit `l` arriving untranslated, say — raises `ArgumentError` rather than being misread.

A format may name a byte order.  Ruby spells it as a suffix on an integer (`q<`, `s>`) and as a letter of its own on a float (`e` / `E` little-endian single / double, `g` / `G` big-endian); a suffix on a float (`d<`) is not Ruby's and is refused as unsupported.  PEP 3118 spells it as a prefix on the whole string (`<d`, plus the order-neutral `=` `@` `|` `^`).  All of these are read.  One naming the *foreign* order raises `ArgumentError: buffer format "..." is not native-endian` — libnetcdf transfers the buffer as native memory, so byte-swapping is the producer's job.

Only a single numeric element is accepted.  Repeat counts, records (`T{...}`), and mixed formats (`dd`) have no netCDF counterpart and are rejected.

A buffer with no format at all is passed through as raw bytes of the variable's own type.  So is any buffer for an opaque variable, whose element size netCDF already knows and whose data has no element type for a format to name — see [opaque I/O](03_reference.md#opaque-fixed-size-raw-bytes).  An enum variable moves its base type whatever the buffer holds, so a formatted buffer for one must name that base type; another raises `ArgumentError` rather than being reinterpreted.  An `NC_CHAR` buffer moves bytes, so its format, when it has one, must name a 1-byte element.

## Array libraries

An array class that exposes MemoryView natively is a buffer here with no adapter written for it and nothing copied on the way through.  Pass the array where a buffer goes; the example takes CArray as one such class:

```ruby
require "netcdf"
require "carray"   # a MemoryView producer/consumer

src = CArray.double(3, 4).seq(1.0)
NetCDF.nc_put_vara(ncid, varid, [0, 0], [3, 4], src)

dst = CArray.double(3, 4)
NetCDF.nc_get_vara(ncid, varid, [0, 0], [3, 4], dst)     # filled in place
```

An array class that does not expose it has its data copied into one that does, once in each direction.

The gem's own code names no library: which of the two paths yours takes is decided by the protocol alone, and it moves to the first the moment it grows a MemoryView entry.

## Types that are not buffers

Two of NetCDF's types do not fit into a single block of memory, and the gem hands them to you as Ruby objects instead:

- **`NC_STRING`** — each element is a separately allocated C string, so a `get` takes no buffer and returns a `String` or a nested `Array<String>`, and a `put` accepts the same.  See [NC_STRING variables](03_reference.md#nc_string-variables).
- **Vlen** — a variable-length sequence per element.  Type definition works; data I/O is not wired through yet.

## The GVL

Array I/O releases the GVL around the libnetcdf call, so a large read or write does not block other Ruby threads.  The buffer must not be touched from another thread while the call is in flight — the gem holds a raw pointer into it for the duration.

libnetcdf, and the HDF5 under it, are not thread-safe: two threads inside them at once corrupt process-wide state, even when they work on different files.  So every `NetCDF` call — array I/O, and inquiry, definition, open and close alike — takes one process-wide lock.  A thread that calls into NetCDF while another is in the middle of a read waits for it, with the GVL released; threads doing anything else keep running.  Threads therefore do not speed up netCDF I/O itself, only the Ruby work around it.  The lock covers one call at a time: a sequence of calls that must not interleave with another thread's — define, then write, say — still needs a lock of your own.

`NetCDF` can be called only from the main Ractor.  A call from any other Ractor raises an error.  For netCDF I/O that runs in parallel, use separate processes.
