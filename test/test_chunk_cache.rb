require "test-unit"
require "tmpdir"
require "netcdf"

class TestChunkCache < Test::Unit::TestCase
  def setup
    @tmp  = Dir.mktmpdir
    @path = File.join(@tmp, "t.nc")
    @ncid = NetCDF.nc_create(@path, NetCDF::NC_CLOBBER | NetCDF::NC_NETCDF4)
    @y    = NetCDF.nc_def_dim(@ncid, "y", 10)
    @x    = NetCDF.nc_def_dim(@ncid, "x", 20)
    @v    = NetCDF.nc_def_var(@ncid, "T", NetCDF::NC_DOUBLE, [@y, @x])
    NetCDF.nc_def_var_chunking(@ncid, @v, NetCDF::NC_CHUNKED, [10, 20])
  end

  def teardown
    NetCDF.nc_close(@ncid) rescue nil
    FileUtils.remove_entry(@tmp) if File.directory?(@tmp)
  end

  def test_per_var_chunk_cache
    NetCDF.nc_set_var_chunk_cache(@ncid, @v, 4 * 1024 * 1024, 1009, 0.5)
    size, nelems, preemption = NetCDF.nc_get_var_chunk_cache(@ncid, @v)
    assert_equal 4 * 1024 * 1024, size
    assert_equal 1009,            nelems
    assert_in_delta 0.5, preemption, 1e-6
  end

  def test_global_chunk_cache
    # Read first so we can restore the original after the test.
    orig_size, orig_nelems, orig_pre = NetCDF.nc_get_chunk_cache
    begin
      NetCDF.nc_set_chunk_cache(8 * 1024 * 1024, 521, 0.75)
      size, nelems, pre = NetCDF.nc_get_chunk_cache
      assert_equal 8 * 1024 * 1024, size
      assert_equal 521,             nelems
      assert_in_delta 0.75, pre, 1e-6
    ensure
      NetCDF.nc_set_chunk_cache(orig_size, orig_nelems, orig_pre)
    end
  end
end
