require "test-unit"
require "tmpdir"
require "netcdf"
require_relative "memory_buffer"

class TestVar < Test::Unit::TestCase
  def setup
    @tmp  = Dir.mktmpdir
    @path = File.join(@tmp, "t.nc")
    @ncid = NetCDF.nc_create(@path, NetCDF::NC_CLOBBER | NetCDF::NC_NETCDF4)
    @yid  = NetCDF.nc_def_dim(@ncid, "y", 3)
    @xid  = NetCDF.nc_def_dim(@ncid, "x", 4)
  end

  def teardown
    NetCDF.nc_close(@ncid) rescue nil
    FileUtils.remove_entry(@tmp) if File.directory?(@tmp)
  end

  def test_round_trip_double
    v = NetCDF.nc_def_var(@ncid, "T", NetCDF::NC_DOUBLE, [@yid, @xid])
    NetCDF.nc_enddef(@ncid)

    src = MemoryBuffer.new([3, 4], format: "d")
    src.write((1..12).map(&:to_f).pack("d*"))
    NetCDF.nc_put_var(@ncid, v, src)

    dst = MemoryBuffer.new([3, 4], format: "d")
    assert_same dst, NetCDF.nc_get_var(@ncid, v, dst)
    assert_equal (1..12).map(&:to_f), dst.to_s.unpack("d*")
  end

  def test_char_var
    v = NetCDF.nc_def_var(@ncid, "S", NetCDF::NC_CHAR, [@yid, @xid])
    NetCDF.nc_enddef(@ncid)

    src = MemoryBuffer.new([3, 4], format: "c")
    src.write("Hello, World")
    NetCDF.nc_put_var(@ncid, v, src)

    dst = MemoryBuffer.new([3, 4], format: "c")
    NetCDF.nc_get_var(@ncid, v, dst)
    assert_equal "Hello, World", dst.to_s
  end

  def test_scalar_variable
    v = NetCDF.nc_def_var(@ncid, "k", NetCDF::NC_DOUBLE, [])
    NetCDF.nc_enddef(@ncid)

    src = MemoryBuffer.new([], format: "d")  # ndim=0
    src.write([3.14].pack("d"))
    NetCDF.nc_put_var(@ncid, v, src)

    dst = MemoryBuffer.new([], format: "d")
    NetCDF.nc_get_var(@ncid, v, dst)
    assert_equal 3.14, dst.to_s.unpack1("d")
  end

  def test_copy_var
    v = NetCDF.nc_def_var(@ncid, "T", NetCDF::NC_DOUBLE, [@yid, @xid])
    NetCDF.nc_put_att(@ncid, v, "units", "K")
    NetCDF.nc_enddef(@ncid)
    src = MemoryBuffer.new([3, 4], format: "d")
    src.write((1..12).map(&:to_f).pack("d*"))
    NetCDF.nc_put_var(@ncid, v, src)

    out = NetCDF.nc_create(File.join(@tmp, "u.nc"), NetCDF::NC_CLOBBER | NetCDF::NC_NETCDF4)
    NetCDF.nc_def_dim(out, "y", 3)
    NetCDF.nc_def_dim(out, "x", 4)
    assert_nil NetCDF.nc_copy_var(@ncid, v, out)

    w = NetCDF.nc_inq_varid(out, "T")
    assert_equal NetCDF::NC_DOUBLE, NetCDF.nc_inq_vartype(out, w)
    assert_equal "K", NetCDF.nc_get_att(out, w, "units")
    dst = MemoryBuffer.new([3, 4], format: "d")
    NetCDF.nc_get_var(out, w, dst)
    assert_equal (1..12).map(&:to_f), dst.to_s.unpack("d*")
    NetCDF.nc_close(out)
  end

  def test_copy_var_needs_dims_by_name
    v = NetCDF.nc_def_var(@ncid, "T", NetCDF::NC_DOUBLE, [@yid, @xid])
    NetCDF.nc_enddef(@ncid)

    out = NetCDF.nc_create(File.join(@tmp, "u.nc"), NetCDF::NC_CLOBBER | NetCDF::NC_NETCDF4)
    assert_raise(NetCDF::Error) do
      NetCDF.nc_copy_var(@ncid, v, out)
    end
    NetCDF.nc_close(out)
  end
end
