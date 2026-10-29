#include "internal.h"

/* ---------------------------------------------------------- */
/* Definition / inquiry                                        */
/* ---------------------------------------------------------- */

static VALUE
rb_nc_def_var (VALUE self, VALUE rncid, VALUE rname, VALUE rxtype, VALUE rdimids)
{
  VALUE vids;
  int *ids;
  long ndims, i;
  int varid, st;

  Check_Type(rdimids, T_ARRAY);
  ndims = RARRAY_LEN(rdimids);
  vids  = 0;
  ids   = NULL;
  if ( ndims > 0 ) {
    ids = ALLOCV_N(int, vids, ndims);
    for (i=0; i<ndims; i++) {
      ids[i] = NUM2INT(RARRAY_AREF(rdimids, i));
    }
  }
  st = nc_def_var(NUM2INT(rncid), StringValueCStr(rname), NUM2INT(rxtype),
                  (int) ndims, ids, &varid);
  if ( ids ) {
    ALLOCV_END(vids);
  }
  if ( st != NC_NOERR ) {
    nc_raise(st);
  }
  return INT2NUM(varid);
}

static VALUE
rb_nc_inq_varid (VALUE self, VALUE rncid, VALUE rname)
{
  int varid, st;

  st = nc_inq_varid(NUM2INT(rncid), StringValueCStr(rname), &varid);
  if ( st == NC_ENOTVAR ) {
    return Qnil;
  }
  if ( st != NC_NOERR ) {
    nc_raise(st);
  }
  return INT2NUM(varid);
}

static VALUE
rb_nc_inq_var (VALUE self, VALUE rncid, VALUE rvarid)
{
  char name[NC_MAX_NAME + 1];
  int dimids[NC_MAX_VAR_DIMS];
  VALUE rdimids;
  nc_type xtype;
  int ndims, natts, i;

  NC_CHECK(nc_inq_var(NUM2INT(rncid), NUM2INT(rvarid), name, &xtype,
                      &ndims, dimids, &natts));
  rdimids = rb_ary_new_capa(ndims);
  for (i=0; i<ndims; i++) {
    rb_ary_push(rdimids, INT2NUM(dimids[i]));
  }
  return rb_ary_new_from_args(5,
                              rb_utf8_str_new_cstr(name), INT2NUM(xtype),
                              INT2NUM(ndims), rdimids, INT2NUM(natts));
}

static VALUE
rb_nc_inq_varname (VALUE self, VALUE rncid, VALUE rvarid)
{
  char name[NC_MAX_NAME + 1];

  NC_CHECK(nc_inq_varname(NUM2INT(rncid), NUM2INT(rvarid), name));
  return rb_utf8_str_new_cstr(name);
}

static VALUE
rb_nc_inq_vartype (VALUE self, VALUE rncid, VALUE rvarid)
{
  nc_type t;

  NC_CHECK(nc_inq_vartype(NUM2INT(rncid), NUM2INT(rvarid), &t));
  return INT2NUM(t);
}

static VALUE
rb_nc_inq_varndims (VALUE self, VALUE rncid, VALUE rvarid)
{
  int n;

  NC_CHECK(nc_inq_varndims(NUM2INT(rncid), NUM2INT(rvarid), &n));
  return INT2NUM(n);
}

static VALUE
rb_nc_inq_vardimid (VALUE self, VALUE rncid, VALUE rvarid)
{
  int dimids[NC_MAX_VAR_DIMS];
  VALUE out;
  int ndims, i;

  NC_CHECK(nc_inq_varndims(NUM2INT(rncid), NUM2INT(rvarid), &ndims));
  NC_CHECK(nc_inq_vardimid(NUM2INT(rncid), NUM2INT(rvarid), dimids));
  out = rb_ary_new_capa(ndims);
  for (i=0; i<ndims; i++) {
    rb_ary_push(out, INT2NUM(dimids[i]));
  }
  return out;
}

static VALUE
rb_nc_inq_varnatts (VALUE self, VALUE rncid, VALUE rvarid)
{
  int n;

  NC_CHECK(nc_inq_varnatts(NUM2INT(rncid), NUM2INT(rvarid), &n));
  return INT2NUM(n);
}

static VALUE
rb_nc_rename_var (VALUE self, VALUE rncid, VALUE rvarid, VALUE rname)
{
  NC_CHECK(nc_rename_var(NUM2INT(rncid), NUM2INT(rvarid), StringValueCStr(rname)));
  return Qnil;
}

/* ---------------------------------------------------------- */
/* NetCDF-4 per-variable options                               */
/* ---------------------------------------------------------- */

static VALUE
rb_nc_def_var_fill (VALUE self, VALUE rncid, VALUE rvarid, VALUE rno_fill, VALUE rfill)
{
  union {
    signed char        c;  unsigned char      uc;
    short              s;  unsigned short     us;
    int                i;  unsigned int       ui;
    long long          ll; unsigned long long ull;
    float              f;  double             d;
  } v;
  void *vp;
  int ncid, varid, no_fill;
  nc_type xt;

  ncid    = NUM2INT(rncid);
  varid   = NUM2INT(rvarid);
  no_fill = RTEST(rno_fill) ? 1 : 0;

  NC_CHECK(nc_inq_vartype(ncid, varid, &xt));

  if ( NIL_P(rfill) ) {
    NC_CHECK(nc_def_var_fill(ncid, varid, no_fill, NULL));
    return Qnil;
  }

  vp = NULL;
  switch ( xt ) {
  case NC_CHAR:
    StringValue(rfill);
    if ( RSTRING_LEN(rfill) < 1 ) {
      rb_raise(rb_eArgError, "fill_value string must be non-empty");
    }
    vp = RSTRING_PTR(rfill);
    break;
  case NC_BYTE:   v.c   = num_to_schar(rfill);      vp = &v.c;   break;
  case NC_UBYTE:  v.uc  = num_to_uchar(rfill);      vp = &v.uc;  break;
  case NC_SHORT:  v.s   = num_to_short(rfill);      vp = &v.s;   break;
  case NC_USHORT: v.us  = num_to_ushort(rfill);     vp = &v.us;  break;
  case NC_INT:    v.i   = NUM2INT(rfill);           vp = &v.i;   break;
  case NC_UINT:   v.ui  = num_to_uint(rfill);       vp = &v.ui;  break;
  case NC_INT64:  v.ll  = NUM2LL(rfill);            vp = &v.ll;  break;
  case NC_UINT64: v.ull = num_to_ulonglong(rfill);  vp = &v.ull; break;
  case NC_FLOAT:  v.f   = (float) NUM2DBL(rfill);   vp = &v.f;   break;
  case NC_DOUBLE: v.d   = NUM2DBL(rfill);           vp = &v.d;   break;
  default:
    rb_raise(rb_eNotImpError, "unsupported nc_type: %d", xt);
  }
  NC_CHECK(nc_def_var_fill(ncid, varid, no_fill, vp));
  return Qnil;
}

static VALUE
rb_nc_inq_var_fill (VALUE self, VALUE rncid, VALUE rvarid)
{
  union {
    signed char        c;  unsigned char      uc;
    short              s;  unsigned short     us;
    int                i;  unsigned int       ui;
    long long          ll; unsigned long long ull;
    float              f;  double             d;
    char               text;
  } v;
  VALUE rfill;
  int ncid, varid, no_fill;
  nc_type xt;

  ncid  = NUM2INT(rncid);
  varid = NUM2INT(rvarid);

  NC_CHECK(nc_inq_vartype(ncid, varid, &xt));
  /* libnetcdf writes a whole element of the variable's type into `v`,
     so refuse any type the union below cannot hold before asking --
     an opaque's element can be any size. */
  if ( nc_xtype_size(xt) < 0 ) {
    rb_raise(rb_eNotImpError, "unsupported nc_type: %d", xt);
  }

  memset(&v, 0, sizeof(v));
  NC_CHECK(nc_inq_var_fill(ncid, varid, &no_fill, &v));

  switch ( xt ) {
  case NC_CHAR:   rfill = rb_utf8_str_new(&v.text, 1); break;
  case NC_BYTE:   rfill = INT2NUM(v.c);                break;
  case NC_UBYTE:  rfill = UINT2NUM(v.uc);              break;
  case NC_SHORT:  rfill = INT2NUM(v.s);                break;
  case NC_USHORT: rfill = UINT2NUM(v.us);              break;
  case NC_INT:    rfill = INT2NUM(v.i);                break;
  case NC_UINT:   rfill = UINT2NUM(v.ui);              break;
  case NC_INT64:  rfill = LL2NUM(v.ll);                break;
  case NC_UINT64: rfill = ULL2NUM(v.ull);              break;
  case NC_FLOAT:  rfill = DBL2NUM(v.f);                break;
  case NC_DOUBLE: rfill = DBL2NUM(v.d);                break;
  default:
    rb_raise(rb_eNotImpError, "unsupported nc_type: %d", xt);
  }
  return rb_ary_new_from_args(2, no_fill ? Qtrue : Qfalse, rfill);
}

static VALUE
rb_nc_def_var_deflate (VALUE self, VALUE rncid, VALUE rvarid,
                       VALUE rshuffle, VALUE rdeflate, VALUE rlevel)
{
  NC_CHECK(nc_def_var_deflate(NUM2INT(rncid), NUM2INT(rvarid),
                              RTEST(rshuffle), RTEST(rdeflate), NUM2INT(rlevel)));
  return Qnil;
}

static VALUE
rb_nc_inq_var_deflate (VALUE self, VALUE rncid, VALUE rvarid)
{
  int shuffle, deflate, level;

  NC_CHECK(nc_inq_var_deflate(NUM2INT(rncid), NUM2INT(rvarid),
                              &shuffle, &deflate, &level));
  return rb_ary_new_from_args(3,
                              shuffle ? Qtrue : Qfalse,
                              deflate ? Qtrue : Qfalse,
                              INT2NUM(level));
}

static VALUE
rb_nc_def_var_chunking (VALUE self, VALUE rncid, VALUE rvarid,
                        VALUE rstorage, VALUE rsizes)
{
  size_t sizes[NC_MAX_VAR_DIMS];
  int ncid, varid, storage, ndims;

  ncid    = NUM2INT(rncid);
  varid   = NUM2INT(rvarid);
  storage = NUM2INT(rstorage);

  if ( storage == NC_CHUNKED ) {
    NC_CHECK(nc_inq_varndims(ncid, varid, &ndims));
    ary_to_sizet(rsizes, ndims, "chunksizes", sizes);
  }
  NC_CHECK(nc_def_var_chunking(ncid, varid, storage,
                               ( storage == NC_CHUNKED ) ? sizes : NULL));
  return Qnil;
}

static VALUE
rb_nc_inq_var_chunking (VALUE self, VALUE rncid, VALUE rvarid)
{
  VALUE rsizes;
  size_t *sizes;
  int ncid, varid, ndims, storage, st, i;

  ncid  = NUM2INT(rncid);
  varid = NUM2INT(rvarid);

  NC_CHECK(nc_inq_varndims(ncid, varid, &ndims));

  sizes = ALLOC_N(size_t, ndims > 0 ? ndims : 1);
  st = nc_inq_var_chunking(ncid, varid, &storage, sizes);
  if ( st != NC_NOERR ) {
    xfree(sizes);
    nc_raise(st);
  }

  rsizes = rb_ary_new_capa(ndims);
  for (i=0; i<ndims; i++) {
    rb_ary_push(rsizes, SIZET2NUM(sizes[i]));
  }
  xfree(sizes);
  return rb_ary_new_from_args(2, INT2NUM(storage), rsizes);
}

static VALUE
rb_nc_def_var_fletcher32 (VALUE self, VALUE rncid, VALUE rvarid, VALUE ron)
{
  NC_CHECK(nc_def_var_fletcher32(NUM2INT(rncid), NUM2INT(rvarid),
                                 RTEST(ron) ? NC_FLETCHER32 : NC_NOCHECKSUM));
  return Qnil;
}

static VALUE
rb_nc_inq_var_fletcher32 (VALUE self, VALUE rncid, VALUE rvarid)
{
  int v;

  NC_CHECK(nc_inq_var_fletcher32(NUM2INT(rncid), NUM2INT(rvarid), &v));
  return ( v == NC_FLETCHER32 ) ? Qtrue : Qfalse;
}

static VALUE
rb_nc_def_var_endian (VALUE self, VALUE rncid, VALUE rvarid, VALUE rendian)
{
  NC_CHECK(nc_def_var_endian(NUM2INT(rncid), NUM2INT(rvarid), NUM2INT(rendian)));
  return Qnil;
}

static VALUE
rb_nc_inq_var_endian (VALUE self, VALUE rncid, VALUE rvarid)
{
  int e;

  NC_CHECK(nc_inq_var_endian(NUM2INT(rncid), NUM2INT(rvarid), &e));
  return INT2NUM(e);
}

/* ---------------------------------------------------------- */
/* Filter (NetCDF-4 / HDF5 — register HDF5 filter on a var)    */
/* ---------------------------------------------------------- */

#ifdef HAVE_NC_DEF_VAR_FILTER
/* nc_def_var_filter(ncid, varid, filter_id, params)
 *   filter_id : Integer (e.g. 32015 for Zstd)
 *   params    : Array<Integer> (raw unsigned 32-bit values) */
static VALUE
rb_nc_def_var_filter (VALUE self, VALUE rncid, VALUE rvarid, VALUE rid, VALUE rparams)
{
  VALUE vparams;
  unsigned int *params;
  long n, i;
  int st;

  Check_Type(rparams, T_ARRAY);
  n = RARRAY_LEN(rparams);
  params = ALLOCV_N(unsigned int, vparams, n > 0 ? n : 1);
  for (i=0; i<n; i++) {
    params[i] = NUM2UINT(RARRAY_AREF(rparams, i));
  }
  st = nc_def_var_filter(NUM2INT(rncid), NUM2INT(rvarid),
                         NUM2UINT(rid), (size_t) n, params);
  ALLOCV_END(vparams);
  if ( st != NC_NOERR ) {
    nc_raise(st);
  }
  return Qnil;
}
#endif  /* HAVE_NC_DEF_VAR_FILTER */

#ifdef HAVE_NC_INQ_VAR_FILTER_INFO
/* Returns the param list for one filter on a var, or nil if the variable
   doesn't carry that filter. */
static VALUE
rb_nc_inq_var_filter_info (VALUE self, VALUE rncid, VALUE rvarid, VALUE rid)
{
  VALUE out;
  unsigned int *params;
  unsigned int id;
  size_t n, i;
  int ncid, varid, st;

  ncid  = NUM2INT(rncid);
  varid = NUM2INT(rvarid);
  id    = NUM2UINT(rid);

  st = nc_inq_var_filter_info(ncid, varid, id, &n, NULL);
  if ( st == NC_ENOFILTER ) {
    return Qnil;
  }
  if ( st != NC_NOERR ) {
    nc_raise(st);
  }

  params = ALLOC_N(unsigned int, n > 0 ? n : 1);
  st = nc_inq_var_filter_info(ncid, varid, id, NULL, params);
  if ( st != NC_NOERR ) {
    xfree(params);
    nc_raise(st);
  }
  out = rb_ary_new_capa((long) n);
  for (i=0; i<n; i++) {
    rb_ary_push(out, UINT2NUM(params[i]));
  }
  xfree(params);
  return out;
}
#endif  /* HAVE_NC_INQ_VAR_FILTER_INFO */

#ifdef HAVE_NC_INQ_VAR_FILTER_IDS
/* Returns the list of filter IDs registered on a variable. */
static VALUE
rb_nc_inq_var_filter_ids (VALUE self, VALUE rncid, VALUE rvarid)
{
  VALUE out;
  unsigned int *ids;
  size_t n, i;
  int ncid, varid, st;

  ncid  = NUM2INT(rncid);
  varid = NUM2INT(rvarid);

  NC_CHECK(nc_inq_var_filter_ids(ncid, varid, &n, NULL));
  ids = ALLOC_N(unsigned int, n > 0 ? n : 1);
  st = nc_inq_var_filter_ids(ncid, varid, NULL, ids);
  if ( st != NC_NOERR ) {
    xfree(ids);
    nc_raise(st);
  }
  out = rb_ary_new_capa((long) n);
  for (i=0; i<n; i++) {
    rb_ary_push(out, UINT2NUM(ids[i]));
  }
  xfree(ids);
  return out;
}
#endif  /* HAVE_NC_INQ_VAR_FILTER_IDS */

#ifdef HAVE_NC_INQ_FILTER_AVAIL
/* Returns true if the given filter is available in the linked libnetcdf
   for the given file's format.  nc_inq_filter_avail returns NC_NOERR when
   it's usable, NC_ENOFILTER otherwise. */
static VALUE
rb_nc_inq_filter_avail (VALUE self, VALUE rncid, VALUE rid)
{
  int st;

  st = nc_inq_filter_avail(NUM2INT(rncid), NUM2UINT(rid));
  if ( st == NC_NOERR ) {
    return Qtrue;
  }
  if ( st == NC_ENOFILTER ) {
    return Qfalse;
  }
  nc_raise(st);
}
#endif

/* ---------------------------------------------------------- */
/* Compression filters with their own calls                    */
/* ---------------------------------------------------------- */

/* Whether the filter can be used depends on how libnetcdf was built,
   but the calls are there either way.  A libnetcdf built without blosc
   returns NC_NOERR from both blosc calls and sets nothing, so every inq
   here starts its outputs at 0: a variable without the filter then
   reads back as not having it.

   Each def call here first asks whether the filter is there, and
   raises NC_ENOFILTER when it is not: the blosc def would otherwise
   return without adding the filter, and the data would be written
   uncompressed.  The check needs nc_inq_filter_avail (4.7+); the szip
   def in an older libnetcdf goes without it. */

#ifdef HAVE_NC_INQ_FILTER_AVAIL
static void
check_filter_available (int ncid, unsigned id)
{
  NC_CHECK(nc_inq_filter_avail(ncid, id));
}
#endif

#ifdef HAVE_NC_DEF_VAR_SZIP
static VALUE
rb_nc_def_var_szip (VALUE self, VALUE rncid, VALUE rvarid,
                    VALUE roptions_mask, VALUE rpixels_per_block)
{
#ifdef HAVE_NC_INQ_FILTER_AVAIL
  check_filter_available(NUM2INT(rncid), H5Z_FILTER_SZIP);
#endif
  NC_CHECK(nc_def_var_szip(NUM2INT(rncid), NUM2INT(rvarid),
                           NUM2INT(roptions_mask), NUM2INT(rpixels_per_block)));
  return Qnil;
}
#endif

#ifdef HAVE_NC_INQ_VAR_SZIP
static VALUE
rb_nc_inq_var_szip (VALUE self, VALUE rncid, VALUE rvarid)
{
  int options_mask, pixels_per_block;

  options_mask     = 0;
  pixels_per_block = 0;
  NC_CHECK(nc_inq_var_szip(NUM2INT(rncid), NUM2INT(rvarid),
                           &options_mask, &pixels_per_block));
  return rb_ary_new_from_args(2, INT2NUM(options_mask), INT2NUM(pixels_per_block));
}
#endif

#ifdef HAVE_NC_DEF_VAR_ZSTANDARD
static VALUE
rb_nc_def_var_zstandard (VALUE self, VALUE rncid, VALUE rvarid, VALUE rlevel)
{
  check_filter_available(NUM2INT(rncid), H5Z_FILTER_ZSTD);
  NC_CHECK(nc_def_var_zstandard(NUM2INT(rncid), NUM2INT(rvarid), NUM2INT(rlevel)));
  return Qnil;
}
#endif

#ifdef HAVE_NC_INQ_VAR_ZSTANDARD
static VALUE
rb_nc_inq_var_zstandard (VALUE self, VALUE rncid, VALUE rvarid)
{
  int has, level;

  has   = 0;
  level = 0;
  NC_CHECK(nc_inq_var_zstandard(NUM2INT(rncid), NUM2INT(rvarid), &has, &level));
  return rb_ary_new_from_args(2, has ? Qtrue : Qfalse, INT2NUM(level));
}
#endif

#ifdef HAVE_NC_DEF_VAR_BZIP2
static VALUE
rb_nc_def_var_bzip2 (VALUE self, VALUE rncid, VALUE rvarid, VALUE rlevel)
{
  check_filter_available(NUM2INT(rncid), H5Z_FILTER_BZIP2);
  NC_CHECK(nc_def_var_bzip2(NUM2INT(rncid), NUM2INT(rvarid), NUM2INT(rlevel)));
  return Qnil;
}
#endif

#ifdef HAVE_NC_INQ_VAR_BZIP2
static VALUE
rb_nc_inq_var_bzip2 (VALUE self, VALUE rncid, VALUE rvarid)
{
  int has, level;

  has   = 0;
  level = 0;
  NC_CHECK(nc_inq_var_bzip2(NUM2INT(rncid), NUM2INT(rvarid), &has, &level));
  return rb_ary_new_from_args(2, has ? Qtrue : Qfalse, INT2NUM(level));
}
#endif

#ifdef HAVE_NC_DEF_VAR_BLOSC
static VALUE
rb_nc_def_var_blosc (VALUE self, VALUE rncid, VALUE rvarid, VALUE rsubcompressor,
                     VALUE rlevel, VALUE rblocksize, VALUE raddshuffle)
{
  check_filter_available(NUM2INT(rncid), H5Z_FILTER_BLOSC);
  NC_CHECK(nc_def_var_blosc(NUM2INT(rncid), NUM2INT(rvarid),
                            NUM2UINT(rsubcompressor), NUM2UINT(rlevel),
                            NUM2UINT(rblocksize), NUM2UINT(raddshuffle)));
  return Qnil;
}
#endif

#ifdef HAVE_NC_INQ_VAR_BLOSC
static VALUE
rb_nc_inq_var_blosc (VALUE self, VALUE rncid, VALUE rvarid)
{
  int has;
  unsigned subcompressor, level, blocksize, addshuffle;

  has           = 0;
  subcompressor = 0;
  level         = 0;
  blocksize     = 0;
  addshuffle    = 0;
  NC_CHECK(nc_inq_var_blosc(NUM2INT(rncid), NUM2INT(rvarid), &has,
                            &subcompressor, &level, &blocksize, &addshuffle));
  return rb_ary_new_from_args(5, has ? Qtrue : Qfalse,
                              UINT2NUM(subcompressor), UINT2NUM(level),
                              UINT2NUM(blocksize), UINT2NUM(addshuffle));
}
#endif

/* ---------------------------------------------------------- */
/* Quantize (lossy bit-grooming for floating-point data)       */
/* ---------------------------------------------------------- */

#ifdef HAVE_NC_DEF_VAR_QUANTIZE
static VALUE
rb_nc_def_var_quantize (VALUE self, VALUE rncid, VALUE rvarid, VALUE rmode, VALUE rnsd)
{
  NC_CHECK(nc_def_var_quantize(NUM2INT(rncid), NUM2INT(rvarid),
                               NUM2INT(rmode), NUM2INT(rnsd)));
  return Qnil;
}
#endif

#ifdef HAVE_NC_INQ_VAR_QUANTIZE
static VALUE
rb_nc_inq_var_quantize (VALUE self, VALUE rncid, VALUE rvarid)
{
  int mode, nsd;

  NC_CHECK(nc_inq_var_quantize(NUM2INT(rncid), NUM2INT(rvarid), &mode, &nsd));
  return rb_ary_new_from_args(2, INT2NUM(mode), INT2NUM(nsd));
}
#endif

/* ---------------------------------------------------------- */
/* Per-variable chunk cache                                    */
/* ---------------------------------------------------------- */

static VALUE
rb_nc_set_var_chunk_cache (VALUE self, VALUE rncid, VALUE rvarid,
                           VALUE rsize, VALUE rnelems, VALUE rpreemption)
{
  NC_CHECK(nc_set_var_chunk_cache(NUM2INT(rncid), NUM2INT(rvarid),
                                  NUM2SIZET(rsize), NUM2SIZET(rnelems),
                                  (float) NUM2DBL(rpreemption)));
  return Qnil;
}

static VALUE
rb_nc_get_var_chunk_cache (VALUE self, VALUE rncid, VALUE rvarid)
{
  size_t size, nelems;
  float  pre;

  NC_CHECK(nc_get_var_chunk_cache(NUM2INT(rncid), NUM2INT(rvarid),
                                  &size, &nelems, &pre));
  return rb_ary_new_from_args(3, SIZET2NUM(size), SIZET2NUM(nelems),
                              DBL2NUM((double) pre));
}

void
init_nc_var (void)
{
  rb_define_module_function(mNetCDF, "nc_def_var",       rb_nc_def_var,       4);
  rb_define_module_function(mNetCDF, "nc_inq_varid",     rb_nc_inq_varid,     2);
  rb_define_module_function(mNetCDF, "nc_inq_var",       rb_nc_inq_var,       2);
  rb_define_module_function(mNetCDF, "nc_inq_varname",   rb_nc_inq_varname,   2);
  rb_define_module_function(mNetCDF, "nc_inq_vartype",   rb_nc_inq_vartype,   2);
  rb_define_module_function(mNetCDF, "nc_inq_varndims",  rb_nc_inq_varndims,  2);
  rb_define_module_function(mNetCDF, "nc_inq_vardimid",  rb_nc_inq_vardimid,  2);
  rb_define_module_function(mNetCDF, "nc_inq_varnatts",  rb_nc_inq_varnatts,  2);
  rb_define_module_function(mNetCDF, "nc_rename_var",    rb_nc_rename_var,    3);

  rb_define_module_function(mNetCDF, "nc_def_var_fill",       rb_nc_def_var_fill,       4);
  rb_define_module_function(mNetCDF, "nc_inq_var_fill",       rb_nc_inq_var_fill,       2);
  rb_define_module_function(mNetCDF, "nc_def_var_deflate",    rb_nc_def_var_deflate,    5);
  rb_define_module_function(mNetCDF, "nc_inq_var_deflate",    rb_nc_inq_var_deflate,    2);
  rb_define_module_function(mNetCDF, "nc_def_var_chunking",   rb_nc_def_var_chunking,   4);
  rb_define_module_function(mNetCDF, "nc_inq_var_chunking",   rb_nc_inq_var_chunking,   2);
  rb_define_module_function(mNetCDF, "nc_def_var_fletcher32", rb_nc_def_var_fletcher32, 3);
  rb_define_module_function(mNetCDF, "nc_inq_var_fletcher32", rb_nc_inq_var_fletcher32, 2);
  rb_define_module_function(mNetCDF, "nc_def_var_endian",     rb_nc_def_var_endian,     3);
  rb_define_module_function(mNetCDF, "nc_inq_var_endian",     rb_nc_inq_var_endian,     2);

#ifdef HAVE_NC_DEF_VAR_FILTER
  rb_define_module_function(mNetCDF, "nc_def_var_filter",      rb_nc_def_var_filter,      4);
#endif
#ifdef HAVE_NC_INQ_VAR_FILTER_INFO
  rb_define_module_function(mNetCDF, "nc_inq_var_filter_info", rb_nc_inq_var_filter_info, 3);
#endif
#ifdef HAVE_NC_INQ_VAR_FILTER_IDS
  rb_define_module_function(mNetCDF, "nc_inq_var_filter_ids",  rb_nc_inq_var_filter_ids,  2);
#endif
#ifdef HAVE_NC_INQ_FILTER_AVAIL
  rb_define_module_function(mNetCDF, "nc_inq_filter_avail",    rb_nc_inq_filter_avail,    2);
#endif

#ifdef HAVE_NC_DEF_VAR_SZIP
  rb_define_module_function(mNetCDF, "nc_def_var_szip",        rb_nc_def_var_szip,        4);
#endif
#ifdef HAVE_NC_INQ_VAR_SZIP
  rb_define_module_function(mNetCDF, "nc_inq_var_szip",        rb_nc_inq_var_szip,        2);
#endif
#ifdef HAVE_NC_DEF_VAR_ZSTANDARD
  rb_define_module_function(mNetCDF, "nc_def_var_zstandard",   rb_nc_def_var_zstandard,   3);
#endif
#ifdef HAVE_NC_INQ_VAR_ZSTANDARD
  rb_define_module_function(mNetCDF, "nc_inq_var_zstandard",   rb_nc_inq_var_zstandard,   2);
#endif
#ifdef HAVE_NC_DEF_VAR_BZIP2
  rb_define_module_function(mNetCDF, "nc_def_var_bzip2",       rb_nc_def_var_bzip2,       3);
#endif
#ifdef HAVE_NC_INQ_VAR_BZIP2
  rb_define_module_function(mNetCDF, "nc_inq_var_bzip2",       rb_nc_inq_var_bzip2,       2);
#endif
#ifdef HAVE_NC_DEF_VAR_BLOSC
  rb_define_module_function(mNetCDF, "nc_def_var_blosc",       rb_nc_def_var_blosc,       6);
#endif
#ifdef HAVE_NC_INQ_VAR_BLOSC
  rb_define_module_function(mNetCDF, "nc_inq_var_blosc",       rb_nc_inq_var_blosc,       2);
#endif

#ifdef HAVE_NC_DEF_VAR_QUANTIZE
  rb_define_module_function(mNetCDF, "nc_def_var_quantize",    rb_nc_def_var_quantize,    4);
#endif
#ifdef HAVE_NC_INQ_VAR_QUANTIZE
  rb_define_module_function(mNetCDF, "nc_inq_var_quantize",    rb_nc_inq_var_quantize,    2);
#endif

  rb_define_module_function(mNetCDF, "nc_set_var_chunk_cache", rb_nc_set_var_chunk_cache, 5);
  rb_define_module_function(mNetCDF, "nc_get_var_chunk_cache", rb_nc_get_var_chunk_cache, 2);
}
