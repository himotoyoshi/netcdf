require "mkmf"

# Prefer nc-config when available, fall back to pkg-config, then bare detection.
nc_config = find_executable("nc-config")
if nc_config
  $CFLAGS  << " " << `#{nc_config} --cflags`.strip
  $LDFLAGS << " " << `#{nc_config} --libs`.strip
  have_library("netcdf") or abort "libnetcdf not linkable via nc-config"
elsif pkg_config("netcdf")
  # pkg_config already populated CFLAGS/LDFLAGS
else
  dir_config("netcdf")
  have_library("netcdf", "nc_open") or abort "libnetcdf not found (install libnetcdf and its headers first)"
end

have_header("netcdf.h") or abort "netcdf.h not found"
have_header("ruby/memory_view.h") or abort "ruby/memory_view.h not found (Ruby >= 3.1 required)"
have_header("ruby/thread.h")

# Minimum supported libnetcdf is 4.1 (the first NetCDF-4 release that
# carried the chunk-cache API and stabilized the user-defined type
# surface).  Everything that arrived after 4.1 is feature-detected, so
# the gem builds against any libnetcdf from 4.1 on.
if nc_config
  version = `#{nc_config} --version`.strip.split.last
  if version =~ /\A(\d+)\.(\d+)/
    major, minor = $1.to_i, $2.to_i
    if major < 4 || (major == 4 && minor < 1)
      abort "libnetcdf >= 4.1 required (found #{version})"
    end
  end
end

# Feature detection for APIs that arrived after 4.1.  Each successful
# probe defines HAVE_<NAME> which the C sources gate optional code on.
# netcdf_filter.h relies on macros declared in netcdf.h (EXTERNL), so
# both headers must be passed together for have_func to compile.
have_func("nc_inq_path",            "netcdf.h")   # 4.1.2+
have_func("nc_inq_format_extended", "netcdf.h")   # 4.2+
have_func("nc_def_var_quantize",    "netcdf.h")   # 4.9+
have_func("nc_inq_var_quantize",    "netcdf.h")   # 4.9+

filter_hdrs = ["netcdf.h", "netcdf_filter.h"]
have_func("nc_def_var_filter",      filter_hdrs)  # 4.6+
have_func("nc_inq_var_filter_info", filter_hdrs)  # 4.6+
have_func("nc_inq_var_filter_ids",  filter_hdrs)  # 4.6+
have_func("nc_inq_filter_avail",    filter_hdrs)  # 4.7+

# In-memory file APIs.  nc_open_mem arrived in 4.4; nc_create_mem and
# nc_close_memio (with NC_memio) arrived in 4.6.2.  netcdf_mem.h needs
# netcdf.h included first (it relies on EXTERNL from netcdf.h), so probe
# both headers together — mirrors the netcdf_filter.h pattern above.
mem_hdrs = ["netcdf.h", "netcdf_mem.h"]
have_header("netcdf_mem.h", "netcdf.h")
have_func("nc_open_mem",     mem_hdrs)        # 4.4+
have_func("nc_create_mem",   mem_hdrs)        # 4.6.2+
have_func("nc_close_memio",  mem_hdrs)        # 4.6.2+

$CFLAGS << " -Wall -Wextra -Wno-unused-parameter"

create_makefile("netcdf/netcdf")
