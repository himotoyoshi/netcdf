# Round-trip data between a NetCDF file and CArray zero-copy.
#
#   $ gem install carray
#   $ ruby -Ilib examples/05_roundtrip_carray.rb
#
# CArray implements the MemoryView protocol natively, so the
# netcdf gem can read from / write to a CArray's storage directly.

require "netcdf"
require "carray"   # a MemoryView producer/consumer

require "tmpdir"
Dir.mktmpdir do |tmp|
  path = File.join(tmp, "carray.nc")
  ncid = NetCDF.nc_create(path, NetCDF::NC_CLOBBER | NetCDF::NC_NETCDF4)
  yid  = NetCDF.nc_def_dim(ncid, "y", 3)
  xid  = NetCDF.nc_def_dim(ncid, "x", 4)

  v_d = NetCDF.nc_def_var(ncid, "T", NetCDF::NC_DOUBLE, [yid, xid])
  v_n = NetCDF.nc_def_var(ncid, "N", NetCDF::NC_INT,    [yid, xid])

  NetCDF.nc_enddef(ncid)

  # 1. CArray double, plain round-trip.
  src = CArray.double(3, 4).seq(1.0)
  NetCDF.nc_put_var(ncid, v_d, src)
  dst = CArray.double(3, 4)
  NetCDF.nc_get_var(ncid, v_d, dst)
  puts "CArray double round-trip matches: #{src.to_a == dst.to_a}"

  # 2. CArray int32, plain round-trip.
  src = CArray.int32(3, 4).seq
  NetCDF.nc_put_var(ncid, v_n, src)
  dst = CArray.int32(3, 4)
  NetCDF.nc_get_var(ncid, v_n, dst)
  puts "CArray int32 round-trip matches:  #{src.to_a == dst.to_a}"

  # 3. libnetcdf converts on the way out: the int32 variable read into a
  #    double CArray.
  dst = CArray.double(3, 4)
  NetCDF.nc_get_var(ncid, v_n, dst)
  puts "int32 -> CArray double matches:   #{dst.to_a == CArray.double(3, 4).seq.to_a}"

  NetCDF.nc_close(ncid)
end
