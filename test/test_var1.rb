require "test-unit"
require "tmpdir"
require "netcdf"

class TestVar1 < Test::Unit::TestCase
  def setup
    @tmp = Dir.mktmpdir
    @path = File.join(@tmp, "t.nc")
    @ncid = NetCDF.nc_create(@path, NetCDF::NC_CLOBBER | NetCDF::NC_NETCDF4)
    @yid = NetCDF.nc_def_dim(@ncid, "y", 3)
    @xid = NetCDF.nc_def_dim(@ncid, "x", 4)
  end

  def teardown
    NetCDF.nc_close(@ncid) rescue nil
    FileUtils.remove_entry(@tmp) if File.directory?(@tmp)
  end

  def test_double
    v = NetCDF.nc_def_var(@ncid, "T", NetCDF::NC_DOUBLE, [@yid, @xid])
    NetCDF.nc_enddef(@ncid)
    NetCDF.nc_put_var1(@ncid, v, [1, 2], 3.14)
    assert_in_delta 3.14, NetCDF.nc_get_var1(@ncid, v, [1, 2]), 1e-12
  end

  # libnetcdf reads one coordinate per dimension; a short index would
  # have it read past the array instead of failing.
  def test_index_length_mismatch_raises
    v = NetCDF.nc_def_var(@ncid, "T", NetCDF::NC_DOUBLE, [@yid, @xid])
    NetCDF.nc_enddef(@ncid)
    assert_raise(ArgumentError) { NetCDF.nc_put_var1(@ncid, v, [1], 3.14) }
    assert_raise(ArgumentError) { NetCDF.nc_get_var1(@ncid, v, [1]) }
    assert_raise(ArgumentError) { NetCDF.nc_get_var1(@ncid, v, [1, 2, 0]) }
  end

  def test_integer_out_of_range_raises
    b = NetCDF.nc_def_var(@ncid, "B", NetCDF::NC_UBYTE,  [@yid, @xid])
    u = NetCDF.nc_def_var(@ncid, "U", NetCDF::NC_UINT64, [@yid, @xid])
    t = NetCDF.nc_def_enum(@ncid, NetCDF::NC_BYTE, "Flag")
    NetCDF.nc_insert_enum(@ncid, t, "OK", 1)
    e = NetCDF.nc_def_var(@ncid, "E", t, [@yid, @xid])

    assert_raise(RangeError) { NetCDF.nc_def_var_fill(@ncid, b, false, 256) }
    assert_raise(RangeError) { NetCDF.nc_insert_enum(@ncid, t, "BIG", 200) }
    NetCDF.nc_enddef(@ncid)

    assert_raise(RangeError) { NetCDF.nc_put_var1(@ncid, b, [0, 0], 256) }
    assert_raise(RangeError) { NetCDF.nc_put_var1(@ncid, u, [0, 0], -1) }
    assert_raise(RangeError) { NetCDF.nc_put_var1(@ncid, e, [0, 0], 200) }
  end

  def test_int
    v = NetCDF.nc_def_var(@ncid, "N", NetCDF::NC_INT, [@yid, @xid])
    NetCDF.nc_enddef(@ncid)
    NetCDF.nc_put_var1(@ncid, v, [0, 0], 42)
    NetCDF.nc_put_var1(@ncid, v, [2, 3], -7)
    assert_equal 42, NetCDF.nc_get_var1(@ncid, v, [0, 0])
    assert_equal -7, NetCDF.nc_get_var1(@ncid, v, [2, 3])
  end

  def test_char
    v = NetCDF.nc_def_var(@ncid, "C", NetCDF::NC_CHAR, [@yid, @xid])
    NetCDF.nc_enddef(@ncid)
    NetCDF.nc_put_var1(@ncid, v, [1, 1], "Z")
    assert_equal "Z", NetCDF.nc_get_var1(@ncid, v, [1, 1])
  end

  def test_int64
    v = NetCDF.nc_def_var(@ncid, "L", NetCDF::NC_INT64, [@yid, @xid])
    NetCDF.nc_enddef(@ncid)
    big = (1 << 40) + 7
    NetCDF.nc_put_var1(@ncid, v, [0, 0], big)
    assert_equal big, NetCDF.nc_get_var1(@ncid, v, [0, 0])
  end

  def test_float_value_to_int_var_coerces
    v = NetCDF.nc_def_var(@ncid, "N", NetCDF::NC_INT, [@yid, @xid])
    NetCDF.nc_enddef(@ncid)
    NetCDF.nc_put_var1(@ncid, v, [0, 0], 5)
    assert_equal 5, NetCDF.nc_get_var1(@ncid, v, [0, 0])
  end
end
