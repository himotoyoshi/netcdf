# Changelog

Releases are recorded here from 0.1.0, which is the first. There is no
separate NEWS file: this is where to look for what changed between the
version you have and a newer one.

<!-- Newest first, at both levels: a new release section goes above the
     ones below it, and a new entry goes directly under its own release
     heading -- not at the end of the section. The kind of change is
     carried by the `- Fix:` / `- Change:` / `- New:` that opens the
     entry; there are no per-kind subheadings.

     A section is written newest-first while the release is open, and
     sorted into New, Change, Fix when it closes -- in the same commit
     that drops `(unreleased)`. Within Change, the ones that ask the
     reader to change code come first.

     An entry says three things and stops: what changed, what to do about
     it (the migration, the replacement, the condition under which nothing
     changes), and what is excluded. It does not say how the code was
     broken or name the internals that were fixed -- those belong in the
     commit message. Two to six lines.

     It is written for someone using the library, not someone working on
     it: with no NEWS file, this is what a reader consults before
     upgrading.

     The version here is this gem's own and is not libnetcdf's. Which
     libnetcdf a release needs is said in extconf.rb, and an entry says
     so only when the answer changes. -->

## 0.2.0 (unreleased)

- New: calls for the compression filters szip, zstandard, bzip2 and
  blosc: `nc_def_var_szip` / `nc_inq_var_szip`, and the same pair for
  `zstandard`, `bzip2` and `blosc` (libnetcdf 4.9 or later). New constants
  give the filter ids (`H5Z_FILTER_*`) and the szip and blosc options.
  When libnetcdf has no such filter, the def calls raise
  `NetCDF::Error` (`NC_ENOFILTER`) instead of writing uncompressed data.
  For szip this check needs libnetcdf 4.7 or later.

- New: `NetCDF.nc_copy_var(src_ncid, varid, dst_ncid)` copies one variable
  -- its definition, attributes and data -- to another file. The
  destination needs dimensions with the same names.

- New: `NetCDF.nc_set_default_format(format)` sets the format that
  `nc_create` uses when the mode names none, and returns the old one.
  The setting is for the whole process, not for one file.

- New: `NetCDF.nc_inq_type_equal(ncid1, typeid1, ncid2, typeid2)` returns
  `true` if the two types are the same, also when they are in different
  files or groups. A user-defined type can be compared only after
  `nc_enddef`; in define mode the call raises `NetCDF::Error`.

- New: `NetCDF.nc_inq_unlimdims(ncid)` returns the ids of all unlimited
  dimensions as an Array, or `[]` if there are none. A NetCDF-4 file can
  have more than one unlimited dimension; `nc_inq_unlimdim` still returns
  only one of them.

## 0.1.0

First release.
