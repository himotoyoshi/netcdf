#include "internal.h"

static VALUE
rb_nc_create (int argc, VALUE *argv, VALUE self)
{
  VALUE rpath, rmode;
  int cmode, ncid;

  rb_scan_args(argc, argv, "11", &rpath, &rmode);
  cmode = NIL_P(rmode) ? NC_CLOBBER : NUM2INT(rmode);
  NC_CHECK(nc_create(StringValueCStr(rpath), cmode, &ncid));
  return INT2NUM(ncid);
}

static VALUE
rb_nc_open (int argc, VALUE *argv, VALUE self)
{
  VALUE rpath, rmode;
  int omode, ncid;

  rb_scan_args(argc, argv, "11", &rpath, &rmode);
  omode = NIL_P(rmode) ? NC_NOWRITE : NUM2INT(rmode);
  NC_CHECK(nc_open(StringValueCStr(rpath), omode, &ncid));
  return INT2NUM(ncid);
}

#ifdef HAVE_NC_OPEN_MEM
/* nc_open_mem passes the buffer to libnetcdf with NC_MEMIO_LOCKED: the
   library reads the caller's memory in place (it does not copy it) until
   the file is closed, and never frees it.  A Ruby String's buffer may be
   moved by compaction or freed while the file is still open, so a
   malloc'd copy is handed over instead and kept in this list until the
   file is closed (nc_close / nc_abort / nc_close_memio).

   nc_open_memio without NC_MEMIO_LOCKED would let the library own and
   free the copy, but whether it frees the memory when the open fails
   depends on where it fails (netcdf-c 4.10.1 frees it for a broken
   header but not for an unknown format), so the copy is owned here. */
typedef struct mem_image {
  int ncid;
  void *memory;
  struct mem_image *next;
} mem_image_t;

static mem_image_t *mem_images = NULL;

/* Free the copies whose files are no longer open.  Each stored ncid is
   asked whether it is still open instead of being matched against the id
   passed to nc_close, so a file closed through another id is covered as
   well.  Returns true if `memory` was one of the freed copies. */
static bool
mem_image_release_closed (const void *memory)
{
  mem_image_t **pp;
  mem_image_t *m;
  bool released;
  int fmt;

  released = false;
  pp = &mem_images;
  while ( *pp ) {
    m = *pp;
    if ( nc_inq_format(m->ncid, &fmt) == NC_EBADID ) {
      if ( m->memory == memory ) {
        released = true;
      }
      *pp = m->next;
      free(m->memory);
      xfree(m);
    }
    else {
      pp = &m->next;
    }
  }
  return released;
}

static VALUE
rb_nc_open_mem (VALUE self, VALUE rpath, VALUE rmode, VALUE rbytes)
{
  const char *path;
  mem_image_t *m;
  long len;
  int omode, st;

  omode = NUM2INT(rmode);
  path  = StringValueCStr(rpath);
  StringValue(rbytes);
  len = RSTRING_LEN(rbytes);
  /* libnetcdf keeps using this memory until the file is closed. */
  m = ALLOC(mem_image_t);
  m->memory = malloc(len > 0 ? (size_t) len : 1);
  if ( ! m->memory ) {
    xfree(m);
    rb_memerror();
  }
  memcpy(m->memory, RSTRING_PTR(rbytes), (size_t) len);
  st = nc_open_mem(path, omode, (size_t) len, m->memory, &m->ncid);
  if ( st != NC_NOERR ) {
    free(m->memory);
    xfree(m);
    nc_raise(st);
  }
  mem_image_release_closed(NULL);
  m->next    = mem_images;
  mem_images = m;
  return INT2NUM(m->ncid);
}
#else
# define mem_image_release_closed(memory) false
#endif

#ifdef HAVE_NC_CREATE_MEM
static VALUE
rb_nc_create_mem (int argc, VALUE *argv, VALUE self)
{
  VALUE rpath, rmode, rinitialsize;
  int cmode, ncid;
  size_t isz;

  rb_scan_args(argc, argv, "12", &rpath, &rmode, &rinitialsize);
  cmode = NIL_P(rmode) ? NC_NETCDF4 : NUM2INT(rmode);
  isz   = NIL_P(rinitialsize) ? 0 : NUM2SIZET(rinitialsize);
  NC_CHECK(nc_create_mem(StringValueCStr(rpath), cmode, isz, &ncid));
  return INT2NUM(ncid);
}
#endif

#ifdef HAVE_NC_CLOSE_MEMIO
static VALUE
rb_nc_close_memio (VALUE self, VALUE rncid)
{
  NC_memio memio;
  VALUE out;

  memset(&memio, 0, sizeof(memio));
  /* flags=0: ownership of memio.memory transfers to us; we must free it. */
  NC_CHECK(nc_close_memio(NUM2INT(rncid), &memio));
  out = rb_str_new((const char *) memio.memory, (long) memio.size);
  /* For a handle from nc_open_mem, memio.memory is the copy made there;
     mem_image_release_closed frees it (once), so it is not freed here. */
  if ( ! mem_image_release_closed(memio.memory) ) {
    free(memio.memory);
  }
  return out;
}
#endif

static VALUE
rb_nc_close (VALUE self, VALUE rncid)
{
  NC_CHECK(nc_close(NUM2INT(rncid)));
  mem_image_release_closed(NULL);
  return Qnil;
}

static VALUE
rb_nc_abort (VALUE self, VALUE rncid)
{
  NC_CHECK(nc_abort(NUM2INT(rncid)));
  mem_image_release_closed(NULL);
  return Qnil;
}

static VALUE
rb_nc_sync (VALUE self, VALUE rncid)
{
  NC_CHECK(nc_sync(NUM2INT(rncid)));
  return Qnil;
}

static VALUE
rb_nc_redef (VALUE self, VALUE rncid)
{
  NC_CHECK(nc_redef(NUM2INT(rncid)));
  return Qnil;
}

static VALUE
rb_nc_enddef (VALUE self, VALUE rncid)
{
  NC_CHECK(nc_enddef(NUM2INT(rncid)));
  return Qnil;
}

static VALUE
rb_nc_inq (VALUE self, VALUE rncid)
{
  VALUE rudim;
  int ndims, nvars, natts, unlimdim;

  NC_CHECK(nc_inq(NUM2INT(rncid), &ndims, &nvars, &natts, &unlimdim));
  rudim = ( unlimdim < 0 ) ? Qnil : INT2NUM(unlimdim);
  return rb_ary_new_from_args(4, INT2NUM(ndims), INT2NUM(nvars), INT2NUM(natts), rudim);
}

static VALUE
rb_nc_inq_ndims (VALUE self, VALUE rncid)
{
  int n;

  NC_CHECK(nc_inq_ndims(NUM2INT(rncid), &n));
  return INT2NUM(n);
}

static VALUE
rb_nc_inq_nvars (VALUE self, VALUE rncid)
{
  int n;

  NC_CHECK(nc_inq_nvars(NUM2INT(rncid), &n));
  return INT2NUM(n);
}

static VALUE
rb_nc_inq_natts (VALUE self, VALUE rncid)
{
  int n;

  NC_CHECK(nc_inq_natts(NUM2INT(rncid), &n));
  return INT2NUM(n);
}

static VALUE
rb_nc_inq_unlimdim (VALUE self, VALUE rncid)
{
  int d;

  NC_CHECK(nc_inq_unlimdim(NUM2INT(rncid), &d));
  return ( d < 0 ) ? Qnil : INT2NUM(d);
}

static VALUE
rb_nc_inq_unlimdims (VALUE self, VALUE rncid)
{
  VALUE out;
  int n, st, i;
  int *ids;

  NC_CHECK(nc_inq_unlimdims(NUM2INT(rncid), &n, NULL));
  ids = ALLOC_N(int, n > 0 ? n : 1);
  st = nc_inq_unlimdims(NUM2INT(rncid), NULL, ids);
  if ( st != NC_NOERR ) {
    xfree(ids);
    nc_raise(st);
  }
  out = rb_ary_new_capa(n);
  for (i=0; i<n; i++) {
    rb_ary_push(out, INT2NUM(ids[i]));
  }
  xfree(ids);
  return out;
}

#ifdef HAVE_NC_INQ_PATH
static VALUE
rb_nc_inq_path (VALUE self, VALUE rncid)
{
  VALUE out;
  size_t len;

  NC_CHECK(nc_inq_path(NUM2INT(rncid), &len, NULL));
  out = rb_str_buf_new((long) len);
  rb_str_set_len(out, (long) len);
  NC_CHECK(nc_inq_path(NUM2INT(rncid), NULL, RSTRING_PTR(out)));
  return out;
}
#endif

static VALUE
rb_nc_inq_format (VALUE self, VALUE rncid)
{
  int fmt;

  NC_CHECK(nc_inq_format(NUM2INT(rncid), &fmt));
  return INT2NUM(fmt);
}

#ifdef HAVE_NC_INQ_FORMAT_EXTENDED
static VALUE
rb_nc_inq_format_extended (VALUE self, VALUE rncid)
{
  int fmt, mode;

  NC_CHECK(nc_inq_format_extended(NUM2INT(rncid), &fmt, &mode));
  return rb_ary_new_from_args(2, INT2NUM(fmt), INT2NUM(mode));
}
#endif

static VALUE
rb_nc_inq_libvers (VALUE self)
{
  return rb_str_new_cstr(nc_inq_libvers());
}

static VALUE
rb_nc_inq_type (VALUE self, VALUE rncid, VALUE rxtype)
{
  char name[NC_MAX_NAME + 1];
  size_t sz;

  NC_CHECK(nc_inq_type(NUM2INT(rncid), NUM2INT(rxtype), name, &sz));
  return rb_ary_new_from_args(2, rb_utf8_str_new_cstr(name), SIZET2NUM(sz));
}

static VALUE
rb_nc_set_fill (VALUE self, VALUE rncid, VALUE rmode)
{
  int old;

  NC_CHECK(nc_set_fill(NUM2INT(rncid), NUM2INT(rmode), &old));
  return INT2NUM(old);
}

/* Process-global format used by nc_create when the mode names none. */
static VALUE
rb_nc_set_default_format (VALUE self, VALUE rformat)
{
  int old;

  NC_CHECK(nc_set_default_format(NUM2INT(rformat), &old));
  return INT2NUM(old);
}

/* Process-global HDF5 chunk cache parameters. */
static VALUE
rb_nc_set_chunk_cache (VALUE self, VALUE rsize, VALUE rnelems, VALUE rpreemption)
{
  NC_CHECK(nc_set_chunk_cache(NUM2SIZET(rsize), NUM2SIZET(rnelems),
                              (float) NUM2DBL(rpreemption)));
  return Qnil;
}

static VALUE
rb_nc_get_chunk_cache (VALUE self)
{
  size_t size, nelems;
  float  pre;

  NC_CHECK(nc_get_chunk_cache(&size, &nelems, &pre));
  return rb_ary_new_from_args(3, SIZET2NUM(size), SIZET2NUM(nelems),
                              DBL2NUM((double) pre));
}

void
init_nc_file (void)
{
  rb_define_module_function(mNetCDF, "nc_create",              rb_nc_create,              -1);
  rb_define_module_function(mNetCDF, "nc_open",                rb_nc_open,                -1);
#ifdef HAVE_NC_OPEN_MEM
  rb_define_module_function(mNetCDF, "nc_open_mem",            rb_nc_open_mem,             3);
#endif
#ifdef HAVE_NC_CREATE_MEM
  rb_define_module_function(mNetCDF, "nc_create_mem",          rb_nc_create_mem,          -1);
#endif
#ifdef HAVE_NC_CLOSE_MEMIO
  rb_define_module_function(mNetCDF, "nc_close_memio",         rb_nc_close_memio,          1);
#endif
  rb_define_module_function(mNetCDF, "nc_close",               rb_nc_close,                1);
  rb_define_module_function(mNetCDF, "nc_abort",               rb_nc_abort,                1);
  rb_define_module_function(mNetCDF, "nc_sync",                rb_nc_sync,                 1);
  rb_define_module_function(mNetCDF, "nc_redef",               rb_nc_redef,                1);
  rb_define_module_function(mNetCDF, "nc_enddef",              rb_nc_enddef,               1);
  rb_define_module_function(mNetCDF, "nc_inq",                 rb_nc_inq,                  1);
  rb_define_module_function(mNetCDF, "nc_inq_ndims",           rb_nc_inq_ndims,            1);
  rb_define_module_function(mNetCDF, "nc_inq_nvars",           rb_nc_inq_nvars,            1);
  rb_define_module_function(mNetCDF, "nc_inq_natts",           rb_nc_inq_natts,            1);
  rb_define_module_function(mNetCDF, "nc_inq_unlimdim",        rb_nc_inq_unlimdim,         1);
  rb_define_module_function(mNetCDF, "nc_inq_unlimdims",       rb_nc_inq_unlimdims,        1);
#ifdef HAVE_NC_INQ_PATH
  rb_define_module_function(mNetCDF, "nc_inq_path",            rb_nc_inq_path,             1);
#endif
  rb_define_module_function(mNetCDF, "nc_inq_format",          rb_nc_inq_format,           1);
#ifdef HAVE_NC_INQ_FORMAT_EXTENDED
  rb_define_module_function(mNetCDF, "nc_inq_format_extended", rb_nc_inq_format_extended,  1);
#endif
  rb_define_module_function(mNetCDF, "nc_inq_libvers",         rb_nc_inq_libvers,          0);
  rb_define_module_function(mNetCDF, "nc_inq_type",            rb_nc_inq_type,             2);
  rb_define_module_function(mNetCDF, "nc_set_fill",            rb_nc_set_fill,             2);
  rb_define_module_function(mNetCDF, "nc_set_default_format",  rb_nc_set_default_format,   1);
  rb_define_module_function(mNetCDF, "nc_set_chunk_cache",     rb_nc_set_chunk_cache,      3);
  rb_define_module_function(mNetCDF, "nc_get_chunk_cache",     rb_nc_get_chunk_cache,      0);
}
