# Reference

Every function this gem defines, grouped by subsystem.  [Getting started](01_getting_started.md) is the place to begin if this is your first look; buffers have [their own page](02_buffers.md).

## Contents

| Section | |
|---|---|
| [Conventions](#conventions) | What is Ruby about this binding |
| [File](#file) | Create, open, mode, inquiry, [in-memory files](#in-memory-files), [Zarr stores](#zarr-stores) |
| [Dimension](#dimension) | Define and inquire dimensions |
| [Variable](#variable) | Define and inquire variables, [scalar](#variable-scalar-io) and [array](#variable-array-io) I/O, [NC_STRING](#nc_string-variables), [NetCDF-4 options](#netcdf-4-per-variable-options) |
| [Attribute](#attribute) | Inquiry, [smart](#smart-generic-putget) and [typed](#typed-putget) put/get |
| [User-defined types](#user-defined-types-netcdf-4-only) | [Opaque](#opaque-fixed-size-raw-bytes), [vlen](#vlen-variable-length-sequence), [enum](#enum-named-integer-constants) |
| [Group](#group-netcdf-4-only) | NetCDF-4 group hierarchy |
| [Constants](#constants) | Every `NC_*` the gem defines |
| [Buffers](#buffers) | Pointer to the buffer page |
| [Error handling](#error-handling) | `NetCDF::Error` and its status |
| [Not yet implemented](#not-yet-implemented) | What is deliberately absent |

## Conventions

- Every function is a module function on `NetCDF`, named as libnetcdf names it.  A status code becomes a `NetCDF::Error`, output parameters become return values, and a lookup that finds nothing returns `nil`.
- `ncid`, `dimid`, `varid`, `attnum`, `grpid` are the integers libnetcdf uses, passed to and from these calls unchanged.
- Functions that take an `Array` of indices (`start`, `count`, `stride`, etc.) expect a Ruby `Array` of `Integer` whose length equals the variable's rank.
- Smart-typing generics (`nc_put_att`, `nc_get_att`, `nc_put_var1`, `nc_get_var1`, `nc_put_var{,a,s,m}`, `nc_get_var{,a,s,m}`) dispatch on the Ruby value type or the variable's `nc_type`. There are no typed Ruby variants for variables — Ruby's type system already covers the relevant cases. Attributes do expose typed variants because the put-call itself determines the on-disk attribute type.
- Buffer arguments for array I/O are objects that support MemoryView. A raw Ruby `String` is *not* accepted there; see [Buffers and MemoryView](02_buffers.md).
- `get` calls read into the buffer passed and return it; the gem allocates none of its own, and omitting the buffer raises `ArgumentError`.  `NC_STRING` variables are the exception: they take no buffer and return a nested Ruby `Array<String>` (`char**` does not map to MemoryView).
- Array I/O releases the GVL around the underlying libnetcdf call.  Every call is serialized on one process-wide lock, because libnetcdf is not thread-safe; see [Threads and the GVL](02_buffers.md#threads-and-the-gvl).

## File

```
ncid = NetCDF.nc_create(path[, mode = NetCDF::NC_CLOBBER])
ncid = NetCDF.nc_open(path[, mode = NetCDF::NC_NOWRITE])
NetCDF.nc_close(ncid)
NetCDF.nc_abort(ncid)         # discard a freshly created file
NetCDF.nc_sync(ncid)
NetCDF.nc_redef(ncid)
NetCDF.nc_enddef(ncid)

ndims, nvars, natts, unlimdim = NetCDF.nc_inq(ncid)
NetCDF.nc_inq_ndims(ncid)      # => Integer
NetCDF.nc_inq_nvars(ncid)      # => Integer
NetCDF.nc_inq_natts(ncid)      # => Integer
NetCDF.nc_inq_unlimdim(ncid)   # => Integer | nil
NetCDF.nc_inq_unlimdims(ncid)  # => [Integer, ...] (every unlimited dim; [] if none)

oldmode = NetCDF.nc_set_fill(ncid, mode)
oldfmt  = NetCDF.nc_set_default_format(fmt)   # fmt: NC_FORMAT_*; for every later nc_create in the process

path  = NetCDF.nc_inq_path(ncid)            # libnetcdf >= 4.1.2, => String
fmt   = NetCDF.nc_inq_format(ncid)          # => NC_FORMAT_*
fmt, mode = NetCDF.nc_inq_format_extended(ncid)   # libnetcdf >= 4.2
vers  = NetCDF.nc_inq_libvers               # => String
name, size = NetCDF.nc_inq_type(ncid, xtype)
```

### In-memory files

Create and read NetCDF data without touching the filesystem. Requires libnetcdf built with the in-memory APIs (4.4+ for `nc_open_mem`, 4.6.2+ for `nc_create_mem` / `nc_close_memio`); each function is only defined when the underlying symbol was detected at build time.

```
ncid  = NetCDF.nc_open_mem(name, omode, bytes)
        # name  : identifying string (need not be a real path)
        # omode : NC_NOWRITE, NC_WRITE, etc.
        # bytes : Ruby String holding a full netCDF file image.
        #         libnetcdf reads the buffer in place, so the gem hands
        #         it a private copy (freed by nc_close / nc_abort /
        #         nc_close_memio); the String can be freed/modified
        #         after the call returns.

ncid  = NetCDF.nc_create_mem(name, cmode[, initialsize = 0])
        # cmode       : NC_NETCDF4 etc.  NC_DISKLESS/NC_INMEMORY are
        #               applied internally by libnetcdf.
        # initialsize : starting buffer size in bytes (0 = libnetcdf default).

bytes = NetCDF.nc_close_memio(ncid)
        # Closes a handle obtained from nc_create_mem and returns the
        # on-disk image as a binary String.  Do NOT also call nc_close
        # on that handle.  Handles obtained from nc_open_mem are closed
        # with nc_close as usual.
```

Open modes can be combined with diskless flags when the underlying libnetcdf supports them: `NC_DISKLESS`, `NC_PERSIST`, `NC_MMAP`, `NC_INMEMORY` (each defined only when present in the linked libnetcdf).

### Zarr stores

libnetcdf 4.8 and later can read and write Zarr.  A Zarr store is a directory, not one file.  Each chunk of a variable is a file of its own, and the metadata is small JSON files.  libnetcdf calls this support NCZarr.

The gem needs no extra functions for it.  Pass a URL in place of a path, and say the format in the `#mode=` part:

```
url  = "file:///data/t.zarr#mode=zarr,file"
ncid = NetCDF.nc_create(url, NetCDF::NC_NETCDF4)
ncid = NetCDF.nc_open(url)

fmt, mode = NetCDF.nc_inq_format_extended(ncid)   # fmt == NetCDF::NC_FORMATX_ZARR
```

- `mode=zarr` reads and writes plain Zarr, the form other tools such as xarray use.
- `mode=nczarr` adds netCDF's own metadata, such as shared dimensions, to the store.
- `file` stores the data in a local directory.  `s3` stores it in Amazon S3, when libnetcdf was built with S3 support.

The `#mode=` part is required.  A plain path that ends in `.zarr` creates an ordinary HDF5 file.

This works only when libnetcdf was built with NCZarr.  `nc-config --has-nczarr` says whether it was.  Groups, attributes, chunking, compression, `NC_STRING` and array I/O work as for a netCDF-4 file.  User-defined types do not: `nc_def_opaque`, for one, raises `NetCDF::Error`.

## Dimension

```
dimid = NetCDF.nc_def_dim(ncid, name, len)        # len may be NC_UNLIMITED
dimid = NetCDF.nc_inq_dimid(ncid, name)           # => Integer | nil
name, len = NetCDF.nc_inq_dim(ncid, dimid)
NetCDF.nc_inq_dimname(ncid, dimid)                # => String
NetCDF.nc_inq_dimlen(ncid, dimid)                 # => Integer
NetCDF.nc_rename_dim(ncid, dimid, newname)
```

## Variable

```
varid = NetCDF.nc_def_var(ncid, name, xtype, dimids)
varid = NetCDF.nc_inq_varid(ncid, name)           # => Integer | nil

name, xtype, ndims, dimids, natts = NetCDF.nc_inq_var(ncid, varid)
NetCDF.nc_inq_varname(ncid, varid)
NetCDF.nc_inq_vartype(ncid, varid)
NetCDF.nc_inq_varndims(ncid, varid)
NetCDF.nc_inq_vardimid(ncid, varid)
NetCDF.nc_inq_varnatts(ncid, varid)
NetCDF.nc_rename_var(ncid, varid, newname)
NetCDF.nc_copy_var(src_ncid, varid, dst_ncid)     # => nil
```

`nc_copy_var` copies the definition, the attributes and the data of one variable to another file. The destination must already have dimensions with the same names. It may be in define mode or in data mode. A variable of the same name already there raises `NetCDF::Error`. To find the new variable, use `nc_inq_varid` with its name.

### Variable scalar I/O

```
NetCDF.nc_put_var1(ncid, varid, index_array, value)
value = NetCDF.nc_get_var1(ncid, varid, index_array)
```

`value` is `String` (for `NC_CHAR`), `Integer`, or `Float` depending on the variable's `nc_type`.

### Variable array I/O

```
NetCDF.nc_put_var (ncid, varid, buffer)
NetCDF.nc_put_vara(ncid, varid, start, count, buffer)
NetCDF.nc_put_vars(ncid, varid, start, count, stride, buffer)
NetCDF.nc_put_varm(ncid, varid, start, count, stride, imap, buffer)

NetCDF.nc_get_var (ncid, varid[, buffer])
NetCDF.nc_get_vara(ncid, varid, start, count[, buffer])
NetCDF.nc_get_vars(ncid, varid, start, count, stride[, buffer])
NetCDF.nc_get_varm(ncid, varid, start, count, stride, imap[, buffer])
```

`buffer` is any object that supports MemoryView.  It must be sized to hold the requested elements, must be writable on `get` calls, and must be row-major C-contiguous; [Buffers and MemoryView](02_buffers.md) has the rules and the errors they raise.

On `get`, the read fills `buffer` and returns it.  The argument is written as optional only because `NC_STRING` variables take none and return a nested `Array<String>` instead (see below); for any other type, omitting it raises `ArgumentError: buffer is required`.

### NC_STRING variables

`NC_STRING` is the one type whose data does not fit into a single `MemoryView`-backed block (each element is a separately allocated C string).  `nc_get_var{,a,s,m}` therefore takes no buffer and returns a Ruby Array directly, and `nc_put_var{,a,s,m}` accepts a Ruby String (only when the requested element count is 1) or an Array of Strings.  If the Array is nested, it is flattened internally and the flattened length must match the requested element count.

| variable rank | `nc_get_*` returns      | `nc_put_*` accepts                |
|---------------|-------------------------|-----------------------------------|
| 0 (scalar)    | `String`                | `String`                          |
| 1             | flat `Array<String>`    | `Array<String>` of correct length |
| n ≥ 2         | nested `Array` (n deep) | any nested form with matching flat length |

After a `get`, libnetcdf-allocated strings are released automatically.

### NetCDF-4 per-variable options

```
NetCDF.nc_def_var_fill(ncid, varid, no_fill_bool, fill_value_or_nil)
no_fill, fill = NetCDF.nc_inq_var_fill(ncid, varid)

NetCDF.nc_def_var_deflate(ncid, varid, shuffle_bool, deflate_bool, level)
shuffle, deflate, level = NetCDF.nc_inq_var_deflate(ncid, varid)

NetCDF.nc_def_var_chunking(ncid, varid, storage, chunksizes_or_nil)
storage, chunksizes = NetCDF.nc_inq_var_chunking(ncid, varid)

NetCDF.nc_def_var_fletcher32(ncid, varid, on_bool)
NetCDF.nc_inq_var_fletcher32(ncid, varid)        # => true | false

NetCDF.nc_def_var_endian(ncid, varid, endian)    # NC_ENDIAN_*
NetCDF.nc_inq_var_endian(ncid, varid)            # => Integer

NetCDF.nc_def_var_filter(ncid, varid, filter_id, params_array)    # libnetcdf >= 4.7
NetCDF.nc_inq_var_filter_info(ncid, varid, filter_id)             # libnetcdf >= 4.7, => Array<Integer> | nil
NetCDF.nc_inq_var_filter_ids(ncid, varid)                         # libnetcdf >= 4.7, => Array<Integer>
NetCDF.nc_inq_filter_avail(ncid, filter_id)                       # libnetcdf >= 4.7, => true | false

NetCDF.nc_def_var_szip(ncid, varid, options_mask, pixels_per_block)   # options_mask: NC_SZIP_NN or NC_SZIP_EC
NetCDF.nc_inq_var_szip(ncid, varid)                               # => [options_mask, pixels_per_block]
NetCDF.nc_def_var_zstandard(ncid, varid, level)                   # libnetcdf >= 4.9
NetCDF.nc_inq_var_zstandard(ncid, varid)                          # libnetcdf >= 4.9, => [has_filter, level]
NetCDF.nc_def_var_bzip2(ncid, varid, level)                       # libnetcdf >= 4.9
NetCDF.nc_inq_var_bzip2(ncid, varid)                              # libnetcdf >= 4.9, => [has_filter, level]
NetCDF.nc_def_var_blosc(ncid, varid, subcompressor, level, blocksize, shuffle)   # libnetcdf >= 4.9
NetCDF.nc_inq_var_blosc(ncid, varid)                              # libnetcdf >= 4.9,
                                                                  #   => [has_filter, subcompressor, level, blocksize, shuffle]

NetCDF.nc_def_var_quantize(ncid, varid, mode, nsd)                # libnetcdf >= 4.9
NetCDF.nc_inq_var_quantize(ncid, varid)                           # libnetcdf >= 4.9, => [mode, nsd]

NetCDF.nc_set_var_chunk_cache(ncid, varid, size, nelems, preemption)
NetCDF.nc_get_var_chunk_cache(ncid, varid)       # => [size, nelems, preemption]
```

The calls for szip, zstandard, bzip2 and blosc are there even when libnetcdf was built without the filter. To check whether a filter can be used, call `nc_inq_filter_avail` with its `H5Z_FILTER_*` id. When the filter is missing:

- The four def calls raise `NetCDF::Error` with status `NC_ENOFILTER` (-136). The gem checks this itself, because a libnetcdf without blosc returns no error from `nc_def_var_blosc` and writes the data uncompressed. For szip the check needs libnetcdf 4.7 or later.
- The `nc_inq_var_*` calls report that the variable does not have the filter.

A filter needs chunked storage (`nc_def_var_chunking` with `NC_CHUNKED`). A classic-format file has no filters.

Global chunk cache (process-wide HDF5 cache parameters):

```
NetCDF.nc_set_chunk_cache(size, nelems, preemption)
NetCDF.nc_get_chunk_cache  # => [size, nelems, preemption]
```

## Attribute

`varid` may be `NetCDF::NC_GLOBAL` for file- or group-level attributes.

### Inquiry

```
NetCDF.nc_inq_attname(ncid, varid, attnum)        # => String
NetCDF.nc_inq_attid(ncid, varid, name)            # => Integer | nil
xtype, len = NetCDF.nc_inq_att(ncid, varid, name)
NetCDF.nc_inq_atttype(ncid, varid, name)
NetCDF.nc_inq_attlen(ncid, varid, name)
NetCDF.nc_rename_att(ncid, varid, name, newname)
NetCDF.nc_del_att(ncid, varid, name)
NetCDF.nc_copy_att(src_ncid, src_varid, name, dst_ncid, dst_varid)
```

### Smart generic put/get

The Ruby value's class determines the on-disk type:

| Ruby                          | NetCDF type      |
|-------------------------------|------------------|
| `String`                      | `NC_CHAR`        |
| `Integer` fitting int32       | `NC_INT`         |
| larger `Integer` fitting int64 | `NC_INT64`     |
| `Integer` past 2**63 - 1, up to 2**64 - 1 | `NC_UINT64` |
| `Float`                       | `NC_DOUBLE`      |
| `Array` of integers           | the narrowest of the above that holds every element; an `Array` mixing a value past 2**63 - 1 with a negative one raises `RangeError` |
| `Array` with at least one float | `NC_DOUBLE`    |

```
NetCDF.nc_put_att(ncid, varid, name, value)
value = NetCDF.nc_get_att(ncid, varid, name)
```

`nc_get_att` returns a single `String` / `Integer` / `Float` when the attribute length is 1, and an `Array` otherwise.

netCDF has no scalar attribute: `5` is stored as a length-1 attribute, the same as `[5]`, so both read back as `5`, and the shape you wrote cannot be recovered. Code that needs an `Array` whatever the length uses the typed gets (`nc_get_att_int` and friends), which always return one.

### Typed put/get

Use these to commit to a specific on-disk attribute type.  As in C, the
numeric puts take `xtype` — the type stored on disk — while the suffix names
the type of the values in memory, and libnetcdf converts between the two:
`nc_put_att_double(ncid, varid, "_FillValue", NC_FLOAT, -999.0)` stores an
`NC_FLOAT`, as a float variable's `_FillValue` has to be.  A value the
suffix's C type cannot hold raises `RangeError`.

```
NetCDF.nc_put_att_text     (ncid, varid, name, string)
NetCDF.nc_put_att_schar    (ncid, varid, name, xtype, value_or_array)
NetCDF.nc_put_att_uchar    (ncid, varid, name, xtype, value_or_array)
NetCDF.nc_put_att_short    (ncid, varid, name, xtype, value_or_array)
NetCDF.nc_put_att_ushort   (ncid, varid, name, xtype, value_or_array)
NetCDF.nc_put_att_int      (ncid, varid, name, xtype, value_or_array)
NetCDF.nc_put_att_uint     (ncid, varid, name, xtype, value_or_array)
NetCDF.nc_put_att_longlong (ncid, varid, name, xtype, value_or_array)
NetCDF.nc_put_att_ulonglong(ncid, varid, name, xtype, value_or_array)
NetCDF.nc_put_att_float    (ncid, varid, name, xtype, value_or_array)
NetCDF.nc_put_att_double   (ncid, varid, name, xtype, value_or_array)
NetCDF.nc_put_att_string   (ncid, varid, name, string_or_array_of_strings)

NetCDF.nc_get_att_text(ncid, varid, name)      # => String
NetCDF.nc_get_att_<numeric>(ncid, varid, name) # => Array<Numeric>
NetCDF.nc_get_att_string(ncid, varid, name)    # => Array<String>
```

The smart `nc_put_att` always maps a plain Ruby `String` to `NC_CHAR`, matching the convention used by most NetCDF tooling. Use the typed `nc_put_att_string` to commit to `NC_STRING` instead. The smart `nc_get_att` returns the stored value as a single `String` when an `NC_STRING` attribute has length 1.

Strings read back — `NC_CHAR` and `NC_STRING` attributes, and `NC_CHAR` and `NC_STRING` values from `nc_get_var1` and the `NC_STRING` array gets — are tagged UTF-8, the encoding netCDF defines for `NC_STRING`. So are the names of groups, dimensions, variables, attributes and types. Strings are written as their bytes, without transcoding.

## User-defined types (NetCDF-4 only)

### Generic inquiry

```
name, size, base_type, nfields, klass = NetCDF.nc_inq_user_type(ncid, typeid)
typeid = NetCDF.nc_inq_typeid(ncid, name)   # => Integer | nil
NetCDF.nc_inq_typeids(ncid)                  # => Array<Integer>
NetCDF.nc_inq_type_equal(ncid1, typeid1, ncid2, typeid2)   # => true | false
```

`klass` is one of `NC_VLEN`, `NC_OPAQUE`, `NC_ENUM`, `NC_COMPOUND`.

`nc_inq_type_equal` compares a user-defined type only after `nc_enddef` on its file. In define mode it raises `NetCDF::Error` ("HDF error"), even when both arguments name the same type.

### Opaque (fixed-size raw bytes)

```
typeid = NetCDF.nc_def_opaque(ncid, size, name)
name, size = NetCDF.nc_inq_opaque(ncid, typeid)
```

Single-element I/O (`nc_put_var1` / `nc_get_var1`) uses a Ruby String of exactly `opaque_size` bytes.

Array I/O (`nc_put_var{,a,s,m}` / `nc_get_var{,a,s,m}`) uses a MemoryView buffer holding `opaque_size × element_count` bytes, passed through untouched — libnetcdf already knows the element size, so the buffer's own `format` and `item_size` are ignored; any buffer of the right byte size will do:

```ruby
t = NetCDF.nc_def_opaque(ncid, 4, "tag_t")
v = NetCDF.nc_def_var(ncid, "tags", t, [dimid])
NetCDF.nc_enddef(ncid)

src = CArray.uint8(12).load_binary("ABCDEFGHIJKL")   # 12 bytes
NetCDF.nc_put_vara(ncid, v, [0], [3], src)

out = CArray.uint8(12)
NetCDF.nc_get_vara(ncid, v, [0], [3], out)
out.to_s                                              # => "ABCDEFGHIJKL"
```

Raw Ruby Strings are not accepted by array I/O, the same as for any other type.

### Vlen (variable-length sequence)

```
typeid = NetCDF.nc_def_vlen(ncid, name, base_type)
name, datum_size, base_type = NetCDF.nc_inq_vlen(ncid, typeid)
```

Type definition only — data I/O for vlen variables is not yet exposed.

### Enum (named integer constants)

```
typeid = NetCDF.nc_def_enum(ncid, base_type, name)   # base_type: any integer nc_type
NetCDF.nc_insert_enum(ncid, typeid, "RED",   0)
NetCDF.nc_insert_enum(ncid, typeid, "GREEN", 1)

name, base, base_size, nmembers = NetCDF.nc_inq_enum(ncid, typeid)
name, value = NetCDF.nc_inq_enum_member(ncid, typeid, idx)
NetCDF.nc_inq_enum_ident(ncid, typeid, value)   # => String | nil
```

Variables of enum type are read and written as plain Integers — internally the gem uses libnetcdf's untyped scalar/array I/O to satisfy the enum type check.  That I/O moves elements of the base type with no conversion, so an array buffer for an enum variable holds the base type: a formatted buffer must name it (`"i"` for an `NC_INT` base), and one naming another type raises `ArgumentError`.  A formatless buffer is read as the base type.

Attributes of enum type are read the same way. libnetcdf's typed getters refuse one, but here `nc_get_att` returns its values as Integers of the base type, and the typed getter for the base type (`nc_get_att_uchar` for an `NC_UBYTE` base) reads it too; any other typed getter raises `NetCDF::Error`. The usual case is a `_FillValue` on an enum variable, which netCDF4-python writes with the enum's type. `nc_def_var_fill` and `nc_inq_var_fill` on an enum variable take and return the fill value as an Integer of the base type, and the default fill is the base type's. There is no call yet to write an enum attribute other than through `nc_def_var_fill`.

## Group (NetCDF-4 only)

```
grpid = NetCDF.nc_def_grp(parent_ncid, name)
NetCDF.nc_rename_grp(grpid, name)

NetCDF.nc_inq_grps(ncid)                  # => Array<Integer>
NetCDF.nc_inq_grpname(ncid)               # => String (leaf name)
NetCDF.nc_inq_grpname_full(ncid)          # => String ("/foo/bar")
NetCDF.nc_inq_grp_parent(ncid)            # => Integer | nil (root)
NetCDF.nc_inq_ncid(parent_ncid, name)     # => Integer | nil
NetCDF.nc_inq_grp_full_ncid(ncid, path)   # => Integer | nil
NetCDF.nc_inq_dimids(ncid[, include_parents])
NetCDF.nc_inq_varids(ncid)
```

A group's `ncid` is interchangeable with the file's `ncid` for almost all calls — dimensions and variables defined under a group are local to it, but attributes can be reached with `NC_GLOBAL`.

## Constants

Open / create modes: `NC_NOWRITE`, `NC_WRITE`, `NC_CLOBBER`, `NC_NOCLOBBER`, `NC_SHARE`, `NC_NETCDF4`, `NC_CLASSIC_MODEL`, `NC_64BIT_OFFSET`, `NC_64BIT_DATA` (alias `NC_CDF5`; CDF-5, defined only when the linked libnetcdf provides it).

Diskless / in-memory modes (defined only when the linked libnetcdf provides them): `NC_DISKLESS`, `NC_PERSIST`, `NC_MMAP`, `NC_INMEMORY`.

Fill: `NC_FILL`, `NC_NOFILL`.

Globals: `NC_GLOBAL`, `NC_UNLIMITED`.

Types: `NC_BYTE`, `NC_CHAR`, `NC_SHORT`, `NC_INT`, `NC_FLOAT`, `NC_DOUBLE`, `NC_UBYTE`, `NC_USHORT`, `NC_UINT`, `NC_INT64`, `NC_UINT64`, `NC_STRING`.

Format codes (returned by `nc_inq_format`): `NC_FORMAT_CLASSIC`, `NC_FORMAT_64BIT_OFFSET`, `NC_FORMAT_64BIT_DATA`, `NC_FORMAT_NETCDF4`, `NC_FORMAT_NETCDF4_CLASSIC`.

Extended format codes (returned by `nc_inq_format_extended`): `NC_FORMATX_NC3`, `NC_FORMATX_NC_HDF5`, `NC_FORMATX_NC_HDF4`, `NC_FORMATX_PNETCDF`, `NC_FORMATX_DAP2`, `NC_FORMATX_DAP4`, `NC_FORMATX_UDF0`, `NC_FORMATX_UDF1`, `NC_FORMATX_ZARR`, `NC_FORMATX_UNDEFINED`.

Storage / chunking: `NC_CONTIGUOUS`, `NC_CHUNKED`, `NC_COMPACT`.

Fletcher32: `NC_NOCHECKSUM`, `NC_FLETCHER32`.

Endian: `NC_ENDIAN_NATIVE`, `NC_ENDIAN_LITTLE`, `NC_ENDIAN_BIG`.

Quantize modes: `NC_NOQUANTIZE`, `NC_QUANTIZE_BITGROOM`, `NC_QUANTIZE_GRANULARBR`, `NC_QUANTIZE_BITROUND`.

Filter ids (for `nc_inq_filter_avail` and `nc_inq_var_filter_ids`): `H5Z_FILTER_DEFLATE`, `H5Z_FILTER_SHUFFLE`, `H5Z_FILTER_FLETCHER32`, `H5Z_FILTER_SZIP`, `H5Z_FILTER_ZSTD`, `H5Z_FILTER_BZIP2`, `H5Z_FILTER_BLOSC`.

Szip options: `NC_SZIP_NN`, `NC_SZIP_EC`.

Blosc subcompressors: `BLOSC_LZ`, `BLOSC_LZ4`, `BLOSC_LZ4HC`, `BLOSC_SNAPPY`, `BLOSC_ZLIB`, `BLOSC_ZSTD`. Blosc shuffle: `BLOSC_NOSHUFFLE`, `BLOSC_SHUFFLE`, `BLOSC_BITSHUFFLE`. (libnetcdf >= 4.9)

Limits: `NC_MAX_NAME`, `NC_MAX_VAR_DIMS`.

## Buffers

Array I/O reads into and writes from MemoryView buffers the caller provides.  What a buffer has to satisfy, how format strings are read, and which array libraries plug in are covered in [Buffers and MemoryView](02_buffers.md).

## Error handling

Every libnetcdf failure becomes a `NetCDF::Error`. The numeric status is available via `#status`.

```ruby
begin
  NetCDF.nc_open("missing.nc")
rescue NetCDF::Error => e
  puts e.message    # libnetcdf's nc_strerror text
  puts e.status     # the original NC_E* code
end
```

## Not yet implemented

A short list of `libnetcdf` features that this gem does not yet expose.

- Compound types (`nc_def_compound`, `nc_insert_compound`, `nc_inq_compound*`).
- Vlen variable I/O (`nc_def_vlen` / `nc_inq_vlen` work, but reading and writing vlen variables is not yet wired through the I/O dispatcher).
- Parallel I/O (`nc_create_par`, `nc_open_par`, `nc_var_par_access`).
- DAP / pNetCDF backend tuning, log control (`nc_set_log_level`), and other rarely used knobs.
