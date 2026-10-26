#include "internal.h"

static VALUE
rb_nc_def_grp (VALUE self, VALUE rparent, VALUE rname)
{
  int grpid;

  NC_CHECK(nc_def_grp(NUM2INT(rparent), StringValueCStr(rname), &grpid));
  return INT2NUM(grpid);
}

static VALUE
rb_nc_rename_grp (VALUE self, VALUE rgrpid, VALUE rname)
{
  NC_CHECK(nc_rename_grp(NUM2INT(rgrpid), StringValueCStr(rname)));
  return Qnil;
}

static VALUE
rb_nc_inq_grps (VALUE self, VALUE rncid)
{
  VALUE out;
  int n, st, i;
  int *ids;

  NC_CHECK(nc_inq_grps(NUM2INT(rncid), &n, NULL));
  ids = ALLOC_N(int, n > 0 ? n : 1);
  st = nc_inq_grps(NUM2INT(rncid), NULL, ids);
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

static VALUE
rb_nc_inq_grpname (VALUE self, VALUE rncid)
{
  char name[NC_MAX_NAME + 1];

  NC_CHECK(nc_inq_grpname(NUM2INT(rncid), name));
  return rb_utf8_str_new_cstr(name);
}

static VALUE
rb_nc_inq_grpname_full (VALUE self, VALUE rncid)
{
  VALUE out;
  size_t len;

  NC_CHECK(nc_inq_grpname_len(NUM2INT(rncid), &len));
  out = rb_str_buf_new((long) len);
  rb_str_set_len(out, (long) len);
  rb_enc_associate(out, rb_utf8_encoding());
  NC_CHECK(nc_inq_grpname_full(NUM2INT(rncid), NULL, RSTRING_PTR(out)));
  return out;
}

/* Returns parent ncid, or nil for the root group. */
static VALUE
rb_nc_inq_grp_parent (VALUE self, VALUE rncid)
{
  int parent, st;

  st = nc_inq_grp_parent(NUM2INT(rncid), &parent);
  if ( st == NC_ENOGRP ) {
    return Qnil;
  }
  if ( st != NC_NOERR ) {
    nc_raise(st);
  }
  return INT2NUM(parent);
}

static VALUE
rb_nc_inq_ncid (VALUE self, VALUE rncid, VALUE rname)
{
  int grpid, st;

  st = nc_inq_ncid(NUM2INT(rncid), StringValueCStr(rname), &grpid);
  if ( st == NC_ENOGRP ) {
    return Qnil;
  }
  if ( st != NC_NOERR ) {
    nc_raise(st);
  }
  return INT2NUM(grpid);
}

static VALUE
rb_nc_inq_grp_full_ncid (VALUE self, VALUE rncid, VALUE rfull_name)
{
  int grpid, st;

  st = nc_inq_grp_full_ncid(NUM2INT(rncid), StringValueCStr(rfull_name), &grpid);
  if ( st == NC_ENOGRP ) {
    return Qnil;
  }
  if ( st != NC_NOERR ) {
    nc_raise(st);
  }
  return INT2NUM(grpid);
}

static VALUE
rb_nc_inq_dimids (int argc, VALUE *argv, VALUE self)
{
  VALUE rncid, rinclude_parents, out;
  int ncid, include_parents, n, st, i;
  int *ids;

  rb_scan_args(argc, argv, "11", &rncid, &rinclude_parents);
  ncid            = NUM2INT(rncid);
  include_parents = RTEST(rinclude_parents) ? 1 : 0;

  NC_CHECK(nc_inq_dimids(ncid, &n, NULL, include_parents));
  ids = ALLOC_N(int, n > 0 ? n : 1);
  st = nc_inq_dimids(ncid, NULL, ids, include_parents);
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

static VALUE
rb_nc_inq_varids (VALUE self, VALUE rncid)
{
  VALUE out;
  int n, st, i;
  int *ids;

  NC_CHECK(nc_inq_varids(NUM2INT(rncid), &n, NULL));
  ids = ALLOC_N(int, n > 0 ? n : 1);
  st = nc_inq_varids(NUM2INT(rncid), NULL, ids);
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

void
init_nc_group (void)
{
  rb_define_module_function(mNetCDF, "nc_def_grp",           rb_nc_def_grp,           2);
  rb_define_module_function(mNetCDF, "nc_rename_grp",        rb_nc_rename_grp,        2);
  rb_define_module_function(mNetCDF, "nc_inq_grps",          rb_nc_inq_grps,          1);
  rb_define_module_function(mNetCDF, "nc_inq_grpname",       rb_nc_inq_grpname,       1);
  rb_define_module_function(mNetCDF, "nc_inq_grpname_full",  rb_nc_inq_grpname_full,  1);
  rb_define_module_function(mNetCDF, "nc_inq_grp_parent",    rb_nc_inq_grp_parent,    1);
  rb_define_module_function(mNetCDF, "nc_inq_ncid",          rb_nc_inq_ncid,          2);
  rb_define_module_function(mNetCDF, "nc_inq_grp_full_ncid", rb_nc_inq_grp_full_ncid, 2);
  rb_define_module_function(mNetCDF, "nc_inq_dimids",        rb_nc_inq_dimids,       -1);
  rb_define_module_function(mNetCDF, "nc_inq_varids",        rb_nc_inq_varids,        1);
}
