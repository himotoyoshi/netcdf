require "test-unit"
require "tmpdir"
require "netcdf"
require_relative "memory_buffer"

# What a buffer's own state does to a call -- a read landing whole in it,
# a frozen producer refusing a writable view but serving as a source.
class TestBuffer < Test::Unit::TestCase
  def setup
    @tmp  = Dir.mktmpdir
    @path = File.join(@tmp, "t.nc")
    @ncid = NetCDF.nc_create(@path, NetCDF::NC_CLOBBER | NetCDF::NC_NETCDF4)
    @yid  = NetCDF.nc_def_dim(@ncid, "y", 3)
    @xid  = NetCDF.nc_def_dim(@ncid, "x", 4)
    @v    = NetCDF.nc_def_var(@ncid, "T", NetCDF::NC_DOUBLE, [@yid, @xid])
    NetCDF.nc_enddef(@ncid)

    @values = (1..12).map(&:to_f)
    src = MemoryBuffer.new([3, 4], format: "d")
    src.write(@values.pack("d*"))
    NetCDF.nc_put_vara(@ncid, @v, [0, 0], [3, 4], src)
  end

  def teardown
    NetCDF.nc_close(@ncid) rescue nil
    FileUtils.remove_entry(@tmp) if File.directory?(@tmp)
  end

  # A read into a MemoryBuffer equals one built from the same values by
  # hand: the bytes landed where they belong and the view's shape and
  # format were left as they were.
  def test_read_equals_independent_buffer
    got      = MemoryBuffer.new([3, 4], format: "d")
    NetCDF.nc_get_vara(@ncid, @v, [0, 0], [3, 4], got)
    expected = MemoryBuffer.new([3, 4], format: "d")
    expected.write(@values.pack("d*"))
    assert_equal expected, got
  end

  # A frozen buffer is rejected as the destination of nc_get_vara: its
  # producer refuses to hand out a writable view of it.
  def test_frozen_buffer_rejected_on_get
    dst = MemoryBuffer.new([3, 4], format: "d")
    dst.freeze
    e = assert_raise(TypeError) do
      NetCDF.nc_get_vara(@ncid, @v, [0, 0], [3, 4], dst)
    end
    assert_match(/must provide a writable MemoryView/, e.message)
    assert_empty MemoryBuffer::FIXTURE_ERRORS
  end

  # A frozen buffer is still fine as a source for nc_put_vara.
  def test_frozen_buffer_ok_for_put
    NetCDF.nc_redef(@ncid)
    v2 = NetCDF.nc_def_var(@ncid, "T2", NetCDF::NC_DOUBLE, [@yid, @xid])
    NetCDF.nc_enddef(@ncid)

    src = MemoryBuffer.new([3, 4], format: "d")
    src.write(@values.pack("d*"))
    src.freeze
    assert_nothing_raised do
      NetCDF.nc_put_vara(@ncid, v2, [0, 0], [3, 4], src)
    end
    got = MemoryBuffer.new([3, 4], format: "d")
    NetCDF.nc_get_vara(@ncid, v2, [0, 0], [3, 4], got)
    assert_equal src, got
  end
end
