#include "internal.h"

static VALUE
rb_nc_def_dim (VALUE self, VALUE rncid, VALUE rname, VALUE rlen)
{
  int dimid;
  size_t len;

  len = NUM2SIZET(rlen);
  NC_CHECK(nc_def_dim(NUM2INT(rncid), StringValueCStr(rname), len, &dimid));
  return INT2NUM(dimid);
}

/* returns dimid, or nil when the name does not exist */
static VALUE
rb_nc_inq_dimid (VALUE self, VALUE rncid, VALUE rname)
{
  int dimid, st;

  st = nc_inq_dimid(NUM2INT(rncid), StringValueCStr(rname), &dimid);
  if ( st == NC_EBADDIM ) {
    return Qnil;
  }
  if ( st != NC_NOERR ) {
    nc_raise(st);
  }
  return INT2NUM(dimid);
}

static VALUE
rb_nc_inq_dim (VALUE self, VALUE rncid, VALUE rdimid)
{
  char name[NC_MAX_NAME + 1];
  size_t len;

  NC_CHECK(nc_inq_dim(NUM2INT(rncid), NUM2INT(rdimid), name, &len));
  return rb_ary_new_from_args(2, rb_utf8_str_new_cstr(name), SIZET2NUM(len));
}

static VALUE
rb_nc_inq_dimname (VALUE self, VALUE rncid, VALUE rdimid)
{
  char name[NC_MAX_NAME + 1];

  NC_CHECK(nc_inq_dimname(NUM2INT(rncid), NUM2INT(rdimid), name));
  return rb_utf8_str_new_cstr(name);
}

static VALUE
rb_nc_inq_dimlen (VALUE self, VALUE rncid, VALUE rdimid)
{
  size_t len;

  NC_CHECK(nc_inq_dimlen(NUM2INT(rncid), NUM2INT(rdimid), &len));
  return SIZET2NUM(len);
}

static VALUE
rb_nc_rename_dim (VALUE self, VALUE rncid, VALUE rdimid, VALUE rname)
{
  NC_CHECK(nc_rename_dim(NUM2INT(rncid), NUM2INT(rdimid), StringValueCStr(rname)));
  return Qnil;
}

void
init_nc_dim (void)
{
  rb_define_module_function(mNetCDF, "nc_def_dim",     rb_nc_def_dim,     3);
  rb_define_module_function(mNetCDF, "nc_inq_dimid",   rb_nc_inq_dimid,   2);
  rb_define_module_function(mNetCDF, "nc_inq_dim",     rb_nc_inq_dim,     2);
  rb_define_module_function(mNetCDF, "nc_inq_dimname", rb_nc_inq_dimname, 2);
  rb_define_module_function(mNetCDF, "nc_inq_dimlen",  rb_nc_inq_dimlen,  2);
  rb_define_module_function(mNetCDF, "nc_rename_dim",  rb_nc_rename_dim,  3);
}
