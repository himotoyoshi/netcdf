/*
 * netcdf.c — entry point.  Defines the NetCDF module, the error class,
 * shared helpers used by the submodules, and the constants table.
 */
#include "internal.h"

VALUE mNetCDF;
VALUE eError;

void
nc_raise (int status)
{
  VALUE msg, err;

  msg = rb_str_new_cstr(nc_strerror(status));
  err = rb_funcall(eError, rb_intern("new"), 1, msg);
  rb_funcall(err, rb_intern("status="), 1, INT2NUM(status));
  rb_exc_raise(err);
}

/* ---------------------------------------------------------- */
/* Common helpers used by io.c, var.c, etc.                    */
/* ---------------------------------------------------------- */

/* The array helpers below fill an array the caller provides -- one of
   NC_MAX_VAR_DIMS entries on its stack holds any variable's -- rather
   than allocating one, so a raise partway through (a length mismatch, an
   entry that is not an Integer) leaves nothing to free. */
void
ary_to_sizet (VALUE ary, int ndims, const char *role, size_t *out)
{
  long n, i;

  Check_Type(ary, T_ARRAY);
  n = RARRAY_LEN(ary);
  if ( n != ndims ) {
    rb_raise(rb_eArgError, "%s length mismatch (%ld for %d)", role, n, ndims);
  }
  for (i=0; i<n; i++) {
    out[i] = NUM2SIZET(RARRAY_AREF(ary, i));
  }
}

/* Returns false, leaving `out` untouched, when `ary` is nil. */
bool
ary_to_ptrdiff (VALUE ary, int ndims, const char *role, ptrdiff_t *out)
{
  long n, i;

  if ( NIL_P(ary) ) {
    return false;
  }
  Check_Type(ary, T_ARRAY);
  n = RARRAY_LEN(ary);
  if ( n != ndims ) {
    rb_raise(rb_eArgError, "%s length mismatch (%ld for %d)", role, n, ndims);
  }
  for (i=0; i<n; i++) {
    out[i] = (ptrdiff_t) NUM2LL(RARRAY_AREF(ary, i));
  }
  return true;
}

ssize_t
nc_xtype_size (nc_type xt)
{
  switch ( xt ) {
  case NC_BYTE:  case NC_UBYTE:  case NC_CHAR:   return 1;
  case NC_SHORT: case NC_USHORT:                 return 2;
  case NC_INT:   case NC_UINT:   case NC_FLOAT:  return 4;
  case NC_INT64: case NC_UINT64: case NC_DOUBLE: return 8;
  default:                                       return -1;
  }
}

/* The base type of the enum type `xt`, or `xt` itself for any other type.
   An enum value is stored as its base type, so code that reads or writes
   one element can treat it as that integer type. */
nc_type
resolve_enum_base (int ncid, nc_type xt)
{
  nc_type base;
  int klass;

  if ( xt < NC_FIRSTUSERTYPEID ) {
    return xt;
  }
  NC_CHECK(nc_inq_user_type(ncid, xt, NULL, NULL, &base, NULL, &klass));
  if ( klass != NC_ENUM ) {
    return xt;
  }
  return base;
}

/* Integer conversions that refuse a value the C type cannot hold.  A cast
   would store 300 as NC_BYTE 44, and Ruby's own NUM2UINT / NUM2ULL wrap a
   negative value around rather than raise, so -1 would be stored as the
   type's maximum. */
static long long
num_in_range (VALUE v, long long lo, long long hi, const char *type)
{
  long long x;

  x = NUM2LL(v);
  if ( x < lo || x > hi ) {
    rb_raise(rb_eRangeError, "integer %lld out of range for %s", x, type);
  }
  return x;
}

signed char
num_to_schar (VALUE v)
{
  return (signed char) num_in_range(v, SCHAR_MIN, SCHAR_MAX, "NC_BYTE");
}

unsigned char
num_to_uchar (VALUE v)
{
  return (unsigned char) num_in_range(v, 0, UCHAR_MAX, "NC_UBYTE");
}

short
num_to_short (VALUE v)
{
  return (short) num_in_range(v, SHRT_MIN, SHRT_MAX, "NC_SHORT");
}

unsigned short
num_to_ushort (VALUE v)
{
  return (unsigned short) num_in_range(v, 0, USHRT_MAX, "NC_USHORT");
}

unsigned int
num_to_uint (VALUE v)
{
  return (unsigned int) num_in_range(v, 0, UINT_MAX, "NC_UINT");
}

unsigned long long
num_to_ulonglong (VALUE v)
{
  VALUE i;

  i = rb_to_int(v);
  if ( FIXNUM_P(i) ? FIX2LONG(i) < 0 : RBIGNUM_NEGATIVE_P(i) ) {
    rb_raise(rb_eRangeError, "integer %"PRIsVALUE" out of range for NC_UINT64", i);
  }
  return NUM2ULL(i);
}

void
indices_to_c (int ncid, int varid, VALUE index_ary, size_t *out)
{
  int ndims;

  /* libnetcdf reads one coordinate per dimension of the variable, so a
     shorter index would have it read past the array. */
  NC_CHECK(nc_inq_varndims(ncid, varid, &ndims));
  ary_to_sizet(index_ary, ndims, "index", out);
}

/* ---------------------------------------------------------- */
/* Init                                                        */
/* ---------------------------------------------------------- */

#define DEF_CONST(name) rb_define_const(mNetCDF, #name, INT2NUM(name))

void
Init_netcdf (void)
{
  mNetCDF = rb_define_module("NetCDF");

  eError = rb_define_class_under(mNetCDF, "Error", rb_eStandardError);
  rb_define_attr(eError, "status", 1, 1);

  init_nc_file();
  init_nc_dim();
  init_nc_var();
  init_nc_att();
  init_nc_io();
  init_nc_group();
  init_nc_types();

  /* Open / create modes */
  DEF_CONST(NC_NOWRITE);
  DEF_CONST(NC_WRITE);
  DEF_CONST(NC_CLOBBER);
  DEF_CONST(NC_NOCLOBBER);
  DEF_CONST(NC_SHARE);
  DEF_CONST(NC_NETCDF4);
  DEF_CONST(NC_CLASSIC_MODEL);
  DEF_CONST(NC_64BIT_OFFSET);
#ifdef NC_64BIT_DATA
  DEF_CONST(NC_64BIT_DATA);            /* CDF-5; libnetcdf 4.4+ */
#endif
#ifdef NC_CDF5
  DEF_CONST(NC_CDF5);                  /* alias of NC_64BIT_DATA */
#endif
#ifdef NC_DISKLESS
  DEF_CONST(NC_DISKLESS);
#endif
#ifdef NC_PERSIST
  DEF_CONST(NC_PERSIST);
#endif
#ifdef NC_MMAP
  DEF_CONST(NC_MMAP);
#endif
#ifdef NC_INMEMORY
  DEF_CONST(NC_INMEMORY);
#endif

  /* Fill */
  DEF_CONST(NC_FILL);
  DEF_CONST(NC_NOFILL);

  /* Global / unlimited */
  DEF_CONST(NC_GLOBAL);
  DEF_CONST(NC_UNLIMITED);

  /* Types */
  DEF_CONST(NC_BYTE);
  DEF_CONST(NC_CHAR);
  DEF_CONST(NC_SHORT);
  DEF_CONST(NC_INT);
  DEF_CONST(NC_FLOAT);
  DEF_CONST(NC_DOUBLE);
  DEF_CONST(NC_UBYTE);
  DEF_CONST(NC_USHORT);
  DEF_CONST(NC_UINT);
  DEF_CONST(NC_INT64);
  DEF_CONST(NC_UINT64);
  DEF_CONST(NC_STRING);

  DEF_CONST(NC_MAX_NAME);
  DEF_CONST(NC_MAX_VAR_DIMS);

  /* File-format codes */
  DEF_CONST(NC_FORMAT_CLASSIC);
  DEF_CONST(NC_FORMAT_64BIT_OFFSET);
#ifdef NC_FORMAT_64BIT_DATA
  DEF_CONST(NC_FORMAT_64BIT_DATA);     /* CDF-5; libnetcdf 4.4+ */
#endif
  DEF_CONST(NC_FORMAT_NETCDF4);
  DEF_CONST(NC_FORMAT_NETCDF4_CLASSIC);

#ifdef NC_FORMATX_NC3
  DEF_CONST(NC_FORMATX_NC3);
#endif
#ifdef NC_FORMATX_NC_HDF5
  DEF_CONST(NC_FORMATX_NC_HDF5);
#endif
#ifdef NC_FORMATX_NC_HDF4
  DEF_CONST(NC_FORMATX_NC_HDF4);
#endif
#ifdef NC_FORMATX_PNETCDF
  DEF_CONST(NC_FORMATX_PNETCDF);
#endif
#ifdef NC_FORMATX_DAP2
  DEF_CONST(NC_FORMATX_DAP2);
#endif
#ifdef NC_FORMATX_DAP4
  DEF_CONST(NC_FORMATX_DAP4);
#endif
#ifdef NC_FORMATX_UDF0
  DEF_CONST(NC_FORMATX_UDF0);
#endif
#ifdef NC_FORMATX_UDF1
  DEF_CONST(NC_FORMATX_UDF1);
#endif
#ifdef NC_FORMATX_ZARR
  DEF_CONST(NC_FORMATX_ZARR);
#endif
#ifdef NC_FORMATX_UNDEFINED
  DEF_CONST(NC_FORMATX_UNDEFINED);
#endif

  /* Chunking / storage */
  DEF_CONST(NC_CONTIGUOUS);
  DEF_CONST(NC_CHUNKED);
  DEF_CONST(NC_COMPACT);

  /* Fletcher32 checksum modes */
  DEF_CONST(NC_NOCHECKSUM);
  DEF_CONST(NC_FLETCHER32);

  /* Endian */
  DEF_CONST(NC_ENDIAN_NATIVE);
  DEF_CONST(NC_ENDIAN_LITTLE);
  DEF_CONST(NC_ENDIAN_BIG);

  /* Quantize modes (libnetcdf >= 4.9) */
#ifdef NC_NOQUANTIZE
  DEF_CONST(NC_NOQUANTIZE);
#endif
#ifdef NC_QUANTIZE_BITGROOM
  DEF_CONST(NC_QUANTIZE_BITGROOM);
#endif
#ifdef NC_QUANTIZE_GRANULARBR
  DEF_CONST(NC_QUANTIZE_GRANULARBR);
#endif
#ifdef NC_QUANTIZE_BITROUND
  DEF_CONST(NC_QUANTIZE_BITROUND);
#endif

  /* Filter ids, for nc_inq_filter_avail and nc_inq_var_filter_ids */
#ifdef H5Z_FILTER_DEFLATE
  DEF_CONST(H5Z_FILTER_DEFLATE);
#endif
#ifdef H5Z_FILTER_SHUFFLE
  DEF_CONST(H5Z_FILTER_SHUFFLE);
#endif
#ifdef H5Z_FILTER_FLETCHER32
  DEF_CONST(H5Z_FILTER_FLETCHER32);
#endif
#ifdef H5Z_FILTER_SZIP
  DEF_CONST(H5Z_FILTER_SZIP);
#endif
#ifdef H5Z_FILTER_ZSTD
  DEF_CONST(H5Z_FILTER_ZSTD);
#endif
#ifdef H5Z_FILTER_BZIP2
  DEF_CONST(H5Z_FILTER_BZIP2);
#endif
#ifdef H5Z_FILTER_BLOSC
  DEF_CONST(H5Z_FILTER_BLOSC);
#endif

  /* szip options_mask */
#ifdef NC_SZIP_EC
  DEF_CONST(NC_SZIP_EC);
#endif
#ifdef NC_SZIP_NN
  DEF_CONST(NC_SZIP_NN);
#endif

  /* Blosc subcompressors and shuffle modes.  These are enum members,
     not macros, so they are gated on the call that takes them. */
#ifdef HAVE_NC_DEF_VAR_BLOSC
  DEF_CONST(BLOSC_LZ);
  DEF_CONST(BLOSC_LZ4);
  DEF_CONST(BLOSC_LZ4HC);
  DEF_CONST(BLOSC_SNAPPY);
  DEF_CONST(BLOSC_ZLIB);
  DEF_CONST(BLOSC_ZSTD);
  DEF_CONST(BLOSC_NOSHUFFLE);
  DEF_CONST(BLOSC_SHUFFLE);
  DEF_CONST(BLOSC_BITSHUFFLE);
#endif

  /* User-defined type classes (returned by nc_inq_user_type) */
  DEF_CONST(NC_VLEN);
  DEF_CONST(NC_OPAQUE);
  DEF_CONST(NC_ENUM);
  DEF_CONST(NC_COMPOUND);

  DEF_CONST(NC_FIRSTUSERTYPEID);
}
