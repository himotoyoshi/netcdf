/*
 * User-defined NetCDF-4 types.
 *
 * Currently covers opaque, vlen, and enum.  Compound types are not yet
 * implemented; see API.md "Deferred features".
 */
#include "internal.h"

/* ---------------------------------------------------------- */
/* Generic user-type inquiry                                   */
/* ---------------------------------------------------------- */

/* Returns [name, size, base_type, nfields, class].
 *   class is one of NC_VLEN / NC_OPAQUE / NC_ENUM / NC_COMPOUND. */
static VALUE
rb_nc_inq_user_type (VALUE self, VALUE rncid, VALUE rtypeid)
{
  char    name[NC_MAX_NAME + 1];
  size_t  size, nfields;
  nc_type base;
  int     klass;

  NC_CHECK(nc_inq_user_type(NUM2INT(rncid), NUM2INT(rtypeid),
                            name, &size, &base, &nfields, &klass));
  return rb_ary_new_from_args(5,
                              rb_utf8_str_new_cstr(name), SIZET2NUM(size),
                              INT2NUM(base), SIZET2NUM(nfields), INT2NUM(klass));
}

/* Look up a type by name within a group.  Returns nil when not found. */
static VALUE
rb_nc_inq_typeid (VALUE self, VALUE rncid, VALUE rname)
{
  nc_type t;
  int st;

  st = nc_inq_typeid(NUM2INT(rncid), StringValueCStr(rname), &t);
  if ( st == NC_EBADTYPE || st == NC_ENOTNC4 ) {
    return Qnil;
  }
  if ( st != NC_NOERR ) {
    nc_raise(st);
  }
  return INT2NUM(t);
}

/* List user-defined typeids visible in a group. */
static VALUE
rb_nc_inq_typeids (VALUE self, VALUE rncid)
{
  VALUE out;
  int n, st, i;
  int *ids;

  NC_CHECK(nc_inq_typeids(NUM2INT(rncid), &n, NULL));
  ids = ALLOC_N(int, n > 0 ? n : 1);
  st = nc_inq_typeids(NUM2INT(rncid), NULL, ids);
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

/* The two types may live in different files or groups. */
static VALUE
rb_nc_inq_type_equal (VALUE self, VALUE rncid1, VALUE rtypeid1, VALUE rncid2, VALUE rtypeid2)
{
  int equal;

  NC_CHECK(nc_inq_type_equal(NUM2INT(rncid1), NUM2INT(rtypeid1),
                             NUM2INT(rncid2), NUM2INT(rtypeid2), &equal));
  return equal ? Qtrue : Qfalse;
}

/* ---------------------------------------------------------- */
/* Opaque (fixed-size byte type)                               */
/* ---------------------------------------------------------- */

static VALUE
rb_nc_def_opaque (VALUE self, VALUE rncid, VALUE rsize, VALUE rname)
{
  nc_type t;

  NC_CHECK(nc_def_opaque(NUM2INT(rncid), NUM2SIZET(rsize),
                         StringValueCStr(rname), &t));
  return INT2NUM(t);
}

/* Returns [name, size]. */
static VALUE
rb_nc_inq_opaque (VALUE self, VALUE rncid, VALUE rtypeid)
{
  char   name[NC_MAX_NAME + 1];
  size_t size;

  NC_CHECK(nc_inq_opaque(NUM2INT(rncid), NUM2INT(rtypeid), name, &size));
  return rb_ary_new_from_args(2, rb_utf8_str_new_cstr(name), SIZET2NUM(size));
}

/* ---------------------------------------------------------- */
/* Vlen (variable-length sequence)                             */
/* ---------------------------------------------------------- */

static VALUE
rb_nc_def_vlen (VALUE self, VALUE rncid, VALUE rname, VALUE rbase_type)
{
  nc_type t;

  NC_CHECK(nc_def_vlen(NUM2INT(rncid), StringValueCStr(rname),
                       NUM2INT(rbase_type), &t));
  return INT2NUM(t);
}

/* Returns [name, datum_size, base_type]. */
static VALUE
rb_nc_inq_vlen (VALUE self, VALUE rncid, VALUE rtypeid)
{
  char    name[NC_MAX_NAME + 1];
  size_t  datum_size;
  nc_type base;

  NC_CHECK(nc_inq_vlen(NUM2INT(rncid), NUM2INT(rtypeid),
                       name, &datum_size, &base));
  return rb_ary_new_from_args(3,
                              rb_utf8_str_new_cstr(name), SIZET2NUM(datum_size),
                              INT2NUM(base));
}

/* ---------------------------------------------------------- */
/* Enum                                                        */
/* ---------------------------------------------------------- */

static VALUE
rb_nc_def_enum (VALUE self, VALUE rncid, VALUE rbase_type, VALUE rname)
{
  nc_type t;

  NC_CHECK(nc_def_enum(NUM2INT(rncid), NUM2INT(rbase_type),
                       StringValueCStr(rname), &t));
  return INT2NUM(t);
}

/* Pack a Ruby integer into a buffer typed per the enum's base nc_type.
   Returns the byte size used; the caller passes the buffer. */
static size_t
pack_int_for_base (nc_type base, VALUE v, void *buf)
{
  switch ( base ) {
  case NC_BYTE:   *(signed char *) buf        = num_to_schar(v);     return 1;
  case NC_UBYTE:  *(unsigned char *) buf      = num_to_uchar(v);     return 1;
  case NC_SHORT:  *(short *) buf              = num_to_short(v);     return 2;
  case NC_USHORT: *(unsigned short *) buf     = num_to_ushort(v);    return 2;
  case NC_INT:    *(int *) buf                = NUM2INT(v);          return 4;
  case NC_UINT:   *(unsigned int *) buf       = num_to_uint(v);      return 4;
  case NC_INT64:  *(long long *) buf          = NUM2LL(v);           return 8;
  case NC_UINT64: *(unsigned long long *) buf = num_to_ulonglong(v); return 8;
  default:
    rb_raise(rb_eArgError, "enum base type must be an integer nc_type (got %d)", base);
  }
}

static VALUE
unpack_int_for_base (nc_type base, const void *buf)
{
  switch ( base ) {
  case NC_BYTE:   return INT2NUM(*(const signed char *) buf);
  case NC_UBYTE:  return UINT2NUM(*(const unsigned char *) buf);
  case NC_SHORT:  return INT2NUM(*(const short *) buf);
  case NC_USHORT: return UINT2NUM(*(const unsigned short *) buf);
  case NC_INT:    return INT2NUM(*(const int *) buf);
  case NC_UINT:   return UINT2NUM(*(const unsigned int *) buf);
  case NC_INT64:  return LL2NUM(*(const long long *) buf);
  case NC_UINT64: return ULL2NUM(*(const unsigned long long *) buf);
  default:
    rb_raise(rb_eArgError, "enum base type must be an integer nc_type (got %d)", base);
  }
}

static VALUE
rb_nc_insert_enum (VALUE self, VALUE rncid, VALUE rtypeid, VALUE rname, VALUE rvalue)
{
  int ncid;
  nc_type typeid, base;
  long long buf;     /* widest base type the API supports */

  ncid   = NUM2INT(rncid);
  typeid = NUM2INT(rtypeid);

  NC_CHECK(nc_inq_user_type(ncid, typeid, NULL, NULL, &base, NULL, NULL));

  buf = 0;
  pack_int_for_base(base, rvalue, &buf);
  NC_CHECK(nc_insert_enum(ncid, typeid, StringValueCStr(rname), &buf));
  return Qnil;
}

/* Returns [name, base_type, base_size, num_members]. */
static VALUE
rb_nc_inq_enum (VALUE self, VALUE rncid, VALUE rtypeid)
{
  char    name[NC_MAX_NAME + 1];
  nc_type base;
  size_t  base_size, nmembers;

  NC_CHECK(nc_inq_enum(NUM2INT(rncid), NUM2INT(rtypeid),
                       name, &base, &base_size, &nmembers));
  return rb_ary_new_from_args(4,
                              rb_utf8_str_new_cstr(name), INT2NUM(base),
                              SIZET2NUM(base_size), SIZET2NUM(nmembers));
}

/* Returns [name, value] for member at given zero-based index. */
static VALUE
rb_nc_inq_enum_member (VALUE self, VALUE rncid, VALUE rtypeid, VALUE ridx)
{
  char name[NC_MAX_NAME + 1];
  int ncid;
  nc_type typeid, base;
  long long buf;

  ncid   = NUM2INT(rncid);
  typeid = NUM2INT(rtypeid);

  NC_CHECK(nc_inq_user_type(ncid, typeid, NULL, NULL, &base, NULL, NULL));

  buf = 0;
  NC_CHECK(nc_inq_enum_member(ncid, typeid, NUM2INT(ridx), name, &buf));
  return rb_ary_new_from_args(2,
                              rb_utf8_str_new_cstr(name),
                              unpack_int_for_base(base, &buf));
}

/* Returns the member name for a given integer value, or nil if not found. */
static VALUE
rb_nc_inq_enum_ident (VALUE self, VALUE rncid, VALUE rtypeid, VALUE rvalue)
{
  char name[NC_MAX_NAME + 1];
  int ncid, st;
  nc_type typeid, base;
  long long v;

  ncid   = NUM2INT(rncid);
  typeid = NUM2INT(rtypeid);

  NC_CHECK(nc_inq_user_type(ncid, typeid, NULL, NULL, &base, NULL, NULL));

  /* nc_inq_enum_ident takes the value itself, not base-type bytes: it
     widens each member to long long and compares.  An NC_UINT64
     member above LLONG_MAX widens by the same wrap-around cast. */
  v = ( base == NC_UINT64 ) ? (long long) NUM2ULL(rvalue) : NUM2LL(rvalue);
  st = nc_inq_enum_ident(ncid, typeid, v, name);
  if ( st == NC_EINVAL ) {
    return Qnil; /* not a defined member */
  }
  if ( st != NC_NOERR ) {
    nc_raise(st);
  }
  return rb_utf8_str_new_cstr(name);
}

/* ---------------------------------------------------------- */
/* Init                                                        */
/* ---------------------------------------------------------- */

void
init_nc_types (void)
{
  rb_define_module_function(mNetCDF, "nc_inq_user_type",     rb_nc_inq_user_type,     2);
  rb_define_module_function(mNetCDF, "nc_inq_typeid",        rb_nc_inq_typeid,        2);
  rb_define_module_function(mNetCDF, "nc_inq_typeids",       rb_nc_inq_typeids,       1);
  rb_define_module_function(mNetCDF, "nc_inq_type_equal",    rb_nc_inq_type_equal,    4);

  rb_define_module_function(mNetCDF, "nc_def_opaque",        rb_nc_def_opaque,        3);
  rb_define_module_function(mNetCDF, "nc_inq_opaque",        rb_nc_inq_opaque,        2);

  rb_define_module_function(mNetCDF, "nc_def_vlen",          rb_nc_def_vlen,          3);
  rb_define_module_function(mNetCDF, "nc_inq_vlen",          rb_nc_inq_vlen,          2);

  rb_define_module_function(mNetCDF, "nc_def_enum",          rb_nc_def_enum,          3);
  rb_define_module_function(mNetCDF, "nc_insert_enum",       rb_nc_insert_enum,       4);
  rb_define_module_function(mNetCDF, "nc_inq_enum",          rb_nc_inq_enum,          2);
  rb_define_module_function(mNetCDF, "nc_inq_enum_member",   rb_nc_inq_enum_member,   3);
  rb_define_module_function(mNetCDF, "nc_inq_enum_ident",    rb_nc_inq_enum_ident,    3);
}
