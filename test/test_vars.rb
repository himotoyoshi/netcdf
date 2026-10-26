require "test-unit"
require "tmpdir"
require "netcdf"
require_relative "memory_buffer"

class TestVars < Test::Unit::TestCase
  def setup
    @tmp  = Dir.mktmpdir
    @path = File.join(@tmp, "t.nc")
    @ncid = NetCDF.nc_create(@path, NetCDF::NC_CLOBBER | NetCDF::NC_NETCDF4)
    @yid  = NetCDF.nc_def_dim(@ncid, "y", 4)
    @xid  = NetCDF.nc_def_dim(@ncid, "x", 6)
    @v    = NetCDF.nc_def_var(@ncid, "T", NetCDF::NC_DOUBLE, [@yid, @xid])
    NetCDF.nc_enddef(@ncid)

    # Fill the full 4x6 grid with sequential values 0..23.
    full = MemoryBuffer.new([4, 6], format: "d")
    full.write((0...24).map(&:to_f).pack("d*"))
    NetCDF.nc_put_vara(@ncid, @v, [0, 0], [4, 6], full)
  end

  def teardown
    NetCDF.nc_close(@ncid) rescue nil
    FileUtils.remove_entry(@tmp) if File.directory?(@tmp)
  end

  def test_get_vars_strided
    out = MemoryBuffer.new([2, 3], format: "d")
    NetCDF.nc_get_vars(@ncid, @v, [0, 0], [2, 3], [2, 2], out)
    assert_equal [0.0, 2.0, 4.0, 12.0, 14.0, 16.0], out.to_s.unpack("d*")
  end

  def test_get_vars_without_buffer_raises
    assert_raise(ArgumentError) do
      NetCDF.nc_get_vars(@ncid, @v, [0, 0], [2, 3], [2, 2])
    end
  end

  def test_put_vars_strided
    src = MemoryBuffer.new([2, 2], format: "d")
    src.write([100.0, 200.0, 300.0, 400.0].pack("d*"))
    NetCDF.nc_put_vars(@ncid, @v, [1, 1], [2, 2], [2, 2], src)

    full = MemoryBuffer.new([6, 6], format: "d")
    NetCDF.nc_get_var(@ncid, @v, full)
    g = full.to_s.unpack("d*")
    assert_equal 100.0, g[1*6 + 1]
    assert_equal 200.0, g[1*6 + 3]
    assert_equal 300.0, g[3*6 + 1]
    assert_equal 400.0, g[3*6 + 3]
    assert_equal 0.0,   g[0*6 + 0]
  end

  def test_stride_length_mismatch
    out = MemoryBuffer.new([2, 3], format: "d")
    assert_raise(ArgumentError) do
      NetCDF.nc_get_vars(@ncid, @v, [0, 0], [2, 3], [2], out)
    end
  end
end
