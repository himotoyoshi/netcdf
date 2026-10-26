# Use NetCDF-4 groups to organize variables hierarchically.
#
#   $ ruby -Ilib examples/03_groups.rb
#   $ ncdump groups.nc

require "netcdf"

path = "groups.nc"
ncid = NetCDF.nc_create(path, NetCDF::NC_CLOBBER | NetCDF::NC_NETCDF4)

# Top-level metadata
NetCDF.nc_put_att(ncid, NetCDF::NC_GLOBAL, "Conventions", "CF-1.10")

# Create /observation and /model groups
obs   = NetCDF.nc_def_grp(ncid, "observation")
model = NetCDF.nc_def_grp(ncid, "model")

# Each group has its own dimensions and variables.
[obs, model].each do |g|
  t = NetCDF.nc_def_dim(g, "time", 24)
  v = NetCDF.nc_def_var(g, "temperature", NetCDF::NC_FLOAT, [t])
  NetCDF.nc_put_att(g, v, "units", "K")
end

NetCDF.nc_enddef(ncid)

# Walk the group tree.
def walk(ncid, indent = 0)
  prefix = "  " * indent
  puts "#{prefix}#{NetCDF.nc_inq_grpname_full(ncid)}"
  NetCDF.nc_inq_varids(ncid).each do |varid|
    name, = NetCDF.nc_inq_var(ncid, varid)
    puts "#{prefix}  var #{name}"
  end
  NetCDF.nc_inq_grps(ncid).each { |g| walk(g, indent + 1) }
end

walk(ncid)

# Look up a group by full path.
gid = NetCDF.nc_inq_grp_full_ncid(ncid, "/observation")
puts "\n/observation has #{NetCDF.nc_inq_varids(gid).length} variable(s)"

NetCDF.nc_close(ncid)
