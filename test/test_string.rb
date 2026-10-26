require "test-unit"
require "tmpdir"
require "netcdf"

class TestString < Test::Unit::TestCase
  def setup
    @tmp  = Dir.mktmpdir
    @path = File.join(@tmp, "t.nc")
    # NC_STRING requires NetCDF-4 (HDF5 backend); not available in classic.
    @ncid = NetCDF.nc_create(@path, NetCDF::NC_CLOBBER | NetCDF::NC_NETCDF4)
  end

  def teardown
    NetCDF.nc_close(@ncid) rescue nil
    FileUtils.remove_entry(@tmp) if File.directory?(@tmp)
  end

  # --- scalar (ndim=0) -----------------------------------------------------

  def test_scalar_var_put_get
    v = NetCDF.nc_def_var(@ncid, "version", NetCDF::NC_STRING, [])
    NetCDF.nc_enddef(@ncid)
    NetCDF.nc_put_var1(@ncid, v, [], "1.2.3")
    assert_equal "1.2.3", NetCDF.nc_get_var1(@ncid, v, [])
  end

  def test_scalar_via_put_var
    v = NetCDF.nc_def_var(@ncid, "version", NetCDF::NC_STRING, [])
    NetCDF.nc_enddef(@ncid)
    NetCDF.nc_put_var(@ncid, v, "0.0.1")
    assert_equal "0.0.1", NetCDF.nc_get_var(@ncid, v)
  end

  # --- 1D ------------------------------------------------------------------

  def test_1d_vara_flat
    d = NetCDF.nc_def_dim(@ncid, "n", 3)
    v = NetCDF.nc_def_var(@ncid, "names", NetCDF::NC_STRING, [d])
    NetCDF.nc_enddef(@ncid)
    NetCDF.nc_put_vara(@ncid, v, [0], [3], ["alice", "bob", "carol"])
    assert_equal ["alice", "bob", "carol"], NetCDF.nc_get_vara(@ncid, v, [0], [3])
  end

  def test_1d_var
    d = NetCDF.nc_def_dim(@ncid, "n", 2)
    v = NetCDF.nc_def_var(@ncid, "names", NetCDF::NC_STRING, [d])
    NetCDF.nc_enddef(@ncid)
    NetCDF.nc_put_var(@ncid, v, ["x", "y"])
    assert_equal ["x", "y"], NetCDF.nc_get_var(@ncid, v)
  end

  # The strings come back as new objects, so a buffer passed for them would
  # be left untouched; it is refused instead.
  def test_get_refuses_a_buffer
    d = NetCDF.nc_def_dim(@ncid, "n", 2)
    v = NetCDF.nc_def_var(@ncid, "names", NetCDF::NC_STRING, [d])
    NetCDF.nc_enddef(@ncid)
    NetCDF.nc_put_var(@ncid, v, ["x", "y"])
    e = assert_raise(ArgumentError) { NetCDF.nc_get_var(@ncid, v, []) }
    assert_match(/take no buffer/, e.message)
  end

  # --- 2D ------------------------------------------------------------------

  def test_2d_get_returns_nested
    y = NetCDF.nc_def_dim(@ncid, "y", 2)
    x = NetCDF.nc_def_dim(@ncid, "x", 3)
    v = NetCDF.nc_def_var(@ncid, "g", NetCDF::NC_STRING, [y, x])
    NetCDF.nc_enddef(@ncid)
    NetCDF.nc_put_vara(@ncid, v, [0, 0], [2, 3],
                      [["a", "b", "c"], ["d", "e", "f"]])
    assert_equal [["a", "b", "c"], ["d", "e", "f"]],
                 NetCDF.nc_get_vara(@ncid, v, [0, 0], [2, 3])
  end

  def test_2d_put_accepts_flat
    y = NetCDF.nc_def_dim(@ncid, "y", 2)
    x = NetCDF.nc_def_dim(@ncid, "x", 3)
    v = NetCDF.nc_def_var(@ncid, "g", NetCDF::NC_STRING, [y, x])
    NetCDF.nc_enddef(@ncid)
    NetCDF.nc_put_vara(@ncid, v, [0, 0], [2, 3],
                      ["a", "b", "c", "d", "e", "f"])
    assert_equal [["a", "b", "c"], ["d", "e", "f"]],
                 NetCDF.nc_get_vara(@ncid, v, [0, 0], [2, 3])
  end

  def test_2d_put_irregular_nested_flattened
    y = NetCDF.nc_def_dim(@ncid, "y", 2)
    x = NetCDF.nc_def_dim(@ncid, "x", 3)
    v = NetCDF.nc_def_var(@ncid, "g", NetCDF::NC_STRING, [y, x])
    NetCDF.nc_enddef(@ncid)
    # Irregular nesting still works as long as flattened length matches.
    NetCDF.nc_put_vara(@ncid, v, [0, 0], [2, 3], [["a"], ["b", "c", "d", "e"], "f"])
    assert_equal [["a", "b", "c"], ["d", "e", "f"]],
                 NetCDF.nc_get_vara(@ncid, v, [0, 0], [2, 3])
  end

  def test_2d_length_mismatch_raises
    y = NetCDF.nc_def_dim(@ncid, "y", 2)
    x = NetCDF.nc_def_dim(@ncid, "x", 3)
    v = NetCDF.nc_def_var(@ncid, "g", NetCDF::NC_STRING, [y, x])
    NetCDF.nc_enddef(@ncid)
    assert_raise(ArgumentError) do
      NetCDF.nc_put_vara(@ncid, v, [0, 0], [2, 3], ["a", "b", "c"])
    end
  end

  # --- 3D nested ------------------------------------------------------------

  def test_3d_round_trip
    a = NetCDF.nc_def_dim(@ncid, "a", 2)
    b = NetCDF.nc_def_dim(@ncid, "b", 2)
    c = NetCDF.nc_def_dim(@ncid, "c", 2)
    v = NetCDF.nc_def_var(@ncid, "cube", NetCDF::NC_STRING, [a, b, c])
    NetCDF.nc_enddef(@ncid)
    src = [
      [["aa", "ab"], ["ba", "bb"]],
      [["ca", "cb"], ["da", "db"]],
    ]
    NetCDF.nc_put_vara(@ncid, v, [0, 0, 0], [2, 2, 2], src)
    assert_equal src, NetCDF.nc_get_vara(@ncid, v, [0, 0, 0], [2, 2, 2])
  end

  # --- attributes ----------------------------------------------------------

  def test_attribute_typed
    NetCDF.nc_put_att_string(@ncid, NetCDF::NC_GLOBAL, "labels", ["x", "y", "z"])
    assert_equal NetCDF::NC_STRING, NetCDF.nc_inq_atttype(@ncid, NetCDF::NC_GLOBAL, "labels")
    assert_equal ["x", "y", "z"],   NetCDF.nc_get_att_string(@ncid, NetCDF::NC_GLOBAL, "labels")
  end

  def test_attribute_generic_get_unwraps_len1
    NetCDF.nc_put_att_string(@ncid, NetCDF::NC_GLOBAL, "version", "1.0")
    assert_equal "1.0", NetCDF.nc_get_att(@ncid, NetCDF::NC_GLOBAL, "version")
  end

  def test_attribute_generic_get_array
    NetCDF.nc_put_att_string(@ncid, NetCDF::NC_GLOBAL, "tags", ["a", "b"])
    assert_equal ["a", "b"], NetCDF.nc_get_att(@ncid, NetCDF::NC_GLOBAL, "tags")
  end

  def test_smart_put_att_still_makes_nc_char
    # Plain string via the smart generic continues to make an NC_CHAR
    # attribute, per the established convention.
    NetCDF.nc_put_att(@ncid, NetCDF::NC_GLOBAL, "title", "hello")
    assert_equal NetCDF::NC_CHAR, NetCDF.nc_inq_atttype(@ncid, NetCDF::NC_GLOBAL, "title")
  end
  # The put copies each element's text out before releasing the GVL;
  # lengths, empty strings and the NUL check have to survive that.
  def test_put_copies_mixed_lengths
    d = NetCDF.nc_def_dim(@ncid, "n", 4)
    v = NetCDF.nc_def_var(@ncid, "s", NetCDF::NC_STRING, [d])
    NetCDF.nc_enddef(@ncid)
    src = ["", "a", "x" * 1000, "末尾"]
    NetCDF.nc_put_var(@ncid, v, src)
    assert_equal src, NetCDF.nc_get_var(@ncid, v)
    assert_raise(ArgumentError) { NetCDF.nc_put_var(@ncid, v, ["a", "b\0c", "d", "e"]) }
  end

  # --- encoding ------------------------------------------------------------

  # Text read back is tagged UTF-8, so it compares equal to what was
  # written; NC_STRING is UTF-8 by definition, and NC_CHAR attributes are
  # text in practice.
  def test_strings_read_back_as_utf8
    g = NetCDF::NC_GLOBAL
    d = NetCDF.nc_def_dim(@ncid, "n", 2)
    v = NetCDF.nc_def_var(@ncid, "names", NetCDF::NC_STRING, [d])
    c = NetCDF.nc_def_var(@ncid, "flag", NetCDF::NC_CHAR, [])
    NetCDF.nc_put_att(@ncid, g, "title", "温度")
    NetCDF.nc_put_att_string(@ncid, g, "tags", ["気温", "湿度"])
    NetCDF.nc_enddef(@ncid)
    NetCDF.nc_put_var(@ncid, v, ["東京", "大阪"])
    NetCDF.nc_put_var1(@ncid, c, [], "y")

    got = [
      NetCDF.nc_get_att(@ncid, g, "title"),
      *NetCDF.nc_get_att(@ncid, g, "tags"),
      *NetCDF.nc_get_var(@ncid, v),
      NetCDF.nc_get_var1(@ncid, v, [1]),
      NetCDF.nc_get_var1(@ncid, c, []),
    ]
    assert_equal ["温度", "気温", "湿度", "東京", "大阪", "大阪", "y"], got
    got.each { |s| assert_equal Encoding::UTF_8, s.encoding }
  end
  # netCDF names are UTF-8 too.
  def test_names_read_back_as_utf8
    grp = NetCDF.nc_def_grp(@ncid, "観測")
    d   = NetCDF.nc_def_dim(grp, "時刻", 2)
    v   = NetCDF.nc_def_var(grp, "気温", NetCDF::NC_FLOAT, [d])
    NetCDF.nc_put_att(grp, v, "単位", "K")

    got = [
      NetCDF.nc_inq_grpname(grp),
      NetCDF.nc_inq_grpname_full(grp),
      NetCDF.nc_inq_dimname(grp, d),
      NetCDF.nc_inq_varname(grp, v),
      NetCDF.nc_inq_attname(grp, v, 0),
    ]
    assert_equal ["観測", "/観測", "時刻", "気温", "単位"], got
    got.each { |s| assert_equal Encoding::UTF_8, s.encoding }
  end
end
