require "test-unit"
require "tmpdir"
require "netcdf"
require_relative "memory_buffer"

# nc_*_varm: like vars, plus a per-dimension "imap" that gives the buffer
# stride (in elements) for the corresponding variable dimension. Most
# common use: transposing data between buffer layout and file layout.
class TestVarm < Test::Unit::TestCase
  def setup
    @tmp  = Dir.mktmpdir
    @path = File.join(@tmp, "t.nc")
    @ncid = NetCDF.nc_create(@path, NetCDF::NC_CLOBBER | NetCDF::NC_NETCDF4)
    @yid  = NetCDF.nc_def_dim(@ncid, "y", 2)
    @xid  = NetCDF.nc_def_dim(@ncid, "x", 3)
    @v    = NetCDF.nc_def_var(@ncid, "T", NetCDF::NC_DOUBLE, [@yid, @xid])
    NetCDF.nc_enddef(@ncid)
  end

  def teardown
    NetCDF.nc_close(@ncid) rescue nil
    FileUtils.remove_entry(@tmp) if File.directory?(@tmp)
  end

  # Write file as 2x3 from a buffer that we treat as 3x2 (transposed) via imap.
  # File layout (after write):
  #   row 0: [1, 2, 3]
  #   row 1: [4, 5, 6]
  # Source buffer (row-major 3x2) holds: [1, 4, 2, 5, 3, 6]
  # imap tells netcdf: for the y dim, step 1 in buffer; for the x dim, step 2.
  def test_put_varm_transpose
    src = MemoryBuffer.new([3, 2], format: "d")
    src.write([1.0, 4.0, 2.0, 5.0, 3.0, 6.0].pack("d*"))
    NetCDF.nc_put_varm(@ncid, @v, [0, 0], [2, 3], [1, 1], [1, 2], src)

    out = MemoryBuffer.new([2, 3], format: "d")
    NetCDF.nc_get_var(@ncid, @v, out)
    assert_equal [1.0, 2.0, 3.0, 4.0, 5.0, 6.0], out.to_s.unpack("d*")
  end
  # A gapped imap spreads the elements past count's product, so the buffer
  # must reach the last one: [1, 4] puts element (1, 2) at offset 9.
  def test_get_varm_gapped_imap_needs_extent
    src = MemoryBuffer.new([2, 3], format: "d")
    src.write((1..6).map(&:to_f).pack("d*"))
    NetCDF.nc_put_var(@ncid, @v, src)

    assert_raise(ArgumentError) do
      NetCDF.nc_get_varm(@ncid, @v, [0, 0], [2, 3], [1, 1], [1, 4],
                         MemoryBuffer.new([6], format: "d"))
    end

    dst = MemoryBuffer.new([10], format: "d")
    NetCDF.nc_get_varm(@ncid, @v, [0, 0], [2, 3], [1, 1], [1, 4], dst)
    got = dst.to_s.unpack("d*")
    assert_equal [1.0, 4.0, 2.0, 5.0, 3.0, 6.0], got.values_at(0, 1, 4, 5, 8, 9)
  end

  def test_get_varm_gapped_imap_without_buffer_raises
    assert_raise(ArgumentError) do
      NetCDF.nc_get_varm(@ncid, @v, [0, 0], [2, 3], [1, 1], [1, 4])
    end
  end

  def test_negative_imap_raises
    buf = MemoryBuffer.new([6], format: "d")
    assert_raise(ArgumentError) do
      NetCDF.nc_get_varm(@ncid, @v, [0, 0], [2, 3], [1, 1], [-3, 1], buf)
    end
  end
end
