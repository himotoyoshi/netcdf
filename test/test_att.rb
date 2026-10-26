require "test-unit"
require "tmpdir"
require "netcdf"

class TestAtt < Test::Unit::TestCase
  def setup
    @tmp = Dir.mktmpdir
    @path = File.join(@tmp, "t.nc")
    @ncid = NetCDF.nc_create(@path, NetCDF::NC_CLOBBER | NetCDF::NC_NETCDF4)
    dim = NetCDF.nc_def_dim(@ncid, "x", 4)
    @varid = NetCDF.nc_def_var(@ncid, "v", NetCDF::NC_DOUBLE, [dim])
  end

  def teardown
    NetCDF.nc_close(@ncid) rescue nil
    FileUtils.remove_entry(@tmp) if File.directory?(@tmp)
  end

  # A value the attribute's type cannot hold is refused, not truncated
  # (300 as NC_UBYTE 44) or wrapped (-1 as the type's maximum).
  def test_integer_out_of_range_raises
    g = NetCDF::NC_GLOBAL
    assert_raise(RangeError) { NetCDF.nc_put_att_schar(@ncid, g, "a", NetCDF::NC_BYTE, 128) }
    assert_raise(RangeError) { NetCDF.nc_put_att_uchar(@ncid, g, "a", NetCDF::NC_UBYTE, [255, 300]) }
    assert_raise(RangeError) { NetCDF.nc_put_att_uchar(@ncid, g, "a", NetCDF::NC_UBYTE, -1) }
    assert_raise(RangeError) { NetCDF.nc_put_att_short(@ncid, g, "a", NetCDF::NC_SHORT, 40000) }
    assert_raise(RangeError) { NetCDF.nc_put_att_ushort(@ncid, g, "a", NetCDF::NC_USHORT, -1) }
    assert_raise(RangeError) { NetCDF.nc_put_att_uint(@ncid, g, "a", NetCDF::NC_UINT, -1) }
    assert_raise(RangeError) { NetCDF.nc_put_att_ulonglong(@ncid, g, "a", NetCDF::NC_UINT64, -1) }
    assert_raise(RangeError) { NetCDF.nc_put_att_ulonglong(@ncid, g, "a", NetCDF::NC_UINT64, -2**64) }

    NetCDF.nc_put_att_schar(@ncid, g, "lo", NetCDF::NC_BYTE, [-128, 127])
    NetCDF.nc_put_att_uchar(@ncid, g, "hi", NetCDF::NC_UBYTE, [0, 255])
    NetCDF.nc_put_att_ulonglong(@ncid, g, "big", NetCDF::NC_UINT64, 2**64 - 1)
    assert_equal [-128, 127], NetCDF.nc_get_att(@ncid, g, "lo")
    assert_equal [0, 255],    NetCDF.nc_get_att(@ncid, g, "hi")
    assert_equal 2**64 - 1,   NetCDF.nc_get_att(@ncid, g, "big")
  end

  # xtype is the on-disk type, as in C: a Float can be stored as NC_FLOAT,
  # which a float variable's _FillValue has to be.
  def test_put_att_xtype_differs_from_memory_type
    dim = NetCDF.nc_inq_dimid(@ncid, "x")
    f = NetCDF.nc_def_var(@ncid, "f", NetCDF::NC_FLOAT, [dim])
    NetCDF.nc_put_att_double(@ncid, f, "_FillValue", NetCDF::NC_FLOAT, -999.0)
    NetCDF.nc_put_att_int(@ncid, f, "flags", NetCDF::NC_SHORT, [1, 2])

    assert_equal [NetCDF::NC_FLOAT, 1], NetCDF.nc_inq_att(@ncid, f, "_FillValue")
    assert_equal [NetCDF::NC_SHORT, 2], NetCDF.nc_inq_att(@ncid, f, "flags")
    assert_equal(-999.0, NetCDF.nc_get_att(@ncid, f, "_FillValue"))
    assert_equal [1, 2], NetCDF.nc_get_att(@ncid, f, "flags")
  end

  # The smart put picks the narrowest of NC_INT, NC_INT64 and NC_UINT64,
  # the last only for a value past 2**63 - 1.
  def test_smart_put_att_integer_types
    g = NetCDF::NC_GLOBAL
    cases = {
      "i_lo"  => [-2**31,         NetCDF::NC_INT],
      "l_hi"  => [2**31,          NetCDF::NC_INT64],
      "l_lo"  => [-2**63,         NetCDF::NC_INT64],
      "l_big" => [2**62,          NetCDF::NC_INT64],
      "u_lo"  => [2**63,          NetCDF::NC_UINT64],
      "u_hi"  => [2**64 - 1,      NetCDF::NC_UINT64],
      "u_ary" => [[1, 2**64 - 1], NetCDF::NC_UINT64],
      "l_ary" => [[-1, 2**40],    NetCDF::NC_INT64],
    }
    cases.each do |name, (value, xtype)|
      NetCDF.nc_put_att(@ncid, g, name, value)
      assert_equal xtype, NetCDF.nc_inq_atttype(@ncid, g, name), name
      assert_equal value, NetCDF.nc_get_att(@ncid, g, name), name
    end

    assert_raise(RangeError) { NetCDF.nc_put_att(@ncid, g, "x", 2**64) }
    assert_raise(RangeError) { NetCDF.nc_put_att(@ncid, g, "x", -2**63 - 1) }
    assert_raise(RangeError) { NetCDF.nc_put_att(@ncid, g, "x", [-1, 2**63]) }
  end

  def test_text_round_trip
    NetCDF.nc_put_att_text(@ncid, @varid, "units", "meter/second")
    assert_equal NetCDF::NC_CHAR, NetCDF.nc_inq_atttype(@ncid, @varid, "units")
    assert_equal 12,              NetCDF.nc_inq_attlen(@ncid, @varid, "units")
    assert_equal "meter/second",  NetCDF.nc_get_att_text(@ncid, @varid, "units")
  end

  def test_int_scalar_and_array
    NetCDF.nc_put_att_int(@ncid, @varid, "valid_min", NetCDF::NC_INT, 0)
    NetCDF.nc_put_att_int(@ncid, @varid, "range", NetCDF::NC_INT, [-100, 100])
    assert_equal NetCDF::NC_INT, NetCDF.nc_inq_atttype(@ncid, @varid, "valid_min")
    assert_equal [0],            NetCDF.nc_get_att_int(@ncid, @varid, "valid_min")
    assert_equal [-100, 100],    NetCDF.nc_get_att_int(@ncid, @varid, "range")
  end

  def test_double
    NetCDF.nc_put_att_double(@ncid, @varid, "scale_factor", NetCDF::NC_DOUBLE, 0.5)
    assert_equal NetCDF::NC_DOUBLE, NetCDF.nc_inq_atttype(@ncid, @varid, "scale_factor")
    assert_equal [0.5],             NetCDF.nc_get_att_double(@ncid, @varid, "scale_factor")
  end

  def test_global_att
    NetCDF.nc_put_att_text(@ncid, NetCDF::NC_GLOBAL, "title", "hello")
    assert_equal "hello", NetCDF.nc_get_att_text(@ncid, NetCDF::NC_GLOBAL, "title")
  end

  def test_inq_attname_and_attid
    NetCDF.nc_put_att_int(@ncid, @varid, "a", NetCDF::NC_INT, 1)
    NetCDF.nc_put_att_int(@ncid, @varid, "b", NetCDF::NC_INT, 2)
    assert_equal "a", NetCDF.nc_inq_attname(@ncid, @varid, 0)
    assert_equal 0,   NetCDF.nc_inq_attid(@ncid, @varid, "a")
    assert_nil        NetCDF.nc_inq_attid(@ncid, @varid, "nope")
  end

  def test_rename_and_del
    NetCDF.nc_put_att_int(@ncid, @varid, "tmp", NetCDF::NC_INT, 1)
    NetCDF.nc_rename_att(@ncid, @varid, "tmp", "renamed")
    assert_equal [1], NetCDF.nc_get_att_int(@ncid, @varid, "renamed")
    NetCDF.nc_del_att(@ncid, @varid, "renamed")
    assert_nil NetCDF.nc_inq_attid(@ncid, @varid, "renamed")
  end

  def test_type_conversion_on_get
    NetCDF.nc_put_att_int(@ncid, @varid, "n", NetCDF::NC_INT, 42)
    assert_equal [42.0], NetCDF.nc_get_att_double(@ncid, @varid, "n")
  end

  # generic put_att: type inferred from the Ruby value
  def test_generic_string
    NetCDF.nc_put_att(@ncid, @varid, "units", "m/s")
    assert_equal NetCDF::NC_CHAR, NetCDF.nc_inq_atttype(@ncid, @varid, "units")
    assert_equal "m/s", NetCDF.nc_get_att(@ncid, @varid, "units")
  end

  def test_generic_integer
    NetCDF.nc_put_att(@ncid, @varid, "n", 42)
    assert_equal NetCDF::NC_INT, NetCDF.nc_inq_atttype(@ncid, @varid, "n")
    assert_equal 42, NetCDF.nc_get_att(@ncid, @varid, "n")   # scalar, not [42]
  end

  def test_generic_big_integer
    NetCDF.nc_put_att(@ncid, @varid, "big", 1 << 40)
    assert_equal NetCDF::NC_INT64, NetCDF.nc_inq_atttype(@ncid, @varid, "big")
    assert_equal (1 << 40), NetCDF.nc_get_att(@ncid, @varid, "big")
  end

  def test_generic_float
    NetCDF.nc_put_att(@ncid, @varid, "scale", 0.5)
    assert_equal NetCDF::NC_DOUBLE, NetCDF.nc_inq_atttype(@ncid, @varid, "scale")
    assert_equal 0.5, NetCDF.nc_get_att(@ncid, @varid, "scale")
  end

  def test_generic_int_array
    NetCDF.nc_put_att(@ncid, @varid, "range", [-100, 100])
    assert_equal NetCDF::NC_INT,    NetCDF.nc_inq_atttype(@ncid, @varid, "range")
    assert_equal [-100, 100],       NetCDF.nc_get_att(@ncid, @varid, "range")
  end

  def test_generic_mixed_array_promotes_to_double
    NetCDF.nc_put_att(@ncid, @varid, "mix", [1, 2.5, 3])
    assert_equal NetCDF::NC_DOUBLE, NetCDF.nc_inq_atttype(@ncid, @varid, "mix")
    assert_equal [1.0, 2.5, 3.0],   NetCDF.nc_get_att(@ncid, @varid, "mix")
  end
end
