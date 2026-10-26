require "test-unit"
require "tmpdir"
require "netcdf"

class TestFilterQuantize < Test::Unit::TestCase
  def setup
    @tmp  = Dir.mktmpdir
    @path = File.join(@tmp, "t.nc")
    @ncid = NetCDF.nc_create(@path, NetCDF::NC_CLOBBER | NetCDF::NC_NETCDF4)
    @y    = NetCDF.nc_def_dim(@ncid, "y", 10)
    @x    = NetCDF.nc_def_dim(@ncid, "x", 20)
    @v    = NetCDF.nc_def_var(@ncid, "T", NetCDF::NC_FLOAT, [@y, @x])
    # chunking is required for filters
    NetCDF.nc_def_var_chunking(@ncid, @v, NetCDF::NC_CHUNKED, [10, 20])
  end

  def teardown
    NetCDF.nc_close(@ncid) rescue nil
    FileUtils.remove_entry(@tmp) if File.directory?(@tmp)
  end

  def test_inq_filter_avail_for_deflate
    omit "linked libnetcdf lacks nc_inq_filter_avail" unless NetCDF.respond_to?(:nc_inq_filter_avail)
    assert_equal true, NetCDF.nc_inq_filter_avail(@ncid, 1)
  end

  def test_filter_no_filter_returns_nil
    omit "linked libnetcdf lacks nc_inq_var_filter_info" unless NetCDF.respond_to?(:nc_inq_var_filter_info)
    assert_nil NetCDF.nc_inq_var_filter_info(@ncid, @v, 1)
  end

  def test_zlib_filter_via_def_var_filter
    omit "linked libnetcdf lacks nc_def_var_filter" unless NetCDF.respond_to?(:nc_def_var_filter)
    NetCDF.nc_def_var_filter(@ncid, @v, 1, [4])
    info = NetCDF.nc_inq_var_filter_info(@ncid, @v, 1)
    assert_equal [4], info
    ids = NetCDF.nc_inq_var_filter_ids(@ncid, @v)
    assert_includes ids, 1
  end

  def test_quantize_bitgroom
    omit "linked libnetcdf lacks nc_def_var_quantize" unless NetCDF.respond_to?(:nc_def_var_quantize)
    NetCDF.nc_def_var_quantize(@ncid, @v, NetCDF::NC_QUANTIZE_BITGROOM, 3)
    mode, nsd = NetCDF.nc_inq_var_quantize(@ncid, @v)
    assert_equal NetCDF::NC_QUANTIZE_BITGROOM, mode
    assert_equal 3, nsd
  end

  def test_quantize_off_by_default
    omit "linked libnetcdf lacks nc_inq_var_quantize" unless NetCDF.respond_to?(:nc_inq_var_quantize)
    mode, _nsd = NetCDF.nc_inq_var_quantize(@ncid, @v)
    assert_equal NetCDF::NC_NOQUANTIZE, mode
  end
end
