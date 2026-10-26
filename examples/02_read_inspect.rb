# Open an existing NetCDF file, walk its structure, and read a variable.
#
#   $ ruby -Ilib examples/01_create_simple.rb     # produce simple.nc
#   $ ruby -Ilib examples/02_read_inspect.rb simple.nc

require "netcdf"
require "carray"

path = ARGV[0] or abort "usage: 02_read_inspect.rb FILE.nc"

ncid = NetCDF.nc_open(path, NetCDF::NC_NOWRITE)

ndims, nvars, natts, unlimid = NetCDF.nc_inq(ncid)
puts "Format     : #{NetCDF.nc_inq_format(ncid)}"
puts "Dimensions : #{ndims}, Variables: #{nvars}, Global attrs: #{natts}"
puts "Unlimited  : #{unlimid ? NetCDF.nc_inq_dimname(ncid, unlimid) : '(none)'}"
puts

ndims.times do |dimid|
  name, len = NetCDF.nc_inq_dim(ncid, dimid)
  puts "  dim   #{name} = #{len}"
end

nvars.times do |varid|
  name, xtype, _, dimids, vatts = NetCDF.nc_inq_var(ncid, varid)
  shape = dimids.map { |d| NetCDF.nc_inq_dimlen(ncid, d) }
  puts "  var   #{name} : nc_type=#{xtype} shape=#{shape}"
  vatts.times do |an|
    aname = NetCDF.nc_inq_attname(ncid, varid, an)
    puts "        @#{aname} = #{NetCDF.nc_get_att(ncid, varid, aname).inspect}"
  end
end

# Read variable 0 in its entirety into a buffer of its shape.  The element
# type is ours to choose -- libnetcdf converts to it -- so doubles it is.
_, _, _, dimids, _ = NetCDF.nc_inq_var(ncid, 0)
shape = dimids.map { |d| NetCDF.nc_inq_dimlen(ncid, d) }
data = CArray.double(*shape)
NetCDF.nc_get_var(ncid, 0, data)
puts
puts "First 6 values of variable 0: #{data.to_a.flatten.first(6).inspect}"

NetCDF.nc_close(ncid)
