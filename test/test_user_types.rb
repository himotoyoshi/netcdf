require "test-unit"
require "tmpdir"
require "netcdf"
require_relative "memory_buffer"

class TestUserTypes < Test::Unit::TestCase
  def setup
    @tmp  = Dir.mktmpdir
    @path = File.join(@tmp, "t.nc")
    @ncid = NetCDF.nc_create(@path, NetCDF::NC_CLOBBER | NetCDF::NC_NETCDF4)
  end

  def teardown
    NetCDF.nc_close(@ncid) rescue nil
    FileUtils.remove_entry(@tmp) if File.directory?(@tmp)
  end

  # --- opaque --------------------------------------------------------------

  def test_def_inq_opaque
    t = NetCDF.nc_def_opaque(@ncid, 16, "uuid_t")
    assert_kind_of Integer, t
    name, size = NetCDF.nc_inq_opaque(@ncid, t)
    assert_equal "uuid_t", name
    assert_equal 16,       size

    nm, sz, base, nf, klass = NetCDF.nc_inq_user_type(@ncid, t)
    assert_equal "uuid_t",     nm
    assert_equal 16,           sz
    assert_equal NetCDF::NC_OPAQUE, klass
  end

  def test_inq_type_equal_atomic
    assert_true  NetCDF.nc_inq_type_equal(@ncid, NetCDF::NC_INT, @ncid, NetCDF::NC_INT)
    assert_false NetCDF.nc_inq_type_equal(@ncid, NetCDF::NC_INT, @ncid, NetCDF::NC_FLOAT)
  end

  def test_inq_type_equal_across_files
    t = NetCDF.nc_def_opaque(@ncid, 16, "uuid_t")

    ncid2 = NetCDF.nc_create(File.join(@tmp, "u.nc"), NetCDF::NC_CLOBBER | NetCDF::NC_NETCDF4)
    same  = NetCDF.nc_def_opaque(ncid2, 16, "uuid_t")
    other = NetCDF.nc_def_opaque(ncid2, 8,  "tag_t")
    # libnetcdf compares user types only after they are written out.
    NetCDF.nc_enddef(@ncid)
    NetCDF.nc_enddef(ncid2)
    assert_true  NetCDF.nc_inq_type_equal(@ncid, t, ncid2, same)
    assert_false NetCDF.nc_inq_type_equal(@ncid, t, ncid2, other)
    NetCDF.nc_close(ncid2)
  end

  def test_opaque_var1_round_trip
    t = NetCDF.nc_def_opaque(@ncid, 4, "tag_t")
    d = NetCDF.nc_def_dim(@ncid, "n", 3)
    v = NetCDF.nc_def_var(@ncid, "tags", t, [d])
    NetCDF.nc_enddef(@ncid)

    NetCDF.nc_put_var1(@ncid, v, [0], "ABCD")
    NetCDF.nc_put_var1(@ncid, v, [2], "WXYZ")
    assert_equal "ABCD", NetCDF.nc_get_var1(@ncid, v, [0])
    assert_equal "WXYZ", NetCDF.nc_get_var1(@ncid, v, [2])
  end

  def test_opaque_vara_round_trip
    t = NetCDF.nc_def_opaque(@ncid, 4, "tag_t")
    d = NetCDF.nc_def_dim(@ncid, "n", 3)
    v = NetCDF.nc_def_var(@ncid, "tags", t, [d])
    NetCDF.nc_enddef(@ncid)

    src = MemoryBuffer.new([3], item_size: 4)
    src.write("ABCDEFGHIJKL")
    NetCDF.nc_put_vara(@ncid, v, [0], [3], src)

    out = MemoryBuffer.new([3], item_size: 4)
    NetCDF.nc_get_vara(@ncid, v, [0], [3], out)
    assert_equal "ABCDEFGHIJKL", out.to_s
  end

  def test_opaque_wrong_size_raises
    t = NetCDF.nc_def_opaque(@ncid, 4, "tag_t")
    d = NetCDF.nc_def_dim(@ncid, "n", 1)
    v = NetCDF.nc_def_var(@ncid, "tags", t, [d])
    NetCDF.nc_enddef(@ncid)

    assert_raise(ArgumentError) { NetCDF.nc_put_var1(@ncid, v, [0], "XYZ") }
  end

  # The fill value of a 64-byte opaque would not fit the scalar union
  # nc_inq_var_fill reads into; it must be refused before the call, with
  # the variable's real type in the message.
  def test_inq_var_fill_on_large_opaque_raises
    t = NetCDF.nc_def_opaque(@ncid, 64, "blob_t")
    d = NetCDF.nc_def_dim(@ncid, "n", 1)
    v = NetCDF.nc_def_var(@ncid, "blobs", t, [d])

    e = assert_raise(NotImplementedError) { NetCDF.nc_inq_var_fill(@ncid, v) }
    assert_match(/unsupported nc_type: #{t}\z/, e.message)
  end

  # --- enum ----------------------------------------------------------------

  def test_def_inq_enum
    t = NetCDF.nc_def_enum(@ncid, NetCDF::NC_INT, "Color")
    NetCDF.nc_insert_enum(@ncid, t, "RED",   0)
    NetCDF.nc_insert_enum(@ncid, t, "GREEN", 1)
    NetCDF.nc_insert_enum(@ncid, t, "BLUE",  2)

    name, base, base_size, nmembers = NetCDF.nc_inq_enum(@ncid, t)
    assert_equal "Color",        name
    assert_equal NetCDF::NC_INT, base
    assert_equal 4,              base_size
    assert_equal 3,              nmembers

    assert_equal ["RED",   0], NetCDF.nc_inq_enum_member(@ncid, t, 0)
    assert_equal ["GREEN", 1], NetCDF.nc_inq_enum_member(@ncid, t, 1)

    assert_equal "BLUE", NetCDF.nc_inq_enum_ident(@ncid, t, 2)
    assert_nil           NetCDF.nc_inq_enum_ident(@ncid, t, 99)
  end

  # The lookup takes the value, not its base-type bytes: packed into a
  # long long, NC_BYTE's -1 would read back as 255.
  def test_inq_enum_ident_negative_byte_member
    t = NetCDF.nc_def_enum(@ncid, NetCDF::NC_BYTE, "Flag")
    NetCDF.nc_insert_enum(@ncid, t, "MISSING", -1)
    NetCDF.nc_insert_enum(@ncid, t, "OK",       1)

    assert_equal "MISSING", NetCDF.nc_inq_enum_ident(@ncid, t, -1)
    assert_equal "OK",      NetCDF.nc_inq_enum_ident(@ncid, t, 1)
    assert_nil              NetCDF.nc_inq_enum_ident(@ncid, t, 255)
  end

  def test_inq_enum_ident_large_uint64_member
    t = NetCDF.nc_def_enum(@ncid, NetCDF::NC_UINT64, "Big")
    NetCDF.nc_insert_enum(@ncid, t, "TOP", 2**64 - 1)
    assert_equal "TOP", NetCDF.nc_inq_enum_ident(@ncid, t, 2**64 - 1)
  end

  def test_enum_var_round_trip
    t = NetCDF.nc_def_enum(@ncid, NetCDF::NC_INT, "Color")
    NetCDF.nc_insert_enum(@ncid, t, "RED",   0)
    NetCDF.nc_insert_enum(@ncid, t, "GREEN", 1)
    NetCDF.nc_insert_enum(@ncid, t, "BLUE",  2)

    d = NetCDF.nc_def_dim(@ncid, "n", 4)
    v = NetCDF.nc_def_var(@ncid, "pixels", t, [d])
    NetCDF.nc_enddef(@ncid)

    NetCDF.nc_put_var1(@ncid, v, [0], 0)
    NetCDF.nc_put_var1(@ncid, v, [1], 1)
    NetCDF.nc_put_var1(@ncid, v, [2], 2)
    NetCDF.nc_put_var1(@ncid, v, [3], 1)

    assert_equal 0, NetCDF.nc_get_var1(@ncid, v, [0])
    assert_equal 2, NetCDF.nc_get_var1(@ncid, v, [2])

    # Round-trip through nc_inq_enum_ident for readability
    assert_equal "RED",   NetCDF.nc_inq_enum_ident(@ncid, t, NetCDF.nc_get_var1(@ncid, v, [0]))
    assert_equal "BLUE",  NetCDF.nc_inq_enum_ident(@ncid, t, NetCDF.nc_get_var1(@ncid, v, [2]))
  end

  def test_enum_vara_via_formatted_buffer
    t = NetCDF.nc_def_enum(@ncid, NetCDF::NC_INT, "Color")
    NetCDF.nc_insert_enum(@ncid, t, "RED",   0)
    NetCDF.nc_insert_enum(@ncid, t, "GREEN", 1)
    d = NetCDF.nc_def_dim(@ncid, "n", 3)
    v = NetCDF.nc_def_var(@ncid, "pixels", t, [d])
    NetCDF.nc_enddef(@ncid)

    src = MemoryBuffer.new([3], format: "i")
    src.write([0, 1, 0].pack("l<*"))
    NetCDF.nc_put_vara(@ncid, v, [0], [3], src)

    out = MemoryBuffer.new([3], format: "i")
    NetCDF.nc_get_vara(@ncid, v, [0], [3], out)
    assert_equal [0, 1, 0], out.to_s.unpack("l<*")
  end

  # A formatless buffer for an enum is raw bytes of the base type, so three
  # bytes are too small for three NC_INT elements.
  def test_enum_buffer_sized_by_base_type
    t = NetCDF.nc_def_enum(@ncid, NetCDF::NC_INT, "Color")
    NetCDF.nc_insert_enum(@ncid, t, "RED", 0)
    d = NetCDF.nc_def_dim(@ncid, "n", 3)
    v = NetCDF.nc_def_var(@ncid, "pixels", t, [d])
    NetCDF.nc_enddef(@ncid)

    small = MemoryBuffer.new([3])
    e = assert_raise(ArgumentError) do
      NetCDF.nc_get_vara(@ncid, v, [0], [3], small)
    end
    assert_match(/too small/, e.message)
  end

  # libnetcdf moves an enum's elements as its base type whatever the buffer
  # holds, so a buffer that names another type would be reinterpreted --
  # "q" bytes read as pairs of ints -- and is refused instead.
  def test_enum_buffer_format_must_match_base_type
    t = NetCDF.nc_def_enum(@ncid, NetCDF::NC_INT, "Color")
    NetCDF.nc_insert_enum(@ncid, t, "RED", 0)
    d = NetCDF.nc_def_dim(@ncid, "n", 3)
    v = NetCDF.nc_def_var(@ncid, "pixels", t, [d])
    NetCDF.nc_enddef(@ncid)

    { "q" => :put, "d" => :get, "I" => :get }.each do |fmt, dir|
      buf = MemoryBuffer.new([3], format: fmt)
      e = assert_raise(ArgumentError, "format #{fmt.inspect}") do
        if dir == :put then NetCDF.nc_put_vara(@ncid, v, [0], [3], buf)
        else                NetCDF.nc_get_vara(@ncid, v, [0], [3], buf)
        end
      end
      assert_match(/enum's base type/, e.message)
    end
  end

  # --- vlen (metadata only for now) ---------------------------------------

  def test_def_inq_vlen
    t = NetCDF.nc_def_vlen(@ncid, "row_t", NetCDF::NC_INT)
    name, datum_size, base = NetCDF.nc_inq_vlen(@ncid, t)
    assert_equal "row_t", name
    assert base.is_a?(Integer)
    # datum_size is the sizeof(nc_vlen_t) struct
    assert datum_size > 0
    assert_equal NetCDF::NC_INT, base
  end

  # --- typeid lookup -------------------------------------------------------

  def test_inq_typeid_and_typeids
    o = NetCDF.nc_def_opaque(@ncid, 8, "blob_t")
    e = NetCDF.nc_def_enum(@ncid, NetCDF::NC_INT, "Color")
    assert_equal o, NetCDF.nc_inq_typeid(@ncid, "blob_t")
    assert_equal e, NetCDF.nc_inq_typeid(@ncid, "Color")
    assert_nil      NetCDF.nc_inq_typeid(@ncid, "nope")

    assert_equal [o, e].sort, NetCDF.nc_inq_typeids(@ncid).sort
  end
end
