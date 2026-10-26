require "test-unit"
require "tmpdir"
require "netcdf"

class TestVarOptions < Test::Unit::TestCase
  def setup
    @tmp  = Dir.mktmpdir
    @path = File.join(@tmp, "t.nc")
    @ncid = NetCDF.nc_create(@path, NetCDF::NC_CLOBBER | NetCDF::NC_NETCDF4)
    @yid  = NetCDF.nc_def_dim(@ncid, "y", 10)
    @xid  = NetCDF.nc_def_dim(@ncid, "x", 20)
    @v    = NetCDF.nc_def_var(@ncid, "T", NetCDF::NC_DOUBLE, [@yid, @xid])
  end

  def teardown
    NetCDF.nc_close(@ncid) rescue nil
    FileUtils.remove_entry(@tmp) if File.directory?(@tmp)
  end

  def test_fill_value
    NetCDF.nc_def_var_fill(@ncid, @v, false, -999.0)
    no_fill, fv = NetCDF.nc_inq_var_fill(@ncid, @v)
    assert_equal false,  no_fill
    assert_equal -999.0, fv
  end

  def test_no_fill
    NetCDF.nc_def_var_fill(@ncid, @v, true, nil)
    no_fill, _fv = NetCDF.nc_inq_var_fill(@ncid, @v)
    assert_equal true, no_fill
  end

  def test_deflate
    NetCDF.nc_def_var_deflate(@ncid, @v, true, true, 4)
    shuffle, deflate, level = NetCDF.nc_inq_var_deflate(@ncid, @v)
    assert_equal true, shuffle
    assert_equal true, deflate
    assert_equal 4,    level
  end

  def test_chunking
    NetCDF.nc_def_var_chunking(@ncid, @v, NetCDF::NC_CHUNKED, [5, 10])
    storage, sizes = NetCDF.nc_inq_var_chunking(@ncid, @v)
    assert_equal NetCDF::NC_CHUNKED, storage
    assert_equal [5, 10],            sizes
  end

  def test_chunking_contiguous
    NetCDF.nc_def_var_chunking(@ncid, @v, NetCDF::NC_CONTIGUOUS, nil)
    storage, _ = NetCDF.nc_inq_var_chunking(@ncid, @v)
    assert_equal NetCDF::NC_CONTIGUOUS, storage
  end

  def test_fletcher32
    NetCDF.nc_def_var_fletcher32(@ncid, @v, true)
    assert_equal true, NetCDF.nc_inq_var_fletcher32(@ncid, @v)
  end

  def test_endian
    NetCDF.nc_def_var_endian(@ncid, @v, NetCDF::NC_ENDIAN_LITTLE)
    assert_equal NetCDF::NC_ENDIAN_LITTLE, NetCDF.nc_inq_var_endian(@ncid, @v)
  end

  def test_int_fill_value
    iv = NetCDF.nc_def_var(@ncid, "N", NetCDF::NC_INT, [@yid, @xid])
    NetCDF.nc_def_var_fill(@ncid, iv, false, -1)
    _, fv = NetCDF.nc_inq_var_fill(@ncid, iv)
    assert_equal -1, fv
  end
end
