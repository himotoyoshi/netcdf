/*
 * Internal declarations shared between the netcdf gem's C sources.
 * Not installed; only used at gem build time.
 */
#ifndef NETCDF_INTERNAL_H
#define NETCDF_INTERNAL_H

#include <ruby.h>
#include <ruby/encoding.h>
#include <ruby/memory_view.h>
#include <ruby/thread.h>
#include <netcdf.h>
/* netcdf_filter.h needs EXTERNL from netcdf.h, so include order matters.
   We pull it in whenever any filter API was detected at configure time. */
#if defined(HAVE_NC_DEF_VAR_FILTER)      || \
    defined(HAVE_NC_INQ_VAR_FILTER_INFO) || \
    defined(HAVE_NC_INQ_VAR_FILTER_IDS)  || \
    defined(HAVE_NC_INQ_FILTER_AVAIL)    || \
    defined(HAVE_NC_DEF_VAR_ZSTANDARD)   || \
    defined(HAVE_NC_DEF_VAR_BZIP2)       || \
    defined(HAVE_NC_DEF_VAR_BLOSC)
# include <netcdf_filter.h>
#endif
#ifdef HAVE_NETCDF_MEM_H
# include <netcdf_mem.h>
#endif
#include <limits.h>
#include <stdbool.h>
#include <string.h>

extern VALUE mNetCDF;
extern VALUE eError;

void nc_raise(int status) __attribute__((noreturn));

#define NC_CHECK(call) do {                     \
    int _st = (call);                           \
    if ( _st != NC_NOERR ) {                    \
      nc_raise(_st);                            \
    }                                           \
  } while ( 0 )

/* Common helpers (defined in netcdf.c). */
void    ary_to_sizet(VALUE ary, int ndims, const char *role, size_t *out);
bool    ary_to_ptrdiff(VALUE ary, int ndims, const char *role, ptrdiff_t *out);
ssize_t nc_xtype_size(nc_type xt);
nc_type resolve_enum_base(int ncid, nc_type xt);
void    indices_to_c(int ncid, int varid, VALUE index_ary, size_t *out);

/* Range-checked integer conversions (defined in netcdf.c); each raises
   RangeError for a value its C type cannot hold. */
signed char        num_to_schar(VALUE v);
unsigned char      num_to_uchar(VALUE v);
short              num_to_short(VALUE v);
unsigned short     num_to_ushort(VALUE v);
unsigned int       num_to_uint(VALUE v);
unsigned long long num_to_ulonglong(VALUE v);

/* Submodule init functions, called by Init_netcdf. */
void init_nc_file(void);
void init_nc_dim(void);
void init_nc_var(void);
void init_nc_att(void);
void init_nc_io(void);
void init_nc_group(void);
void init_nc_types(void);

#endif
