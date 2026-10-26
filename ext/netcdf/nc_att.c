#include "internal.h"

/* ---------------------------------------------------------- */
/* Inquiry                                                     */
/* ---------------------------------------------------------- */

static VALUE
rb_nc_inq_attname (VALUE self, VALUE rncid, VALUE rvarid, VALUE rattnum)
{
  char name[NC_MAX_NAME + 1];

  NC_CHECK(nc_inq_attname(NUM2INT(rncid), NUM2INT(rvarid), NUM2INT(rattnum), name));
  return rb_utf8_str_new_cstr(name);
}

static VALUE
rb_nc_inq_attid (VALUE self, VALUE rncid, VALUE rvarid, VALUE rname)
{
  int attnum, st;

  st = nc_inq_attid(NUM2INT(rncid), NUM2INT(rvarid), StringValueCStr(rname), &attnum);
  if ( st == NC_ENOTATT ) {
    return Qnil;
  }
  if ( st != NC_NOERR ) {
    nc_raise(st);
  }
  return INT2NUM(attnum);
}

static VALUE
rb_nc_inq_att (VALUE self, VALUE rncid, VALUE rvarid, VALUE rname)
{
  nc_type xtype;
  size_t len;

  NC_CHECK(nc_inq_att(NUM2INT(rncid), NUM2INT(rvarid), StringValueCStr(rname), &xtype, &len));
  return rb_ary_new_from_args(2, INT2NUM(xtype), SIZET2NUM(len));
}

static VALUE
rb_nc_inq_atttype (VALUE self, VALUE rncid, VALUE rvarid, VALUE rname)
{
  nc_type xtype;

  NC_CHECK(nc_inq_atttype(NUM2INT(rncid), NUM2INT(rvarid), StringValueCStr(rname), &xtype));
  return INT2NUM(xtype);
}

static VALUE
rb_nc_inq_attlen (VALUE self, VALUE rncid, VALUE rvarid, VALUE rname)
{
  size_t len;

  NC_CHECK(nc_inq_attlen(NUM2INT(rncid), NUM2INT(rvarid), StringValueCStr(rname), &len));
  return SIZET2NUM(len);
}

static VALUE
rb_nc_rename_att (VALUE self, VALUE rncid, VALUE rvarid, VALUE rname, VALUE rnewname)
{
  NC_CHECK(nc_rename_att(NUM2INT(rncid), NUM2INT(rvarid),
                         StringValueCStr(rname), StringValueCStr(rnewname)));
  return Qnil;
}

static VALUE
rb_nc_del_att (VALUE self, VALUE rncid, VALUE rvarid, VALUE rname)
{
  NC_CHECK(nc_del_att(NUM2INT(rncid), NUM2INT(rvarid), StringValueCStr(rname)));
  return Qnil;
}

static VALUE
rb_nc_copy_att (VALUE self, VALUE rncid_in, VALUE rvarid_in, VALUE rname,
                VALUE rncid_out, VALUE rvarid_out)
{
  NC_CHECK(nc_copy_att(NUM2INT(rncid_in), NUM2INT(rvarid_in), StringValueCStr(rname),
                       NUM2INT(rncid_out), NUM2INT(rvarid_out)));
  return Qnil;
}

/* ---------------------------------------------------------- */
/* Typed put / get                                             */
/* ---------------------------------------------------------- */

static inline VALUE
att_value_to_ary (VALUE rvalue)
{
  if ( RB_TYPE_P(rvalue, T_ARRAY) ) {
    return rvalue;
  }
  return rb_ary_new_from_args(1, rvalue);
}

static VALUE
rb_nc_put_att_text (VALUE self, VALUE rncid, VALUE rvarid, VALUE rname, VALUE rtext)
{
  StringValue(rtext);
  NC_CHECK(nc_put_att_text(NUM2INT(rncid), NUM2INT(rvarid), StringValueCStr(rname),
                           (size_t) RSTRING_LEN(rtext), RSTRING_PTR(rtext)));
  return Qnil;
}

static VALUE
rb_nc_get_att_text (VALUE self, VALUE rncid, VALUE rvarid, VALUE rname)
{
  VALUE out;
  size_t len;

  NC_CHECK(nc_inq_attlen(NUM2INT(rncid), NUM2INT(rvarid), StringValueCStr(rname), &len));
  /* Text comes back as UTF-8, NC_STRING's encoding too: netCDF has no
     other, and CF attributes are text in practice. */
  out = rb_str_buf_new((long) len);
  rb_str_set_len(out, (long) len);
  rb_enc_associate(out, rb_utf8_encoding());
  NC_CHECK(nc_get_att_text(NUM2INT(rncid), NUM2INT(rvarid), StringValueCStr(rname),
                           RSTRING_PTR(out)));
  return out;
}

/* As in C, `xtype` is the attribute's type on disk and the suffix names
   the type of the values in memory; libnetcdf converts between them. */
#define DEFINE_ATT_NUM(suffix, ctype, num2c, c2num)                     \
  static VALUE                                                          \
  rb_nc_put_att_##suffix (VALUE self, VALUE rncid, VALUE rvarid,        \
                          VALUE rname, VALUE rxtype, VALUE rvalue)      \
  {                                                                     \
    VALUE rary, vbuf;                                                   \
    ctype *buf;                                                         \
    long n, i;                                                          \
    int st;                                                             \
                                                                        \
    rary = att_value_to_ary(rvalue);                                    \
    n    = RARRAY_LEN(rary);                                            \
    buf  = ALLOCV_N(ctype, vbuf, (n > 0 ? n : 1));                      \
    for (i=0; i<n; i++) {                                               \
      buf[i] = (ctype) num2c(RARRAY_AREF(rary, i));                     \
    }                                                                   \
    st = nc_put_att_##suffix(NUM2INT(rncid), NUM2INT(rvarid),           \
                             StringValueCStr(rname),                    \
                             NUM2INT(rxtype), (size_t) n, buf);         \
    ALLOCV_END(vbuf);                                                   \
    if ( st != NC_NOERR ) {                                             \
      nc_raise(st);                                                     \
    }                                                                   \
    return Qnil;                                                        \
  }                                                                     \
                                                                        \
  static VALUE                                                          \
  rb_nc_get_att_##suffix (VALUE self, VALUE rncid, VALUE rvarid,        \
                          VALUE rname)                                  \
  {                                                                     \
    VALUE out;                                                          \
    ctype *buf;                                                         \
    size_t len, i;                                                      \
    int st;                                                             \
                                                                        \
    NC_CHECK(nc_inq_attlen(NUM2INT(rncid), NUM2INT(rvarid),             \
                           StringValueCStr(rname), &len));              \
    buf = ALLOC_N(ctype, (len > 0 ? len : 1));                          \
    st = nc_get_att_##suffix(NUM2INT(rncid), NUM2INT(rvarid),           \
                             StringValueCStr(rname), buf);              \
    if ( st != NC_NOERR ) {                                             \
      xfree(buf);                                                       \
      nc_raise(st);                                                     \
    }                                                                   \
    out = rb_ary_new_capa((long) len);                                  \
    for (i=0; i<len; i++) {                                             \
      rb_ary_push(out, c2num(buf[i]));                                  \
    }                                                                   \
    xfree(buf);                                                         \
    return out;                                                         \
  }

DEFINE_ATT_NUM(schar,     signed char,        num_to_schar,     INT2NUM)
DEFINE_ATT_NUM(uchar,     unsigned char,      num_to_uchar,     UINT2NUM)
DEFINE_ATT_NUM(short,     short,              num_to_short,     INT2NUM)
DEFINE_ATT_NUM(ushort,    unsigned short,     num_to_ushort,    UINT2NUM)
DEFINE_ATT_NUM(int,       int,                NUM2INT,          INT2NUM)
DEFINE_ATT_NUM(uint,      unsigned int,       num_to_uint,      UINT2NUM)
DEFINE_ATT_NUM(longlong,  long long,          NUM2LL,           LL2NUM)
DEFINE_ATT_NUM(ulonglong, unsigned long long, num_to_ulonglong, ULL2NUM)
DEFINE_ATT_NUM(float,     float,              NUM2DBL,          DBL2NUM)
DEFINE_ATT_NUM(double,    double,             NUM2DBL,          DBL2NUM)

static VALUE
rb_nc_put_att_string (VALUE self, VALUE rncid, VALUE rvarid, VALUE rname, VALUE rvalue)
{
  VALUE rary, rstr, vptrs;
  char **ptrs;
  long n, i;
  int st;

  rary = att_value_to_ary(rvalue);
  n    = RARRAY_LEN(rary);
  ptrs = ALLOCV_N(char *, vptrs, n > 0 ? n : 1);
  for (i=0; i<n; i++) {
    rstr = RARRAY_AREF(rary, i);
    if ( ! RB_TYPE_P(rstr, T_STRING) ) {
      rb_raise(rb_eTypeError, "NC_STRING attribute elements must all be Strings");
    }
    ptrs[i] = StringValueCStr(rstr);
  }
  st = nc_put_att_string(NUM2INT(rncid), NUM2INT(rvarid), StringValueCStr(rname),
                         (size_t) n, (const char **) ptrs);
  ALLOCV_END(vptrs);
  if ( st != NC_NOERR ) {
    nc_raise(st);
  }
  return Qnil;
}

static VALUE
rb_nc_get_att_string (VALUE self, VALUE rncid, VALUE rvarid, VALUE rname)
{
  VALUE out;
  char **ptrs;
  size_t len, i;
  int st;

  NC_CHECK(nc_inq_attlen(NUM2INT(rncid), NUM2INT(rvarid), StringValueCStr(rname), &len));
  ptrs = ALLOC_N(char *, len > 0 ? len : 1);
  for (i=0; i<len; i++) {
    ptrs[i] = NULL;
  }
  st = nc_get_att_string(NUM2INT(rncid), NUM2INT(rvarid), StringValueCStr(rname), ptrs);
  if ( st != NC_NOERR ) {
    xfree(ptrs);
    nc_raise(st);
  }
  out = rb_ary_new_capa((long) len);
  for (i=0; i<len; i++) {
    rb_ary_push(out, rb_utf8_str_new_cstr(ptrs[i] ? ptrs[i] : ""));
  }
  nc_free_string(len, ptrs);
  xfree(ptrs);
  return out;
}

/* ---------------------------------------------------------- */
/* Smart generic put / get                                     */
/* ---------------------------------------------------------- */

/* The narrowest of NC_INT, NC_INT64 and NC_UINT64 that holds the Integer
   `v`; NC_UINT64 only for a value past 2**63 - 1.  Sized by bit count so
   that no conversion can raise before the type is known. */
static nc_type
infer_int_xtype (VALUE v)
{
  size_t bits;
  long l;
  int nlz;
  bool pow2;

  if ( FIXNUM_P(v) ) {
    l = FIX2LONG(v);
    return ( l >= INT_MIN && l <= INT_MAX ) ? NC_INT : NC_INT64;
  }
  bits = rb_absint_size(v, &nlz) * CHAR_BIT - (size_t) nlz;
  if ( RBIGNUM_NEGATIVE_P(v) ) {
    /* |v| up to 2**31 fits int, up to 2**63 long long */
    pow2 = rb_absint_singlebit_p(v);
    if ( bits <= 31 || ( bits == 32 && pow2 ) ) {
      return NC_INT;
    }
    if ( bits <= 63 || ( bits == 64 && pow2 ) ) {
      return NC_INT64;
    }
  }
  else {
    if ( bits <= 31 ) {
      return NC_INT;
    }
    if ( bits <= 63 ) {
      return NC_INT64;
    }
    if ( bits <= 64 ) {
      return NC_UINT64;
    }
  }
  rb_raise(rb_eRangeError, "integer %"PRIsVALUE" does not fit a 64-bit netCDF type", v);
}

static bool
is_int_negative (VALUE v)
{
  return FIXNUM_P(v) ? FIX2LONG(v) < 0 : RBIGNUM_NEGATIVE_P(v);
}

static VALUE
rb_nc_put_att (VALUE self, VALUE rncid, VALUE rvarid, VALUE rname, VALUE rval)
{
  VALUE relem;
  long n, i;
  bool any_float, any_negative;
  nc_type int_xt, xt;

  if ( RB_TYPE_P(rval, T_STRING) ) {
    return rb_nc_put_att_text(self, rncid, rvarid, rname, rval);
  }
  if ( RB_TYPE_P(rval, T_FIXNUM) || RB_TYPE_P(rval, T_BIGNUM) ) {
    switch ( infer_int_xtype(rval) ) {
    case NC_INT:   return rb_nc_put_att_int(self, rncid, rvarid, rname, INT2NUM(NC_INT), rval);
    case NC_INT64: return rb_nc_put_att_longlong(self, rncid, rvarid, rname, INT2NUM(NC_INT64), rval);
    default:       return rb_nc_put_att_ulonglong(self, rncid, rvarid, rname, INT2NUM(NC_UINT64), rval);
    }
  }
  if ( RB_TYPE_P(rval, T_FLOAT) ) {
    return rb_nc_put_att_double(self, rncid, rvarid, rname, INT2NUM(NC_DOUBLE), rval);
  }
  if ( RB_TYPE_P(rval, T_ARRAY) ) {
    n            = RARRAY_LEN(rval);
    any_float    = false;
    any_negative = false;
    int_xt       = NC_INT;        /* widened NC_INT -> NC_INT64 -> NC_UINT64 */
    for (i=0; i<n; i++) {
      relem = RARRAY_AREF(rval, i);
      if ( RB_TYPE_P(relem, T_FLOAT) ) {
        any_float = true;
      }
      else if ( RB_TYPE_P(relem, T_FIXNUM) || RB_TYPE_P(relem, T_BIGNUM) ) {
        xt = infer_int_xtype(relem);
        if ( xt == NC_UINT64 || ( xt == NC_INT64 && int_xt == NC_INT ) ) {
          int_xt = xt;
        }
        if ( is_int_negative(relem) ) {
          any_negative = true;
        }
      }
      else {
        rb_raise(rb_eTypeError, "unsupported element type in attribute array");
      }
    }
    if ( any_float ) {
      return rb_nc_put_att_double(self, rncid, rvarid, rname, INT2NUM(NC_DOUBLE), rval);
    }
    switch ( int_xt ) {
    case NC_INT:   return rb_nc_put_att_int(self, rncid, rvarid, rname, INT2NUM(NC_INT), rval);
    case NC_INT64: return rb_nc_put_att_longlong(self, rncid, rvarid, rname, INT2NUM(NC_INT64), rval);
    default:
      if ( any_negative ) {
        rb_raise(rb_eRangeError, "no netCDF integer type holds both a value past 2**63 - 1 "
                                 "and a negative one");
      }
      return rb_nc_put_att_ulonglong(self, rncid, rvarid, rname, INT2NUM(NC_UINT64), rval);
    }
  }
  rb_raise(rb_eTypeError, "unsupported attribute value type");
}

static VALUE
rb_nc_get_att (VALUE self, VALUE rncid, VALUE rvarid, VALUE rname)
{
  VALUE out;
  nc_type xt;
  size_t len;

  NC_CHECK(nc_inq_att(NUM2INT(rncid), NUM2INT(rvarid), StringValueCStr(rname), &xt, &len));

  if ( xt == NC_CHAR ) {
    return rb_nc_get_att_text(self, rncid, rvarid, rname);
  }
  if ( xt == NC_STRING ) {
    out = rb_nc_get_att_string(self, rncid, rvarid, rname);
    if ( len == 1 ) {
      return rb_ary_entry(out, 0);
    }
    return out;
  }

  switch ( xt ) {
  case NC_BYTE:   out = rb_nc_get_att_schar(self, rncid, rvarid, rname);     break;
  case NC_UBYTE:  out = rb_nc_get_att_uchar(self, rncid, rvarid, rname);     break;
  case NC_SHORT:  out = rb_nc_get_att_short(self, rncid, rvarid, rname);     break;
  case NC_USHORT: out = rb_nc_get_att_ushort(self, rncid, rvarid, rname);    break;
  case NC_INT:    out = rb_nc_get_att_int(self, rncid, rvarid, rname);       break;
  case NC_UINT:   out = rb_nc_get_att_uint(self, rncid, rvarid, rname);      break;
  case NC_INT64:  out = rb_nc_get_att_longlong(self, rncid, rvarid, rname);  break;
  case NC_UINT64: out = rb_nc_get_att_ulonglong(self, rncid, rvarid, rname); break;
  case NC_FLOAT:  out = rb_nc_get_att_float(self, rncid, rvarid, rname);     break;
  case NC_DOUBLE: out = rb_nc_get_att_double(self, rncid, rvarid, rname);    break;
  default:        rb_raise(rb_eNotImpError, "unsupported attribute type: %d", xt);
  }

  if ( len == 1 ) {
    return RARRAY_AREF(out, 0);
  }
  return out;
}

void
init_nc_att (void)
{
  rb_define_module_function(mNetCDF, "nc_inq_attname",       rb_nc_inq_attname,       3);
  rb_define_module_function(mNetCDF, "nc_inq_attid",         rb_nc_inq_attid,         3);
  rb_define_module_function(mNetCDF, "nc_inq_att",           rb_nc_inq_att,           3);
  rb_define_module_function(mNetCDF, "nc_inq_atttype",       rb_nc_inq_atttype,       3);
  rb_define_module_function(mNetCDF, "nc_inq_attlen",        rb_nc_inq_attlen,        3);
  rb_define_module_function(mNetCDF, "nc_rename_att",        rb_nc_rename_att,        4);
  rb_define_module_function(mNetCDF, "nc_del_att",           rb_nc_del_att,           3);
  rb_define_module_function(mNetCDF, "nc_copy_att",          rb_nc_copy_att,          5);

  rb_define_module_function(mNetCDF, "nc_put_att_text",      rb_nc_put_att_text,      4);
  rb_define_module_function(mNetCDF, "nc_get_att_text",      rb_nc_get_att_text,      3);
  rb_define_module_function(mNetCDF, "nc_put_att_schar",     rb_nc_put_att_schar,     5);
  rb_define_module_function(mNetCDF, "nc_get_att_schar",     rb_nc_get_att_schar,     3);
  rb_define_module_function(mNetCDF, "nc_put_att_uchar",     rb_nc_put_att_uchar,     5);
  rb_define_module_function(mNetCDF, "nc_get_att_uchar",     rb_nc_get_att_uchar,     3);
  rb_define_module_function(mNetCDF, "nc_put_att_short",     rb_nc_put_att_short,     5);
  rb_define_module_function(mNetCDF, "nc_get_att_short",     rb_nc_get_att_short,     3);
  rb_define_module_function(mNetCDF, "nc_put_att_ushort",    rb_nc_put_att_ushort,    5);
  rb_define_module_function(mNetCDF, "nc_get_att_ushort",    rb_nc_get_att_ushort,    3);
  rb_define_module_function(mNetCDF, "nc_put_att_int",       rb_nc_put_att_int,       5);
  rb_define_module_function(mNetCDF, "nc_get_att_int",       rb_nc_get_att_int,       3);
  rb_define_module_function(mNetCDF, "nc_put_att_uint",      rb_nc_put_att_uint,      5);
  rb_define_module_function(mNetCDF, "nc_get_att_uint",      rb_nc_get_att_uint,      3);
  rb_define_module_function(mNetCDF, "nc_put_att_longlong",  rb_nc_put_att_longlong,  5);
  rb_define_module_function(mNetCDF, "nc_get_att_longlong",  rb_nc_get_att_longlong,  3);
  rb_define_module_function(mNetCDF, "nc_put_att_ulonglong", rb_nc_put_att_ulonglong, 5);
  rb_define_module_function(mNetCDF, "nc_get_att_ulonglong", rb_nc_get_att_ulonglong, 3);
  rb_define_module_function(mNetCDF, "nc_put_att_float",     rb_nc_put_att_float,     5);
  rb_define_module_function(mNetCDF, "nc_get_att_float",     rb_nc_get_att_float,     3);
  rb_define_module_function(mNetCDF, "nc_put_att_double",    rb_nc_put_att_double,    5);
  rb_define_module_function(mNetCDF, "nc_get_att_double",    rb_nc_get_att_double,    3);
  rb_define_module_function(mNetCDF, "nc_put_att_string",    rb_nc_put_att_string,    4);
  rb_define_module_function(mNetCDF, "nc_get_att_string",    rb_nc_get_att_string,    3);

  rb_define_module_function(mNetCDF, "nc_put_att",           rb_nc_put_att,           4);
  rb_define_module_function(mNetCDF, "nc_get_att",           rb_nc_get_att,           3);
}
