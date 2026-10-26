require "test-unit"
require "tmpdir"
require "netcdf"

class TestInqMisc < Test::Unit::TestCase
  def setup
    @tmp  = Dir.mktmpdir
    @path = File.join(@tmp, "t.nc")
  end

  def teardown
    FileUtils.remove_entry(@tmp) if File.directory?(@tmp)
  end

  def test_libvers
    v = NetCDF.nc_inq_libvers
    assert_kind_of String, v
    assert_match(/\d+\.\d+/, v)
  end

  def test_inq_path
    omit "linked libnetcdf lacks nc_inq_path" unless NetCDF.respond_to?(:nc_inq_path)
    ncid = NetCDF.nc_create(@path, NetCDF::NC_CLOBBER)
    assert_equal File.expand_path(@path), File.expand_path(NetCDF.nc_inq_path(ncid))
    NetCDF.nc_close(ncid)
  end

  def test_inq_format_classic
    ncid = NetCDF.nc_create(@path, NetCDF::NC_CLOBBER)
    assert_equal NetCDF::NC_FORMAT_CLASSIC, NetCDF.nc_inq_format(ncid)
    NetCDF.nc_close(ncid)
  end

  def test_inq_format_netcdf4
    ncid = NetCDF.nc_create(@path, NetCDF::NC_CLOBBER | NetCDF::NC_NETCDF4)
    assert_equal NetCDF::NC_FORMAT_NETCDF4, NetCDF.nc_inq_format(ncid)
    NetCDF.nc_close(ncid)
  end

  def test_inq_format_extended
    omit "linked libnetcdf lacks nc_inq_format_extended" unless NetCDF.respond_to?(:nc_inq_format_extended)
    ncid = NetCDF.nc_create(@path, NetCDF::NC_CLOBBER | NetCDF::NC_NETCDF4)
    fmt, mode = NetCDF.nc_inq_format_extended(ncid)
    assert_equal NetCDF::NC_FORMATX_NC_HDF5, fmt
    assert_kind_of Integer, mode
    NetCDF.nc_close(ncid)
  end

  def test_inq_type
    ncid = NetCDF.nc_create(@path, NetCDF::NC_CLOBBER | NetCDF::NC_NETCDF4)
    name, size = NetCDF.nc_inq_type(ncid, NetCDF::NC_DOUBLE)
    assert_equal "double", name
    assert_equal 8,        size

    name, size = NetCDF.nc_inq_type(ncid, NetCDF::NC_INT)
    assert_equal "int", name
    assert_equal 4,     size
    NetCDF.nc_close(ncid)
  end

  def test_abort
    ncid = NetCDF.nc_create(@path, NetCDF::NC_CLOBBER)
    NetCDF.nc_def_dim(ncid, "x", 10)
    NetCDF.nc_abort(ncid)
    # After abort the (empty) file is removed.
    assert !File.exist?(@path), "file should be removed after nc_abort on a fresh nc_create"
  end
end
