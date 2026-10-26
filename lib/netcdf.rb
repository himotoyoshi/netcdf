require "monitor"
require "netcdf/netcdf"
require "netcdf/version"

module NetCDF
  # libnetcdf, and the HDF5 under it, keep process-wide state and are not
  # thread-safe: two threads inside them at once corrupt it even when they
  # work on different files.  Array I/O runs with the GVL released, and a
  # thread holding the GVL can meanwhile call any other function -- an
  # nc_close of the file being read included -- so every call is
  # serialized on one lock.  Waiting for it releases the GVL, so threads
  # that are not using netCDF keep running.  The lock is reentrant, for a
  # conversion (a #to_int, say) that calls back into NetCDF.
  LOCK = Monitor.new
  private_constant :LOCK

  synchronized = lambda do |names, visibility|
    Module.new do
      names.each do |name|
        define_method(name) { |*args, &blk| LOCK.synchronize { super(*args, &blk) } }
      end
      send(visibility, *names)
    end
  end
  # Both copies module_function makes: NetCDF.nc_open, and the private
  # nc_open a class gets by including NetCDF.
  singleton_class.prepend(synchronized.(singleton_methods(false), :public))
  prepend(synchronized.(private_instance_methods(false), :private))
end
