# Configure per-variable NetCDF-4 storage options: chunking + deflate.
#
#   $ ruby -Ilib examples/04_compression.rb
#   $ ncdump -h compressed.nc

require "netcdf"

path = "compressed.nc"
ncid = NetCDF.nc_create(path, NetCDF::NC_CLOBBER | NetCDF::NC_NETCDF4)

t = NetCDF.nc_def_dim(ncid, "t", 100)
y = NetCDF.nc_def_dim(ncid, "y", 90)
x = NetCDF.nc_def_dim(ncid, "x", 180)

v = NetCDF.nc_def_var(ncid, "T", NetCDF::NC_FLOAT, [t, y, x])

# Chunking: 1 time slice at a time, full lat/lon plane.
NetCDF.nc_def_var_chunking(ncid, v, NetCDF::NC_CHUNKED, [1, 90, 180])

# Compression: shuffle + zlib level 4.
NetCDF.nc_def_var_deflate(ncid, v, true, true, 4)

# Checksum protection.
NetCDF.nc_def_var_fletcher32(ncid, v, true)

# Fill value for missing data.
NetCDF.nc_def_var_fill(ncid, v, false, -9999.0)

NetCDF.nc_enddef(ncid)

# Report what we set.
storage, sizes = NetCDF.nc_inq_var_chunking(ncid, v)
shuffle, deflate, level = NetCDF.nc_inq_var_deflate(ncid, v)
no_fill, fill = NetCDF.nc_inq_var_fill(ncid, v)

puts "Storage    : #{storage} (NC_CHUNKED=#{NetCDF::NC_CHUNKED})"
puts "Chunk      : #{sizes.inspect}"
puts "Deflate    : shuffle=#{shuffle} deflate=#{deflate} level=#{level}"
puts "Fletcher32 : #{NetCDF.nc_inq_var_fletcher32(ncid, v)}"
puts "Fill       : no_fill=#{no_fill} value=#{fill}"

NetCDF.nc_close(ncid)
