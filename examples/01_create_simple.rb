# Create a small NetCDF file with one variable and a few attributes.
#
#   $ gem install carray
#   $ ruby -Ilib examples/01_create_simple.rb
#   $ ncdump simple.nc

require "netcdf"
require "carray"   # a MemoryView producer/consumer

path = "simple.nc"

ncid = NetCDF.nc_create(path, NetCDF::NC_CLOBBER | NetCDF::NC_NETCDF4)

# Dimensions
yid = NetCDF.nc_def_dim(ncid, "y", 4)
xid = NetCDF.nc_def_dim(ncid, "x", 6)

# Variable
vid = NetCDF.nc_def_var(ncid, "temperature", NetCDF::NC_DOUBLE, [yid, xid])

# Attributes (smart generic: type inferred from Ruby value)
NetCDF.nc_put_att(ncid, vid, "units", "K")
NetCDF.nc_put_att(ncid, vid, "valid_range", [200.0, 350.0])
NetCDF.nc_put_att(ncid, vid, "scale_factor", 0.01)
NetCDF.nc_put_att(ncid, NetCDF::NC_GLOBAL, "title", "Example temperature field")

NetCDF.nc_enddef(ncid)

# Write all 24 cells in one shot from a CArray holding the data.
src = CArray.double(4, 6).seq(273.15)
NetCDF.nc_put_var(ncid, vid, src)

NetCDF.nc_close(ncid)
puts "wrote #{path}"
