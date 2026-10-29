require "test-unit"
require "tmpdir"
require "netcdf"

class TestFile < Test::Unit::TestCase
  def setup
    @tmp = Dir.mktmpdir
    @path = File.join(@tmp, "t.nc")
  end

  def teardown
    FileUtils.remove_entry(@tmp) if File.directory?(@tmp)
  end

  def test_create_inq_close
    ncid = NetCDF.nc_create(@path, NetCDF::NC_CLOBBER)
    assert_kind_of Integer, ncid

    time_id = NetCDF.nc_def_dim(ncid, "time", NetCDF::NC_UNLIMITED)
    x_id    = NetCDF.nc_def_dim(ncid, "x", 10)
    y_id    = NetCDF.nc_def_dim(ncid, "y", 20)

    v_id = NetCDF.nc_def_var(ncid, "T", NetCDF::NC_DOUBLE, [time_id, y_id, x_id])

    NetCDF.nc_enddef(ncid)
    NetCDF.nc_close(ncid)

    ncid = NetCDF.nc_open(@path, NetCDF::NC_NOWRITE)
    assert_equal [3, 1, 0, time_id], NetCDF.nc_inq(ncid)
    assert_equal time_id, NetCDF.nc_inq_unlimdim(ncid)
    assert_equal x_id,    NetCDF.nc_inq_dimid(ncid, "x")
    assert_nil            NetCDF.nc_inq_dimid(ncid, "nope")

    assert_equal ["x", 10], NetCDF.nc_inq_dim(ncid, x_id)
    assert_equal 20,        NetCDF.nc_inq_dimlen(ncid, y_id)

    assert_equal v_id, NetCDF.nc_inq_varid(ncid, "T")
    assert_nil         NetCDF.nc_inq_varid(ncid, "nope")
    name, xtype, ndims, dimids, natts = NetCDF.nc_inq_var(ncid, v_id)
    assert_equal "T", name
    assert_equal NetCDF::NC_DOUBLE, xtype
    assert_equal 3, ndims
    assert_equal [time_id, y_id, x_id], dimids
    assert_equal 0, natts

    NetCDF.nc_close(ncid)
  end

  def test_inq_unlimdims_classic
    ncid = NetCDF.nc_create(@path, NetCDF::NC_CLOBBER)
    NetCDF.nc_def_dim(ncid, "x", 10)
    NetCDF.nc_enddef(ncid)
    assert_equal [], NetCDF.nc_inq_unlimdims(ncid)
    NetCDF.nc_close(ncid)

    ncid = NetCDF.nc_create(@path, NetCDF::NC_CLOBBER)
    time_id = NetCDF.nc_def_dim(ncid, "time", NetCDF::NC_UNLIMITED)
    NetCDF.nc_def_dim(ncid, "x", 10)
    NetCDF.nc_enddef(ncid)
    NetCDF.nc_close(ncid)

    ncid = NetCDF.nc_open(@path, NetCDF::NC_NOWRITE)
    assert_equal [time_id], NetCDF.nc_inq_unlimdims(ncid)
    NetCDF.nc_close(ncid)
  end

  def test_inq_unlimdims_netcdf4_two_unlimited
    ncid = NetCDF.nc_create(@path, NetCDF::NC_CLOBBER | NetCDF::NC_NETCDF4)
    NetCDF.nc_def_dim(ncid, "x", 10)
    time_id    = NetCDF.nc_def_dim(ncid, "time", NetCDF::NC_UNLIMITED)
    station_id = NetCDF.nc_def_dim(ncid, "station", NetCDF::NC_UNLIMITED)
    NetCDF.nc_enddef(ncid)
    NetCDF.nc_close(ncid)

    ncid = NetCDF.nc_open(@path, NetCDF::NC_NOWRITE)
    assert_equal [time_id, station_id], NetCDF.nc_inq_unlimdims(ncid).sort
    assert_equal time_id, NetCDF.nc_inq_unlimdim(ncid)
    NetCDF.nc_close(ncid)
  end

  def test_error_raises_with_status
    err = assert_raise(NetCDF::Error) do
      NetCDF.nc_open(File.join(@tmp, "missing.nc"))
    end
    assert_kind_of Integer, err.status
    assert_not_equal 0, err.status
  end
end
