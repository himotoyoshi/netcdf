#include "internal.h"

/* ---------------------------------------------------------- */
/* Scalar I/O (var1)                                           */
/* ---------------------------------------------------------- */

static VALUE
rb_nc_put_var1_text (VALUE self, VALUE rncid, VALUE rvarid, VALUE rindex, VALUE rvalue)
{
  size_t idx[NC_MAX_VAR_DIMS];
  int st;

  StringValue(rvalue);
  if ( RSTRING_LEN(rvalue) < 1 ) {
    rb_raise(rb_eArgError, "value string must have at least 1 character");
  }
  indices_to_c(NUM2INT(rncid), NUM2INT(rvarid), rindex, idx);
  st = nc_put_var1_text(NUM2INT(rncid), NUM2INT(rvarid), idx, RSTRING_PTR(rvalue));
  if ( st != NC_NOERR ) {
    nc_raise(st);
  }
  return Qnil;
}

static VALUE
rb_nc_get_var1_text (VALUE self, VALUE rncid, VALUE rvarid, VALUE rindex)
{
  size_t idx[NC_MAX_VAR_DIMS];
  char c;
  int st;

  indices_to_c(NUM2INT(rncid), NUM2INT(rvarid), rindex, idx);
  st = nc_get_var1_text(NUM2INT(rncid), NUM2INT(rvarid), idx, &c);
  if ( st != NC_NOERR ) {
    nc_raise(st);
  }
  return rb_utf8_str_new(&c, 1);
}

#define DEFINE_VAR1_NUM(suffix, ctype, num2c, c2num)                    \
  static VALUE                                                          \
  rb_nc_put_var1_##suffix (VALUE self, VALUE rncid, VALUE rvarid,       \
                           VALUE rindex, VALUE rvalue)                  \
  {                                                                     \
    size_t idx[NC_MAX_VAR_DIMS];                                        \
    ctype v;                                                            \
    int st;                                                             \
                                                                        \
    indices_to_c(NUM2INT(rncid), NUM2INT(rvarid), rindex, idx);         \
    v = (ctype) num2c(rvalue);                                          \
    st = nc_put_var1_##suffix(NUM2INT(rncid), NUM2INT(rvarid), idx, &v); \
    if ( st != NC_NOERR ) {                                             \
      nc_raise(st);                                                     \
    }                                                                   \
    return Qnil;                                                        \
  }                                                                     \
                                                                        \
  static VALUE                                                          \
  rb_nc_get_var1_##suffix (VALUE self, VALUE rncid, VALUE rvarid,       \
                           VALUE rindex)                                \
  {                                                                     \
    size_t idx[NC_MAX_VAR_DIMS];                                        \
    ctype v;                                                            \
    int st;                                                             \
                                                                        \
    indices_to_c(NUM2INT(rncid), NUM2INT(rvarid), rindex, idx);         \
    st = nc_get_var1_##suffix(NUM2INT(rncid), NUM2INT(rvarid), idx, &v); \
    if ( st != NC_NOERR ) {                                             \
      nc_raise(st);                                                     \
    }                                                                   \
    return c2num(v);                                                    \
  }

DEFINE_VAR1_NUM(schar,     signed char,        num_to_schar,     INT2NUM)
DEFINE_VAR1_NUM(uchar,     unsigned char,      num_to_uchar,     UINT2NUM)
DEFINE_VAR1_NUM(short,     short,              num_to_short,     INT2NUM)
DEFINE_VAR1_NUM(ushort,    unsigned short,     num_to_ushort,    UINT2NUM)
DEFINE_VAR1_NUM(int,       int,                NUM2INT,          INT2NUM)
DEFINE_VAR1_NUM(uint,      unsigned int,       num_to_uint,      UINT2NUM)
DEFINE_VAR1_NUM(longlong,  long long,          NUM2LL,           LL2NUM)
DEFINE_VAR1_NUM(ulonglong, unsigned long long, num_to_ulonglong, ULL2NUM)
DEFINE_VAR1_NUM(float,     float,              NUM2DBL,          DBL2NUM)
DEFINE_VAR1_NUM(double,    double,             NUM2DBL,          DBL2NUM)

static VALUE
rb_nc_put_var1_string (VALUE self, VALUE rncid, VALUE rvarid, VALUE rindex, VALUE rvalue)
{
  size_t idx[NC_MAX_VAR_DIMS];
  const char *p;
  int st;

  indices_to_c(NUM2INT(rncid), NUM2INT(rvarid), rindex, idx);
  StringValue(rvalue);
  p = StringValueCStr(rvalue);
  st = nc_put_var1_string(NUM2INT(rncid), NUM2INT(rvarid), idx, &p);
  if ( st != NC_NOERR ) {
    nc_raise(st);
  }
  return Qnil;
}

static VALUE
rb_nc_get_var1_string (VALUE self, VALUE rncid, VALUE rvarid, VALUE rindex)
{
  size_t idx[NC_MAX_VAR_DIMS];
  VALUE out;
  char *p;
  int st;

  indices_to_c(NUM2INT(rncid), NUM2INT(rvarid), rindex, idx);
  p = NULL;
  st = nc_get_var1_string(NUM2INT(rncid), NUM2INT(rvarid), idx, &p);
  if ( st != NC_NOERR ) {
    nc_raise(st);
  }
  out = rb_utf8_str_new_cstr(p ? p : "");
  nc_free_string(1, &p);
  return out;
}

/* NC_OPAQUE single-element I/O: String of opaque_size bytes per element. */
static VALUE
rb_nc_put_var1_opaque (VALUE rncid, VALUE rvarid, VALUE rindex, VALUE rvalue, size_t opaque_size)
{
  size_t idx[NC_MAX_VAR_DIMS];
  int st;

  StringValue(rvalue);
  if ( (size_t) RSTRING_LEN(rvalue) != opaque_size ) {
    rb_raise(rb_eArgError, "NC_OPAQUE element must be %zu bytes (got %ld)",
             opaque_size, RSTRING_LEN(rvalue));
  }
  indices_to_c(NUM2INT(rncid), NUM2INT(rvarid), rindex, idx);
  st = nc_put_var1(NUM2INT(rncid), NUM2INT(rvarid), idx, RSTRING_PTR(rvalue));
  if ( st != NC_NOERR ) {
    nc_raise(st);
  }
  return Qnil;
}

static VALUE
rb_nc_get_var1_opaque (VALUE rncid, VALUE rvarid, VALUE rindex, size_t opaque_size)
{
  size_t idx[NC_MAX_VAR_DIMS];
  VALUE out;
  int st;

  indices_to_c(NUM2INT(rncid), NUM2INT(rvarid), rindex, idx);
  out = rb_str_buf_new((long) opaque_size);
  rb_str_set_len(out, (long) opaque_size);
  st = nc_get_var1(NUM2INT(rncid), NUM2INT(rvarid), idx, RSTRING_PTR(out));
  if ( st != NC_NOERR ) {
    nc_raise(st);
  }
  return out;
}

/* Enum scalar I/O: use untyped nc_put_var1 / nc_get_var1 with a buffer
   sized for the enum's base nc_type. */
static VALUE
rb_nc_put_var1_enum (VALUE rncid, VALUE rvarid, VALUE rindex, VALUE rvalue, nc_type base)
{
  size_t idx[NC_MAX_VAR_DIMS];
  long long buf;
  int st;

  buf = 0;
  switch ( base ) {
  case NC_BYTE:   *(signed char *) &buf        = num_to_schar(rvalue);     break;
  case NC_UBYTE:  *(unsigned char *) &buf      = num_to_uchar(rvalue);     break;
  case NC_SHORT:  *(short *) &buf              = num_to_short(rvalue);     break;
  case NC_USHORT: *(unsigned short *) &buf     = num_to_ushort(rvalue);    break;
  case NC_INT:    *(int *) &buf                = NUM2INT(rvalue);          break;
  case NC_UINT:   *(unsigned int *) &buf       = num_to_uint(rvalue);      break;
  case NC_INT64:  *(long long *) &buf          = NUM2LL(rvalue);           break;
  case NC_UINT64: *(unsigned long long *) &buf = num_to_ulonglong(rvalue); break;
  default:
    rb_raise(rb_eArgError, "unsupported enum base type %d", base);
  }
  indices_to_c(NUM2INT(rncid), NUM2INT(rvarid), rindex, idx);
  st = nc_put_var1(NUM2INT(rncid), NUM2INT(rvarid), idx, &buf);
  if ( st != NC_NOERR ) {
    nc_raise(st);
  }
  return Qnil;
}

static VALUE
rb_nc_get_var1_enum (VALUE rncid, VALUE rvarid, VALUE rindex, nc_type base)
{
  size_t idx[NC_MAX_VAR_DIMS];
  long long buf;
  int st;

  buf = 0;
  indices_to_c(NUM2INT(rncid), NUM2INT(rvarid), rindex, idx);
  st = nc_get_var1(NUM2INT(rncid), NUM2INT(rvarid), idx, &buf);
  if ( st != NC_NOERR ) {
    nc_raise(st);
  }
  switch ( base ) {
  case NC_BYTE:   return INT2NUM(*(const signed char *) &buf);
  case NC_UBYTE:  return UINT2NUM(*(const unsigned char *) &buf);
  case NC_SHORT:  return INT2NUM(*(const short *) &buf);
  case NC_USHORT: return UINT2NUM(*(const unsigned short *) &buf);
  case NC_INT:    return INT2NUM(*(const int *) &buf);
  case NC_UINT:   return UINT2NUM(*(const unsigned int *) &buf);
  case NC_INT64:  return LL2NUM(*(const long long *) &buf);
  case NC_UINT64: return ULL2NUM(*(const unsigned long long *) &buf);
  default:
    rb_raise(rb_eArgError, "unsupported enum base type %d", base);
  }
}

/* Resolve a user-defined type's class + size + base type.  Returns -1 when
   the variable's type is a primitive, otherwise the class code. */
static int
resolve_user_type (int ncid, nc_type xt, nc_type *base_out, size_t *size_out)
{
  nc_type base;
  size_t  size;
  int     klass;

  if ( xt < NC_FIRSTUSERTYPEID ) {
    return -1;
  }
  NC_CHECK(nc_inq_user_type(ncid, xt, NULL, &size, &base, NULL, &klass));
  if ( base_out ) {
    *base_out = base;
  }
  if ( size_out ) {
    *size_out = size;
  }
  return klass;
}

/* Generic var1 — dispatches by the variable's nc_type. */
static VALUE
rb_nc_put_var1 (VALUE self, VALUE rncid, VALUE rvarid, VALUE rindex, VALUE rvalue)
{
  nc_type xt, base;
  size_t  size;
  int     klass;

  NC_CHECK(nc_inq_vartype(NUM2INT(rncid), NUM2INT(rvarid), &xt));
  /* User-defined type handling */
  if ( xt >= NC_FIRSTUSERTYPEID ) {
    klass = resolve_user_type(NUM2INT(rncid), xt, &base, &size);
    switch ( klass ) {
    case NC_OPAQUE:   return rb_nc_put_var1_opaque(rncid, rvarid, rindex, rvalue, size);
    case NC_ENUM:     return rb_nc_put_var1_enum(rncid, rvarid, rindex, rvalue, base);
    case NC_VLEN:     rb_raise(rb_eNotImpError, "NC_VLEN scalar I/O not yet supported");
    case NC_COMPOUND: rb_raise(rb_eNotImpError, "NC_COMPOUND scalar I/O not yet supported");
    default:          rb_raise(rb_eNotImpError, "unknown user-defined type class %d", klass);
    }
  }
  switch ( xt ) {
  case NC_CHAR:   return rb_nc_put_var1_text(self, rncid, rvarid, rindex, rvalue);
  case NC_BYTE:   return rb_nc_put_var1_schar(self, rncid, rvarid, rindex, rvalue);
  case NC_UBYTE:  return rb_nc_put_var1_uchar(self, rncid, rvarid, rindex, rvalue);
  case NC_SHORT:  return rb_nc_put_var1_short(self, rncid, rvarid, rindex, rvalue);
  case NC_USHORT: return rb_nc_put_var1_ushort(self, rncid, rvarid, rindex, rvalue);
  case NC_INT:    return rb_nc_put_var1_int(self, rncid, rvarid, rindex, rvalue);
  case NC_UINT:   return rb_nc_put_var1_uint(self, rncid, rvarid, rindex, rvalue);
  case NC_INT64:  return rb_nc_put_var1_longlong(self, rncid, rvarid, rindex, rvalue);
  case NC_UINT64: return rb_nc_put_var1_ulonglong(self, rncid, rvarid, rindex, rvalue);
  case NC_FLOAT:  return rb_nc_put_var1_float(self, rncid, rvarid, rindex, rvalue);
  case NC_DOUBLE: return rb_nc_put_var1_double(self, rncid, rvarid, rindex, rvalue);
  case NC_STRING: return rb_nc_put_var1_string(self, rncid, rvarid, rindex, rvalue);
  default:        rb_raise(rb_eNotImpError, "unsupported nc_type: %d", xt);
  }
}

static VALUE
rb_nc_get_var1 (VALUE self, VALUE rncid, VALUE rvarid, VALUE rindex)
{
  nc_type xt, base;
  size_t  size;
  int     klass;

  NC_CHECK(nc_inq_vartype(NUM2INT(rncid), NUM2INT(rvarid), &xt));
  if ( xt >= NC_FIRSTUSERTYPEID ) {
    klass = resolve_user_type(NUM2INT(rncid), xt, &base, &size);
    switch ( klass ) {
    case NC_OPAQUE:   return rb_nc_get_var1_opaque(rncid, rvarid, rindex, size);
    case NC_ENUM:     return rb_nc_get_var1_enum(rncid, rvarid, rindex, base);
    case NC_VLEN:     rb_raise(rb_eNotImpError, "NC_VLEN scalar I/O not yet supported");
    case NC_COMPOUND: rb_raise(rb_eNotImpError, "NC_COMPOUND scalar I/O not yet supported");
    default:          rb_raise(rb_eNotImpError, "unknown user-defined type class %d", klass);
    }
  }
  switch ( xt ) {
  case NC_CHAR:   return rb_nc_get_var1_text(self, rncid, rvarid, rindex);
  case NC_BYTE:   return rb_nc_get_var1_schar(self, rncid, rvarid, rindex);
  case NC_UBYTE:  return rb_nc_get_var1_uchar(self, rncid, rvarid, rindex);
  case NC_SHORT:  return rb_nc_get_var1_short(self, rncid, rvarid, rindex);
  case NC_USHORT: return rb_nc_get_var1_ushort(self, rncid, rvarid, rindex);
  case NC_INT:    return rb_nc_get_var1_int(self, rncid, rvarid, rindex);
  case NC_UINT:   return rb_nc_get_var1_uint(self, rncid, rvarid, rindex);
  case NC_INT64:  return rb_nc_get_var1_longlong(self, rncid, rvarid, rindex);
  case NC_UINT64: return rb_nc_get_var1_ulonglong(self, rncid, rvarid, rindex);
  case NC_FLOAT:  return rb_nc_get_var1_float(self, rncid, rvarid, rindex);
  case NC_DOUBLE: return rb_nc_get_var1_double(self, rncid, rvarid, rindex);
  case NC_STRING: return rb_nc_get_var1_string(self, rncid, rvarid, rindex);
  default:        rb_raise(rb_eNotImpError, "unsupported nc_type: %d", xt);
  }
}

/* ---------------------------------------------------------- */
/* Array I/O (vara / vars / varm)                              */
/* ---------------------------------------------------------- */

typedef enum {
  OP_PUT_GENERIC,
  OP_GET_GENERIC,
  OP_PUT_TEXT,      OP_GET_TEXT,
  OP_PUT_SCHAR,     OP_GET_SCHAR,
  OP_PUT_UCHAR,     OP_GET_UCHAR,
  OP_PUT_SHORT,     OP_GET_SHORT,
  OP_PUT_USHORT,    OP_GET_USHORT,
  OP_PUT_INT,       OP_GET_INT,
  OP_PUT_UINT,      OP_GET_UINT,
  OP_PUT_LONGLONG,  OP_GET_LONGLONG,
  OP_PUT_ULONGLONG, OP_GET_ULONGLONG,
  OP_PUT_FLOAT,     OP_GET_FLOAT,
  OP_PUT_DOUBLE,    OP_GET_DOUBLE,
} vara_op_t;

typedef struct {
  int               ncid;
  int               varid;
  const size_t     *start;
  const size_t     *count;
  const ptrdiff_t  *stride;   /* NULL -> vara, non-NULL -> vars/varm */
  const ptrdiff_t  *imap;     /* NULL -> vara/vars, non-NULL -> varm */
  void             *data;
  vara_op_t         op;
  int               status;
} vara_args_t;

#define DO_TYPED(family, suffix)                                        \
  do {                                                                  \
    if ( a->imap ) {                                                    \
      a->status = nc_##family##_varm_##suffix(a->ncid, a->varid,        \
                                              a->start, a->count,       \
                                              a->stride, a->imap,       \
                                              a->data);                 \
    }                                                                   \
    else if ( a->stride ) {                                             \
      a->status = nc_##family##_vars_##suffix(a->ncid, a->varid,        \
                                              a->start, a->count,       \
                                              a->stride, a->data);      \
    }                                                                   \
    else {                                                              \
      a->status = nc_##family##_vara_##suffix(a->ncid, a->varid,        \
                                              a->start, a->count,       \
                                              a->data);                 \
    }                                                                   \
  } while ( 0 ); break

#define DO_GENERIC(family)                                              \
  do {                                                                  \
    if ( a->imap ) {                                                    \
      a->status = nc_##family##_varm(a->ncid, a->varid,                 \
                                     a->start, a->count,                \
                                     a->stride, a->imap, a->data);      \
    }                                                                   \
    else if ( a->stride ) {                                             \
      a->status = nc_##family##_vars(a->ncid, a->varid,                 \
                                     a->start, a->count,                \
                                     a->stride, a->data);               \
    }                                                                   \
    else {                                                              \
      a->status = nc_##family##_vara(a->ncid, a->varid,                 \
                                     a->start, a->count, a->data);      \
    }                                                                   \
  } while ( 0 ); break

static void *
nogvl_io (void *p)
{
  vara_args_t *a;

  a = (vara_args_t *) p;
  switch ( a->op ) {
  case OP_PUT_GENERIC:   DO_GENERIC(put);
  case OP_GET_GENERIC:   DO_GENERIC(get);
  case OP_PUT_TEXT:      DO_TYPED(put, text);
  case OP_GET_TEXT:      DO_TYPED(get, text);
  case OP_PUT_SCHAR:     DO_TYPED(put, schar);
  case OP_GET_SCHAR:     DO_TYPED(get, schar);
  case OP_PUT_UCHAR:     DO_TYPED(put, uchar);
  case OP_GET_UCHAR:     DO_TYPED(get, uchar);
  case OP_PUT_SHORT:     DO_TYPED(put, short);
  case OP_GET_SHORT:     DO_TYPED(get, short);
  case OP_PUT_USHORT:    DO_TYPED(put, ushort);
  case OP_GET_USHORT:    DO_TYPED(get, ushort);
  case OP_PUT_INT:       DO_TYPED(put, int);
  case OP_GET_INT:       DO_TYPED(get, int);
  case OP_PUT_UINT:      DO_TYPED(put, uint);
  case OP_GET_UINT:      DO_TYPED(get, uint);
  case OP_PUT_LONGLONG:  DO_TYPED(put, longlong);
  case OP_GET_LONGLONG:  DO_TYPED(get, longlong);
  case OP_PUT_ULONGLONG: DO_TYPED(put, ulonglong);
  case OP_GET_ULONGLONG: DO_TYPED(get, ulonglong);
  case OP_PUT_FLOAT:     DO_TYPED(put, float);
  case OP_GET_FLOAT:     DO_TYPED(get, float);
  case OP_PUT_DOUBLE:    DO_TYPED(put, double);
  case OP_GET_DOUBLE:    DO_TYPED(get, double);
  }
  return NULL;
}
#undef DO_TYPED
#undef DO_GENERIC

/* ---------------------------------------------------------- */
/* NC_STRING — separate char**-based dispatch                  */
/* ---------------------------------------------------------- */

typedef struct {
  int               ncid;
  int               varid;
  const size_t     *start;
  const size_t     *count;
  const ptrdiff_t  *stride;
  const ptrdiff_t  *imap;
  char            **ptrs;
  int               status;
  bool              is_put;
} string_args_t;

static void *
nogvl_string_io (void *p)
{
  string_args_t *a;

  a = (string_args_t *) p;
  if ( a->is_put ) {
    if ( a->imap ) {
      a->status = nc_put_varm_string(a->ncid, a->varid, a->start, a->count,
                                     a->stride, a->imap, (const char **) a->ptrs);
    }
    else if ( a->stride ) {
      a->status = nc_put_vars_string(a->ncid, a->varid, a->start, a->count,
                                     a->stride, (const char **) a->ptrs);
    }
    else {
      a->status = nc_put_vara_string(a->ncid, a->varid, a->start, a->count,
                                     (const char **) a->ptrs);
    }
  }
  else {
    if ( a->imap ) {
      a->status = nc_get_varm_string(a->ncid, a->varid, a->start, a->count,
                                     a->stride, a->imap, a->ptrs);
    }
    else if ( a->stride ) {
      a->status = nc_get_vars_string(a->ncid, a->varid, a->start, a->count,
                                     a->stride, a->ptrs);
    }
    else {
      a->status = nc_get_vara_string(a->ncid, a->varid, a->start, a->count,
                                     a->ptrs);
    }
  }
  return NULL;
}

/* Recursively shape a flat Ruby Array<String> into nested form based on
   count[0..ndim-1].  `offset` is the running cursor through the flat array. */
static VALUE
build_nested (VALUE flat, size_t *offset, const size_t *count, int ndim)
{
  VALUE out;
  size_t n, i;

  n   = count[0];
  out = rb_ary_new_capa((long) n);
  if ( ndim == 1 ) {
    for (i=0; i<n; i++) {
      rb_ary_push(out, RARRAY_AREF(flat, (long) (*offset)++));
    }
  }
  else {
    for (i=0; i<n; i++) {
      rb_ary_push(out, build_nested(flat, offset, count + 1, ndim - 1));
    }
  }
  return out;
}

/* Map a single-char MemoryView format to a typed op (or -1 for unknown),
 * reporting through *width the element size the format implies.
 *
 * `ruby/memory_view.h` specifies the vocabulary: `format` is a sequence
 * of pack-template specifiers, and `item_size` should equal
 * rb_memory_view_item_size_from_format(format).  So there is one
 * spelling, not a choice of conventions -- `c`/`C` for the 1-byte
 * integers, `s`/`S` for the 2-byte ones, `i`/`I`/`q`/`Q`/`f`/`d` for
 * the rest.  It is what the producers around us publish.  (`l`/`L` are
 * in that vocabulary too, and mean 32 bits here; red-arrow spells Int32
 * that way.  We read them as Ruby does.)  So are `e`/`E` and `g`/`G`,
 * the floats that carry their own byte order -- little- and big-endian
 * single and double -- which the string parser below holds to the
 * host's.
 *
 * `b`/`B`/`h`/`H` are PEP 3118's spellings for the 1- and 2-byte
 * integers and are outside Ruby's vocabulary, where they would denote
 * bit- and hex-strings.  We accept them anyway.  CArray 3.0.0, the
 * released version, publishes them -- 3.0.1 switches to Ruby's
 * spelling, but a buffer from the version people have installed today
 * arrives spelled PEP's way -- and so does a buffer that crossed a
 * language boundary without being translated.  That is a thin
 * compatibility layer around the canonical set, not a second
 * convention: nothing binary-numeric in Ruby's vocabulary collides with
 * them, so accepting them cannot cause a misread.
 *
 * The item_size check at the call site is what makes this safe.  A
 * format that crossed from PEP-land untranslated can disagree with the
 * width it implies here -- numpy spells int64 `l` and float16 `e` --
 * and comparing against item_size catches that without having to guess
 * which vocabulary the producer meant.
 *
 * Formats Ruby has no word for and netCDF has no type for (`?`,
 * `Zf`/`Zd`, `T{...}`) fall through as unsupported. */
static int
vara_op_for_format (char fmt, bool is_put, ssize_t *width)
{
  switch ( fmt ) {
  case 'c': case 'b':           *width = 1; return is_put ? OP_PUT_SCHAR     : OP_GET_SCHAR;
  case 'C': case 'B':           *width = 1; return is_put ? OP_PUT_UCHAR     : OP_GET_UCHAR;
  case 's': case 'h':           *width = 2; return is_put ? OP_PUT_SHORT     : OP_GET_SHORT;
  case 'S': case 'H':           *width = 2; return is_put ? OP_PUT_USHORT    : OP_GET_USHORT;
  case 'i': case 'l':           *width = 4; return is_put ? OP_PUT_INT       : OP_GET_INT;
  case 'I': case 'L':           *width = 4; return is_put ? OP_PUT_UINT      : OP_GET_UINT;
  case 'q':                     *width = 8; return is_put ? OP_PUT_LONGLONG  : OP_GET_LONGLONG;
  case 'Q':                     *width = 8; return is_put ? OP_PUT_ULONGLONG : OP_GET_ULONGLONG;
  case 'f': case 'e': case 'g': *width = 4; return is_put ? OP_PUT_FLOAT     : OP_GET_FLOAT;
  case 'd': case 'E': case 'G': *width = 8; return is_put ? OP_PUT_DOUBLE    : OP_GET_DOUBLE;
  default:                      *width = 0; return -1;
  }
}

/* The netCDF type a typed op moves, for holding a buffer's format to the
 * type libnetcdf will move regardless (an enum's base type). */
static nc_type
nc_type_of_op (vara_op_t op)
{
  switch ( op ) {
  case OP_PUT_SCHAR:     case OP_GET_SCHAR:     return NC_BYTE;
  case OP_PUT_UCHAR:     case OP_GET_UCHAR:     return NC_UBYTE;
  case OP_PUT_SHORT:     case OP_GET_SHORT:     return NC_SHORT;
  case OP_PUT_USHORT:    case OP_GET_USHORT:    return NC_USHORT;
  case OP_PUT_INT:       case OP_GET_INT:       return NC_INT;
  case OP_PUT_UINT:      case OP_GET_UINT:      return NC_UINT;
  case OP_PUT_LONGLONG:  case OP_GET_LONGLONG:  return NC_INT64;
  case OP_PUT_ULONGLONG: case OP_GET_ULONGLONG: return NC_UINT64;
  case OP_PUT_FLOAT:     case OP_GET_FLOAT:     return NC_FLOAT;
  case OP_PUT_DOUBLE:    case OP_GET_DOUBLE:    return NC_DOUBLE;
  default:                                      return NC_NAT;
  }
}

/* True if a byte-order marker denotes the host's own byte order (or says
 * nothing about byte order at all).
 *
 * libnetcdf reads and writes the buffer as native-endian memory, so a
 * marker naming the opposite order has to be refused rather than
 * silently misread; byte-swapping is the producer's job. */
static bool
is_native_endian_marker (char marker)
{
  switch ( marker ) {
  case '=': case '@': case '|': case '^':
    return true;                    /* native, or not applicable */
#ifdef WORDS_BIGENDIAN
  case '>': case '!': return true;
  case '<':           return false;
#else
  case '<':           return true;
  case '>': case '!': return false;
#endif
  default:
    return false;
  }
}

/* Resolve a whole format string to a typed op, or -1 with *error_msg set.
 *
 * `format` is a sequence of specifiers, and the vocabulary lets one carry
 * a byte order.  Ruby spells it as a suffix on an integer (`q<`, `s>`)
 * and as a letter of its own on a float (`e`/`E` little-endian, `g`/`G`
 * big); a suffix on any other specifier is not Ruby's, and is refused
 * as unsupported.  PEP 3118 spells it as a prefix on the whole string
 * (`<d`), which arrives here for the same reason PEP's `b`/`B` do -- a
 * buffer that crossed a language boundary untranslated.  All of these
 * are read, and one naming the foreign order is refused: without this
 * the order is simply not looked at, and a byte-swapped buffer
 * transfers as if it were native.
 *
 * Only a single numeric element is accepted.  Repeat counts, records
 * (`T{...}`) and mixed formats have no netCDF counterpart, and reading
 * just the first specifier of one would transfer the buffer as an array
 * of whatever it happens to start with. */
static int
vara_op_for_format_string (const char *format, bool is_put, ssize_t *width,
                           VALUE *error_msg)
{
  const char *p;
  char spec;
  int op;

  p = format;
  if ( *p == '<' || *p == '>' || *p == '=' || *p == '!' ||
       *p == '@' || *p == '|' || *p == '^' ) {
    if ( ! is_native_endian_marker(*p) ) {
      goto not_native;
    }
    p++;
  }

  spec = *p;
  op = vara_op_for_format(spec, is_put, width);
  if ( op < 0 ) {
    *error_msg = rb_sprintf("unsupported buffer format: %s", format);
    return -1;
  }
  p++;

  if ( spec == 'e' || spec == 'E' || spec == 'g' || spec == 'G' ) {
    if ( ! is_native_endian_marker(( spec == 'e' || spec == 'E' ) ? '<' : '>') ) {
      goto not_native;
    }
  }
  else if ( *p == '<' || *p == '>' ) {
    if ( strchr("sSiIlLqQ", spec) == NULL ) {
      *error_msg = rb_sprintf("unsupported buffer format: %s", format);
      return -1;
    }
    if ( ! is_native_endian_marker(*p) ) {
      goto not_native;
    }
    p++;
  }

  if ( *p != '\0' ) {
    *error_msg = rb_sprintf("buffer format \"%s\" does not describe a "
                            "single numeric element", format);
    return -1;
  }

  return op;

 not_native:
  *error_msg = rb_sprintf("buffer format \"%s\" is not native-endian", format);
  return -1;
}

/* ---------------------------------------------------------- */
/* Core dispatcher                                             */
/* ---------------------------------------------------------- */

/* Sizes a transfer of `count`: *items is the number of elements moved
 * (the product of count) and *extent the number the buffer must span.
 * Without an imap the two are equal.  With one, libnetcdf addresses
 * element sum(index[i] * imap[i]) of the buffer, so it must reach the
 * largest such offset -- which a gapped imap puts well past the product.
 *
 * Returns NULL, or why the transfer cannot be sized: a negative imap
 * entry would address memory before the buffer, and a size that
 * overflows ssize_t would wrap to one any buffer passes -- count is not
 * bounded by libnetcdf on a put along an unlimited dimension. */
static const char *
transfer_size (int ndims, const size_t *count, const ptrdiff_t *imap,
               ssize_t *items_out, ssize_t *extent_out)
{
  ssize_t items, last, term;
  int i;

  if ( imap ) {
    for (i=0; i<ndims; i++) {
      if ( imap[i] < 0 ) {
        return "imap entries must not be negative";
      }
    }
  }
  for (i=0; i<ndims; i++) {
    if ( count[i] == 0 ) {
      *items_out = *extent_out = 0;
      return NULL;
    }
  }

  items = 1;
  for (i=0; i<ndims; i++) {
    if ( count[i] > (size_t) SSIZE_MAX ||
         __builtin_mul_overflow(items, (ssize_t) count[i], &items) ) {
      return "count is too large";
    }
  }
  *items_out = items;
  if ( imap == NULL ) {
    *extent_out = items;
    return NULL;
  }

  last = 0;
  for (i=0; i<ndims; i++) {
    if ( __builtin_mul_overflow((ssize_t) (count[i] - 1), (ssize_t) imap[i], &term) ||
         __builtin_add_overflow(last, term, &last) ) {
      return "imap reaches too far";
    }
  }
  if ( __builtin_add_overflow(last, (ssize_t) 1, extent_out) ) {
    return "imap reaches too far";
  }
  return NULL;
}

typedef struct {
  void *(*fn)(void *);
  void  *arg;
  bool   ran;
} nogvl_call_t;

static void *
nogvl_call (void *p)
{
  nogvl_call_t *c;

  c = (nogvl_call_t *) p;
  c->ran = true;
  return c->fn(c->arg);
}

/* Runs fn(arg) without the GVL and returns whether it ran.
 *
 * rb_thread_call_without_gvl checks for interrupts as it returns and
 * raises right there -- a Ctrl-C, or a Thread#raise, that arrived during
 * the call -- which would skip var_io_dispatch's cleanup: a MemoryView
 * left unreleased stays in Ruby's table of exported objects, which marks
 * it, so its buffer is never collected; strings libnetcdf allocated
 * leak.  The _gvl2 variant leaves a pending interrupt pending, for the
 * caller to raise with rb_thread_check_ints() once it has cleaned up.
 * It also does not call fn at all when an interrupt is already pending,
 * and the caller then has to raise it or try again. */
static bool
call_without_gvl (void *(*fn)(void *), void *arg)
{
  nogvl_call_t c;

  c.fn  = fn;
  c.arg = arg;
  c.ran = false;
  rb_thread_call_without_gvl2(nogvl_call, &c, RUBY_UBF_IO, NULL);
  return c.ran;
}

static VALUE
var_io_dispatch (VALUE rncid, VALUE rvarid,
                 VALUE rstart, VALUE rcount,
                 VALUE rstride, VALUE rimap,
                 VALUE rbuf, bool is_put)
{
  size_t            start[NC_MAX_VAR_DIMS], count[NC_MAX_VAR_DIMS];
  ptrdiff_t         stride_buf[NC_MAX_VAR_DIMS], imap_buf[NC_MAX_VAR_DIMS];
  const ptrdiff_t  *stride, *imap;
  int               ncid, varid, ndims;
  int               saved_status;
  VALUE             error_class, error_msg;
  rb_memory_view_t  view;
  bool              view_held;
  char            **str_ptrs;
  VALUE             vstr_ptrs;      /* ALLOCV holder for str_ptrs */
  VALUE             vtext;          /* ALLOCV holder for their text on a put */
  size_t            str_to_free;    /* strings libnetcdf allocated on a get */
  VALUE             str_result;
  bool              ran;            /* false if an interrupt kept libnetcdf from running */
  vara_args_t       args;
  string_args_t     sa;
  nc_type           var_xtype, base;
  int               user_class, st, flags, fmt_op;
  size_t            user_size, n_elem, text_len, len, offset, i;
  bool              force_generic;  /* set for enum to bypass typed dispatch */
  bool              contiguous;
  ssize_t           expected_items, extent_items, fmt_width;
  ssize_t           effective_item_size, expected_bytes;
  const char       *why;
  char             *text;
  VALUE             flat, rstr;

  ncid  = NUM2INT(rncid);
  varid = NUM2INT(rvarid);

  NC_CHECK(nc_inq_varndims(ncid, varid, &ndims));
  ary_to_sizet(rstart, ndims, "start", start);
  ary_to_sizet(rcount, ndims, "count", count);
  stride = ary_to_ptrdiff(rstride, ndims, "stride", stride_buf) ? stride_buf : NULL;
  imap   = ary_to_ptrdiff(rimap,   ndims, "imap",   imap_buf)   ? imap_buf   : NULL;

  saved_status = NC_NOERR;
  error_class  = Qnil;
  error_msg    = Qnil;
  /* Zero-initialize the view to guard against MemoryView providers
     that forget to set `view.item_desc.components` /
     `view.item_desc.length`.  rb_memory_view_release unconditionally
     xfree()s `components`, so an uninitialized garbage pointer
     there aborts the process — and the crash looks like it comes
     from this gem, not from the producer.  red-arrow <= 24.0.0 is a
     confirmed case (primitive_array_get and buffer_get in
     ext/arrow/memory-view.cpp); it is fixed from 25.0.0, which
     memsets the view itself.  We cannot rely on users having
     upgraded, and the same class of bug can recur in any future
     producer, so this stays permanently as cheap defensive
     insurance. */
  memset(&view, 0, sizeof(view));
  view_held    = false;
  str_ptrs     = NULL;
  vstr_ptrs    = 0;
  vtext        = 0;
  str_to_free  = 0;
  str_result   = Qnil;
  ran          = true;

  args.ncid   = ncid;
  args.varid  = varid;
  args.start  = start;
  args.count  = count;
  args.stride = stride;
  args.imap   = imap;
  args.data   = NULL;
  args.op     = OP_PUT_GENERIC;
  args.status = NC_NOERR;

  st = nc_inq_vartype(ncid, varid, &var_xtype);
  if ( st != NC_NOERR ) {
    saved_status = st;
    goto cleanup;
  }

  /* For user-defined types, determine the class so we can route opaque/
     enum/vlen correctly.  Enum I/O delegates to the base type; opaque is
     handled as a fixed-size byte buffer via the generic path; vlen is
     still unsupported in this dispatcher. */
  user_class    = 0;
  user_size     = 0;
  force_generic = false;
  if ( var_xtype >= NC_FIRSTUSERTYPEID ) {
    st = nc_inq_user_type(ncid, var_xtype, NULL, &user_size, &base,
                          NULL, &user_class);
    if ( st != NC_NOERR ) {
      saved_status = st;
      goto cleanup;
    }
    if ( user_class == NC_ENUM ) {
      /* Enum vars: bytewise the same as their base type, but libnetcdf
         rejects the typed nc_*_vara_int family for them.  Substitute
         the base type so byte-size logic uses the right element width,
         and force the untyped nc_*_vara path. */
      var_xtype     = base;
      user_class    = 0;
      force_generic = true;
    }
    else if ( user_class == NC_COMPOUND ) {
      error_class = rb_eNotImpError;
      error_msg   = rb_str_new_cstr("NC_COMPOUND variables are not supported");
      goto cleanup;
    }
    else if ( user_class == NC_VLEN ) {
      error_class = rb_eNotImpError;
      error_msg   = rb_str_new_cstr("NC_VLEN variables are not supported");
      goto cleanup;
    }
    /* NC_OPAQUE falls through; size is in user_size. */
  }

  why = transfer_size(ndims, count, imap, &expected_items, &extent_items);
  if ( why ) {
    error_class = rb_eArgError;
    error_msg   = rb_str_new_cstr(why);
    goto cleanup;
  }

  /* Path 0: NC_STRING — char**, separate from MemoryView. */
  if ( var_xtype == NC_STRING ) {
    /* The Ruby Array handed in or out is not laid out in memory, so
       there is nothing for an imap to map onto. */
    if ( imap ) {
      error_class = rb_eNotImpError;
      error_msg   = rb_str_new_cstr("imap is not supported for NC_STRING variables");
      goto cleanup;
    }
    n_elem = (size_t) expected_items;

    sa.ncid   = ncid;
    sa.varid  = varid;
    sa.start  = start;
    sa.count  = count;
    sa.stride = stride;
    sa.imap   = imap;
    sa.status = NC_NOERR;

    if ( is_put ) {
      if ( RB_TYPE_P(rbuf, T_STRING) ) {
        if ( n_elem != 1 ) {
          error_class = rb_eArgError;
          error_msg   = rb_sprintf("NC_STRING: single String given but %zu strings expected", n_elem);
          goto cleanup;
        }
        flat = rb_ary_new_from_args(1, rbuf);
      }
      else if ( RB_TYPE_P(rbuf, T_ARRAY) ) {
        flat = rb_funcall(rbuf, rb_intern("flatten"), 0);
      }
      else {
        error_class = rb_eTypeError;
        error_msg   = rb_str_new_cstr("NC_STRING buffer must be a String or Array of Strings");
        goto cleanup;
      }
      if ( (size_t) RARRAY_LEN(flat) != n_elem ) {
        error_class = rb_eArgError;
        error_msg   = rb_sprintf("NC_STRING: expected %zu strings, got %ld after flatten",
                                 n_elem, RARRAY_LEN(flat));
        goto cleanup;
      }
      /* Copy the text out of the Strings before letting go of the
         GVL.  Pointers into them would be left to another thread
         that can modify a String -- reallocating its bytes -- or to
         a GC compaction that moves an embedded one. */
      text_len = 0;
      for (i=0; i<n_elem; i++) {
        rstr = RARRAY_AREF(flat, (long) i);
        if ( ! RB_TYPE_P(rstr, T_STRING) ) {
          error_class = rb_eTypeError;
          error_msg   = rb_str_new_cstr("NC_STRING elements must all be Strings");
          goto cleanup;
        }
        StringValueCStr(rstr);     /* raises on an embedded NUL */
        text_len += (size_t) RSTRING_LEN(rstr) + 1;
      }
      str_ptrs = ALLOCV_N(char *, vstr_ptrs, n_elem > 0 ? n_elem : 1);
      text     = ALLOCV_N(char, vtext, text_len > 0 ? text_len : 1);
      for (i=0; i<n_elem; i++) {
        rstr = RARRAY_AREF(flat, (long) i);
        len  = (size_t) RSTRING_LEN(rstr);
        memcpy(text, RSTRING_PTR(rstr), len);
        text[len]   = '\0';
        str_ptrs[i] = text;
        text += len + 1;
      }
      sa.ptrs   = str_ptrs;
      sa.is_put = true;
      ran = call_without_gvl(nogvl_string_io, &sa);
      saved_status = sa.status;
      str_result   = rbuf;
    }
    else {
      /* The strings come back as new Ruby objects; a buffer passed
         for them would be left untouched. */
      if ( ! NIL_P(rbuf) ) {
        error_class = rb_eArgError;
        error_msg   = rb_str_new_cstr("NC_STRING variables take no buffer on a get");
        goto cleanup;
      }
      str_ptrs = ALLOCV_N(char *, vstr_ptrs, n_elem > 0 ? n_elem : 1);
      for (i=0; i<n_elem; i++) {
        str_ptrs[i] = NULL;
      }
      sa.ptrs   = str_ptrs;
      sa.is_put = false;
      ran = call_without_gvl(nogvl_string_io, &sa);
      if ( ran ) {
        str_to_free = n_elem;
      }
      saved_status = sa.status;
      if ( ran && saved_status == NC_NOERR ) {
        flat = rb_ary_new_capa((long) n_elem);
        for (i=0; i<n_elem; i++) {
          rb_ary_push(flat, rb_utf8_str_new_cstr(str_ptrs[i] ? str_ptrs[i] : ""));
        }

        if ( ndims == 0 ) {
          str_result = rb_ary_entry(flat, 0);
        }
        else if ( ndims == 1 ) {
          str_result = flat;
        }
        else {
          offset = 0;
          str_result = build_nested(flat, &offset, count, ndims);
        }
      }
    }
    goto cleanup;
  }

  /* Only NC_STRING, above, is read into an object this gem makes; the
     rest go into memory the caller provides, on a get as on a put. */
  if ( NIL_P(rbuf) ) {
    error_class = rb_eArgError;
    error_msg   = rb_str_new_cstr(is_put
                                  ? "buffer is required for put operations"
                                  : "buffer is required for get operations (except on NC_STRING variables)");
    goto cleanup;
  }

  /* Path B: MemoryView provider buffer.  Ask for what is read below: the
     format, to pick the typed call, and the strides, to see whether the
     memory is row-major.  Taking strides is the weaker request -- a
     producer with contiguous memory can always answer it -- and a
     strided one, such as a transposed numpy array, then hands over its
     view to be refused below as not row-major, where it would refuse a
     request without strides and leave the reason unknown. */
  flags = RUBY_MEMORY_VIEW_FORMAT | RUBY_MEMORY_VIEW_STRIDES;
  if ( ! is_put ) {
    flags |= RUBY_MEMORY_VIEW_WRITABLE;
  }
  if ( ! rb_memory_view_get(rbuf, &view, flags) ) {
    error_class = rb_eTypeError;
    error_msg   = rb_sprintf("buffer must provide a%s MemoryView",
                             is_put ? "" : " writable");
    goto cleanup;
  }
  view_held = true;

  /* Check contiguity for any rank, including 1-D.  A producer may hand us
     a strided view (a slice whose stride != item_size), so consumer-side
     verification is necessary to prevent silent data corruption.
     NULL strides mean contiguous by definition (a byte-array view from
     rb_memory_view_init_as_byte_array, such as Fiddle::Pointer's, has
     NULL shape and strides).  ruby/memory_view.h lets a 1-D view leave
     shape NULL and still give strides, so that case is checked by hand
     -- rb_memory_view_is_row_major_contiguous would dereference the
     shape -- and a view of more dimensions without a shape cannot be
     laid out at all. */
  if ( view.ndim >= 1 && view.strides != NULL ) {
    contiguous = ( view.shape != NULL )
                 ? rb_memory_view_is_row_major_contiguous(&view)
                 : ( view.ndim == 1 && view.strides[0] == view.item_size );
    if ( ! contiguous ) {
      error_class = rb_eArgError;
      error_msg   = rb_str_new_cstr("buffer must be row-major (C-contiguous)");
      goto cleanup;
    }
  }

  /* rb_memory_view_get passes RUBY_MEMORY_VIEW_WRITABLE to the producer
     but does not hold it to the request, so a producer that ignores it
     hands back a read-only view.  libnetcdf would write into it. */
  if ( ! is_put && view.readonly ) {
    error_class = rb_eTypeError;
    error_msg   = rb_str_new_cstr("buffer is read-only (its producer ignored the request "
                                  "for a writable MemoryView)");
    goto cleanup;
  }

  /* A buffer that names its element type is held to it: the format must
     be one we read, and agree with item_size.  Opaque data has no
     element type for a format to name, so an opaque buffer's is not
     looked at. */
  fmt_op = -1;
  if ( view.format != NULL && user_class != NC_OPAQUE ) {
    fmt_width = 0;
    fmt_op = vara_op_for_format_string(view.format, is_put, &fmt_width, &error_msg);
    if ( fmt_op < 0 ) {
      error_class = rb_eArgError;
      goto cleanup;
    }
    /* The format tells us how to interpret the elements; item_size
       tells us how wide they actually are.  If the two disagree the
       producer means something we are not reading -- PEP's 64-bit
       `l`, say, against Ruby's 32-bit one -- so refuse rather than
       misread the buffer. */
    if ( view.item_size != fmt_width ) {
      error_class = rb_eArgError;
      error_msg   = rb_sprintf("buffer format \"%s\" implies %"PRIdSIZE"-byte elements "
                               "but item_size is %"PRIdSIZE,
                               view.format, fmt_width, view.item_size);
      goto cleanup;
    }
  }

  /* The per-element byte size libnetcdf will actually move.  Opaque,
     enum and format-less buffers go through the untyped call, which
     transfers elements of the variable's own type (the opaque's declared
     size, the enum's base type) regardless of how the buffer chose to
     break itself up.  Only a formatted buffer on the typed path is
     transferred at its own item_size. */
  if ( user_class == NC_OPAQUE ) {
    effective_item_size = (ssize_t) user_size;
  }
  else if ( force_generic || view.format == NULL ) {
    effective_item_size = nc_xtype_size(var_xtype);
  }
  else {
    effective_item_size = view.item_size;
  }
  if ( __builtin_mul_overflow(extent_items, effective_item_size, &expected_bytes) ) {
    error_class = rb_eArgError;
    error_msg   = rb_str_new_cstr("count is too large");
    goto cleanup;
  }
  if ( view.byte_size < expected_bytes ) {
    error_class = rb_eArgError;
    error_msg   = rb_sprintf("buffer too small (have %"PRIdSIZE" bytes, need %"PRIdSIZE")",
                             view.byte_size, expected_bytes);
    goto cleanup;
  }

  if ( user_class == NC_OPAQUE ) {
    /* Opaque: pass the bytes straight through, libnetcdf knows the
       element size. */
    args.op = is_put ? OP_PUT_GENERIC : OP_GET_GENERIC;
  }
  else if ( force_generic ) {
    /* Enum: libnetcdf moves elements of the base type whatever the
       buffer holds, so a format naming another type would be
       reinterpreted, not converted. */
    if ( fmt_op >= 0 && nc_type_of_op((vara_op_t) fmt_op) != var_xtype ) {
      error_class = rb_eArgError;
      error_msg   = rb_sprintf("buffer format \"%s\" does not match the enum's base type",
                               view.format);
      goto cleanup;
    }
    args.op = is_put ? OP_PUT_GENERIC : OP_GET_GENERIC;
  }
  else if ( var_xtype == NC_CHAR ) {
    if ( view.item_size != 1 ) {
      error_class = rb_eArgError;
      error_msg   = rb_sprintf("NC_CHAR variable requires a 1-byte-element buffer (item_size=%"PRIdSIZE")",
                               view.item_size);
      goto cleanup;
    }
    args.op = is_put ? OP_PUT_TEXT : OP_GET_TEXT;
  }
  else if ( fmt_op < 0 ) {
    args.op = is_put ? OP_PUT_GENERIC : OP_GET_GENERIC;
  }
  else {
    args.op = (vara_op_t) fmt_op;
  }
  args.data = view.data;
  ran = call_without_gvl(nogvl_io, &args);
  saved_status = args.status;

 cleanup:
  /* A producer whose release_func refuses keeps its buffer exported --
     pinned for good -- so say so rather than return as if released. */
  if ( view_held && ! rb_memory_view_release(&view) && NIL_P(error_msg) ) {
    error_class = rb_eRuntimeError;
    error_msg   = rb_str_new_cstr("buffer's producer refused to release its MemoryView");
  }
  if ( str_to_free ) {
    nc_free_string(str_to_free, str_ptrs);
  }
  if ( str_ptrs ) {
    ALLOCV_END(vstr_ptrs);
  }
  if ( vtext ) {
    ALLOCV_END(vtext);
  }

  /* With nothing held any more, raise an interrupt that arrived while
     libnetcdf ran, or kept it from running.  One that raises nothing
     (a trap handler, a timer) leaves the transfer to be done over. */
  rb_thread_check_ints();
  if ( ! ran ) {
    return var_io_dispatch(rncid, rvarid, rstart, rcount,
                           rstride, rimap, rbuf, is_put);
  }

  if ( ! NIL_P(error_msg) ) {
    rb_raise(NIL_P(error_class) ? rb_eRuntimeError : error_class,
             "%s", StringValueCStr(error_msg));
  }
  if ( saved_status != NC_NOERR ) {
    nc_raise(saved_status);
  }
  return NIL_P(str_result) ? rbuf : str_result;
}

static VALUE
rb_nc_put_vara (VALUE self, VALUE rncid, VALUE rvarid, VALUE rstart, VALUE rcount, VALUE rbuf)
{
  return var_io_dispatch(rncid, rvarid, rstart, rcount, Qnil, Qnil, rbuf, true);
}

static VALUE
rb_nc_get_vara (int argc, VALUE *argv, VALUE self)
{
  VALUE rncid, rvarid, rstart, rcount, rbuf;

  rb_scan_args(argc, argv, "41", &rncid, &rvarid, &rstart, &rcount, &rbuf);
  return var_io_dispatch(rncid, rvarid, rstart, rcount, Qnil, Qnil, rbuf, false);
}

static VALUE
rb_nc_put_vars (VALUE self, VALUE rncid, VALUE rvarid, VALUE rstart, VALUE rcount,
                VALUE rstride, VALUE rbuf)
{
  return var_io_dispatch(rncid, rvarid, rstart, rcount, rstride, Qnil, rbuf, true);
}

static VALUE
rb_nc_get_vars (int argc, VALUE *argv, VALUE self)
{
  VALUE rncid, rvarid, rstart, rcount, rstride, rbuf;

  rb_scan_args(argc, argv, "51", &rncid, &rvarid, &rstart, &rcount, &rstride, &rbuf);
  return var_io_dispatch(rncid, rvarid, rstart, rcount, rstride, Qnil, rbuf, false);
}

static VALUE
rb_nc_put_varm (VALUE self, VALUE rncid, VALUE rvarid, VALUE rstart, VALUE rcount,
                VALUE rstride, VALUE rimap, VALUE rbuf)
{
  return var_io_dispatch(rncid, rvarid, rstart, rcount, rstride, rimap, rbuf, true);
}

static VALUE
rb_nc_get_varm (int argc, VALUE *argv, VALUE self)
{
  VALUE rncid, rvarid, rstart, rcount, rstride, rimap, rbuf;

  rb_scan_args(argc, argv, "61", &rncid, &rvarid, &rstart, &rcount, &rstride, &rimap, &rbuf);
  return var_io_dispatch(rncid, rvarid, rstart, rcount, rstride, rimap, rbuf, false);
}

/* Whole-variable I/O: builds start = [0...] and count = [dimlens...] for the
   variable, then defers to the array dispatcher. */
static VALUE
rb_nc_var_io (VALUE rncid, VALUE rvarid, VALUE rbuf, bool is_put)
{
  int dimids[NC_MAX_VAR_DIMS];
  VALUE rstart, rcount;
  size_t len;
  int ncid, varid, ndims, i;

  ncid  = NUM2INT(rncid);
  varid = NUM2INT(rvarid);

  NC_CHECK(nc_inq_varndims(ncid, varid, &ndims));
  NC_CHECK(nc_inq_vardimid(ncid, varid, dimids));

  rstart = rb_ary_new_capa(ndims);
  rcount = rb_ary_new_capa(ndims);
  for (i=0; i<ndims; i++) {
    NC_CHECK(nc_inq_dimlen(ncid, dimids[i], &len));
    rb_ary_push(rstart, INT2FIX(0));
    rb_ary_push(rcount, SIZET2NUM(len));
  }
  return var_io_dispatch(rncid, rvarid, rstart, rcount, Qnil, Qnil, rbuf, is_put);
}

static VALUE
rb_nc_put_var (VALUE self, VALUE rncid, VALUE rvarid, VALUE rbuf)
{
  return rb_nc_var_io(rncid, rvarid, rbuf, true);
}

static VALUE
rb_nc_get_var (int argc, VALUE *argv, VALUE self)
{
  VALUE rncid, rvarid, rbuf;

  rb_scan_args(argc, argv, "21", &rncid, &rvarid, &rbuf);
  return rb_nc_var_io(rncid, rvarid, rbuf, false);
}

/* ---------------------------------------------------------- */
/* Whole-variable copy between files                           */
/* ---------------------------------------------------------- */

typedef struct {
  int ncid_in, varid, ncid_out;
  int status;
} copy_var_args_t;

static void *
nogvl_copy_var (void *p)
{
  copy_var_args_t *a;

  a = (copy_var_args_t *) p;
  a->status = nc_copy_var(a->ncid_in, a->varid, a->ncid_out);
  return NULL;
}

/* Copies the definition, the attributes and the data.  Nothing is held
   across the call, so an interrupt that kept it from running is raised,
   or the copy tried again, with nothing to clean up first. */
static VALUE
rb_nc_copy_var (VALUE self, VALUE rncid_in, VALUE rvarid, VALUE rncid_out)
{
  copy_var_args_t a;

  a.ncid_in  = NUM2INT(rncid_in);
  a.varid    = NUM2INT(rvarid);
  a.ncid_out = NUM2INT(rncid_out);
  a.status   = NC_NOERR;
  while ( ! call_without_gvl(nogvl_copy_var, &a) ) {
    rb_thread_check_ints();
  }
  rb_thread_check_ints();
  NC_CHECK(a.status);
  return Qnil;
}

void
init_nc_io (void)
{
  rb_define_module_function(mNetCDF, "nc_put_var1", rb_nc_put_var1,  4);
  rb_define_module_function(mNetCDF, "nc_get_var1", rb_nc_get_var1,  3);

  rb_define_module_function(mNetCDF, "nc_put_var",  rb_nc_put_var,   3);
  rb_define_module_function(mNetCDF, "nc_get_var",  rb_nc_get_var,  -1);
  rb_define_module_function(mNetCDF, "nc_put_vara", rb_nc_put_vara,  5);
  rb_define_module_function(mNetCDF, "nc_get_vara", rb_nc_get_vara, -1);
  rb_define_module_function(mNetCDF, "nc_put_vars", rb_nc_put_vars,  6);
  rb_define_module_function(mNetCDF, "nc_get_vars", rb_nc_get_vars, -1);
  rb_define_module_function(mNetCDF, "nc_put_varm", rb_nc_put_varm,  7);
  rb_define_module_function(mNetCDF, "nc_get_varm", rb_nc_get_varm, -1);

  rb_define_module_function(mNetCDF, "nc_copy_var", rb_nc_copy_var,  3);
}
