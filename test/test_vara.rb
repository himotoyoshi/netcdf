require "test-unit"
require "tmpdir"
require "netcdf"
require_relative "memory_buffer"

# A 1-D view without a shape, as ruby/memory_view.h permits; strides, when
# the buffer has them, are still handed over.
class NullShapeBuffer < MemoryBuffer
  def initialize(shape, strides: nil, **kw)
    super
    @explicit_strides = strides
  end

  def fill_view(view, flags)
    super.tap do
      view.shape = 0
      view.strides = 0 unless @explicit_strides
    end
  end
end

# A producer that ignores RUBY_MEMORY_VIEW_WRITABLE and hands back its
# read-only view anyway.
class IgnoresWritableBuffer < MemoryBuffer
  def fill_view(view, flags) = super(view, flags & ~WRITABLE)
end

# A producer whose release_func refuses.
class UnreleasableBuffer < MemoryBuffer
  register_memory_view(release: -> { false })
end

class TestVara < Test::Unit::TestCase
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

  def test_double_round_trip
    v = NetCDF.nc_def_var(@ncid, "T", NetCDF::NC_DOUBLE, [@yid, @xid])
    NetCDF.nc_enddef(@ncid)

    src = MemoryBuffer.new([3, 4], format: "d")
    values = (1..12).map(&:to_f)
    src.write(values.pack("d*"))
    NetCDF.nc_put_vara(@ncid, v, [0, 0], [3, 4], src)

    dst = MemoryBuffer.new([3, 4], format: "d")
    NetCDF.nc_get_vara(@ncid, v, [0, 0], [3, 4], dst)
    assert_equal values, dst.to_s.unpack("d*")
  end

  # A get, like a put, reads into memory the caller provides; only an
  # NC_STRING variable comes back as objects the gem makes.
  def test_get_without_buffer_raises
    v = NetCDF.nc_def_var(@ncid, "T", NetCDF::NC_DOUBLE, [@yid, @xid])
    NetCDF.nc_enddef(@ncid)

    e = assert_raise(ArgumentError) { NetCDF.nc_get_vara(@ncid, v, [0, 0], [3, 4]) }
    assert_match(/buffer is required/, e.message)
    assert_raise(ArgumentError) { NetCDF.nc_get_var(@ncid, v) }
  end

  def test_subarray
    v = NetCDF.nc_def_var(@ncid, "T", NetCDF::NC_DOUBLE, [@yid, @xid])
    NetCDF.nc_enddef(@ncid)

    patch = MemoryBuffer.new([2, 2], format: "d")
    patch.write([10.0, 20.0, 30.0, 40.0].pack("d*"))
    NetCDF.nc_put_vara(@ncid, v, [1, 1], [2, 2], patch)

    full = MemoryBuffer.new([3, 4], format: "d")
    NetCDF.nc_get_vara(@ncid, v, [0, 0], [3, 4], full)
    out = full.to_s.unpack("d*")
    assert_equal 10.0, out[1*4 + 1]
    assert_equal 20.0, out[1*4 + 2]
    assert_equal 30.0, out[2*4 + 1]
    assert_equal 40.0, out[2*4 + 2]
  end

  def test_int_to_double_var_with_libnetcdf_conversion
    v = NetCDF.nc_def_var(@ncid, "T", NetCDF::NC_DOUBLE, [@yid, @xid])
    NetCDF.nc_enddef(@ncid)

    src = MemoryBuffer.new([3, 4], format: "i")
    src.write((1..12).to_a.pack("l<*"))   # little-endian int32
    NetCDF.nc_put_vara(@ncid, v, [0, 0], [3, 4], src)

    dst = MemoryBuffer.new([3, 4], format: "d")
    NetCDF.nc_get_vara(@ncid, v, [0, 0], [3, 4], dst)
    assert_equal (1..12).map(&:to_f), dst.to_s.unpack("d*")
  end

  def test_readonly_buffer_rejected_on_get
    v = NetCDF.nc_def_var(@ncid, "T", NetCDF::NC_DOUBLE, [@yid, @xid])
    NetCDF.nc_enddef(@ncid)

    ro = MemoryBuffer.new([3, 4], format: "d", readonly: true)
    e = assert_raise(TypeError) do
      NetCDF.nc_get_vara(@ncid, v, [0, 0], [3, 4], ro)
    end
    assert_match(/must provide a writable MemoryView/, e.message)
    assert_empty MemoryBuffer::FIXTURE_ERRORS
  end

  # A producer like numpy refuses a request without strides when its memory
  # is strided.  netcdf asks with strides, so such a buffer is handed over
  # and reported as not row-major, the reason it cannot be used, rather
  # than refused as not writable.
  def test_strict_strided_buffer_reported_as_not_row_major
    v = NetCDF.nc_def_var(@ncid, "T", NetCDF::NC_DOUBLE, [@yid, @xid])
    NetCDF.nc_enddef(@ncid)

    transposed = MemoryBuffer.new([3, 4], format: "d", strides: [8, 24], strict: true)
    e = assert_raise(ArgumentError) do
      NetCDF.nc_get_vara(@ncid, v, [0, 0], [3, 4], transposed)
    end
    assert_match(/row-major/, e.message)
    e = assert_raise(ArgumentError) do
      NetCDF.nc_put_vara(@ncid, v, [0, 0], [3, 4], transposed)
    end
    assert_match(/row-major/, e.message)
    assert_empty MemoryBuffer::FIXTURE_ERRORS
  end

  def test_contiguous_strict_buffer_round_trips
    v = NetCDF.nc_def_var(@ncid, "T", NetCDF::NC_DOUBLE, [@yid, @xid])
    NetCDF.nc_enddef(@ncid)

    src = MemoryBuffer.new([3, 4], format: "d", strict: true)
    src.write((1..12).map(&:to_f).pack("d*"))
    NetCDF.nc_put_vara(@ncid, v, [0, 0], [3, 4], src)
    dst = MemoryBuffer.new([3, 4], format: "d", strict: true)
    NetCDF.nc_get_vara(@ncid, v, [0, 0], [3, 4], dst)
    assert_equal src.to_s, dst.to_s
  end

  def test_buffer_too_small_raises
    v = NetCDF.nc_def_var(@ncid, "T", NetCDF::NC_DOUBLE, [@yid, @xid])
    NetCDF.nc_enddef(@ncid)

    small = MemoryBuffer.new([2, 2], format: "d")
    assert_raise(ArgumentError) do
      NetCDF.nc_get_vara(@ncid, v, [0, 0], [3, 4], small)
    end
  end

  # A format-less buffer is raw bytes of the variable's own type, so its
  # size is measured in doubles here, not in its 1-byte items.
  def test_formatless_buffer_sized_by_variable_type
    v = NetCDF.nc_def_var(@ncid, "T", NetCDF::NC_DOUBLE, [@yid])
    NetCDF.nc_enddef(@ncid)

    assert_raise(ArgumentError) do
      NetCDF.nc_put_vara(@ncid, v, [0], [3], MemoryBuffer.new([3]))
    end

    raw = MemoryBuffer.new([24])
    raw.write([1.0, 2.0, 3.0].pack("d*"))
    NetCDF.nc_put_vara(@ncid, v, [0], [3], raw)
    got = MemoryBuffer.new([24])
    NetCDF.nc_get_var(@ncid, v, got)
    assert_equal [1.0, 2.0, 3.0], got.to_s.unpack("d*")
  end

  # Fiddle::Pointer publishes a byte-array view with NULL shape and
  # strides, which is contiguous by definition.  Up to fiddle 1.1.8 it
  # marks every view read-only, whatever was asked for, so it serves as a
  # source but not as the destination of a get; from 1.1.9 it hands out a
  # writable view when asked for one (ruby/fiddle#215).
  def test_byte_array_view_without_strides
    v = NetCDF.nc_def_var(@ncid, "T", NetCDF::NC_DOUBLE, [@yid])
    NetCDF.nc_enddef(@ncid)

    src = Fiddle::Pointer.malloc(24, Fiddle::RUBY_FREE)
    src[0, 24] = [1.0, 2.0, 3.0].pack("d*")
    NetCDF.nc_put_vara(@ncid, v, [0], [3], src)

    dst = NullShapeBuffer.new([24])
    NetCDF.nc_get_vara(@ncid, v, [0], [3], dst)
    assert_equal [1.0, 2.0, 3.0], dst.to_s.unpack("d*")

    ptr = Fiddle::Pointer.malloc(24, Fiddle::RUBY_FREE)
    if Gem::Version.new(Fiddle::VERSION) >= Gem::Version.new("1.1.9")
      NetCDF.nc_get_vara(@ncid, v, [0], [3], ptr)
      assert_equal [1.0, 2.0, 3.0], ptr[0, 24].unpack("d*")
    else
      e = assert_raise(TypeError) { NetCDF.nc_get_vara(@ncid, v, [0], [3], ptr) }
      assert_match(/read-only/, e.message)
    end
  end

  # libnetcdf does not bound count on a put along an unlimited dimension,
  # so a count whose byte size wraps around must be refused here rather
  # than pass the size check as zero.
  def test_count_too_large_raises
    t = NetCDF.nc_def_dim(@ncid, "t", NetCDF::NC_UNLIMITED)
    v = NetCDF.nc_def_var(@ncid, "T", NetCDF::NC_DOUBLE, [t, @xid])
    NetCDF.nc_enddef(@ncid)
    buf = MemoryBuffer.new([1], format: "d")

    assert_raise(ArgumentError) { NetCDF.nc_put_vara(@ncid, v, [0, 0], [2**61, 1], buf) }
    assert_raise(ArgumentError) { NetCDF.nc_put_vara(@ncid, v, [0, 0], [2**62, 4], buf) }
    assert_raise(ArgumentError) { NetCDF.nc_get_vara(@ncid, v, [0, 0], [2**61, 1], buf) }
    assert_raise(ArgumentError) do
      NetCDF.nc_get_varm(@ncid, v, [0, 0], [2, 2], [1, 1], [2**62, 2**62], buf)
    end
  end

  def test_non_contiguous_1d_rejected
    v = NetCDF.nc_def_var(@ncid, "T", NetCDF::NC_DOUBLE, [@yid])
    NetCDF.nc_enddef(@ncid)

    strided = MemoryBuffer.new([3], format: "d", strides: [16])
    assert_raise(ArgumentError) do
      NetCDF.nc_get_vara(@ncid, v, [0], [3], strided)
    end
  end

  def test_count_length_mismatch_raises
    v = NetCDF.nc_def_var(@ncid, "T", NetCDF::NC_DOUBLE, [@yid, @xid])
    NetCDF.nc_enddef(@ncid)

    buf = MemoryBuffer.new([3, 4], format: "d")
    assert_raise(ArgumentError) do
      NetCDF.nc_get_vara(@ncid, v, [0], [3], buf)
    end
  end

  def test_unlimited_dimension
    uid = NetCDF.nc_def_dim(@ncid, "t", NetCDF::NC_UNLIMITED)
    v = NetCDF.nc_def_var(@ncid, "U", NetCDF::NC_FLOAT, [uid, @xid])
    NetCDF.nc_enddef(@ncid)

    src = MemoryBuffer.new([2, 4], format: "f")
    src.write((1..8).map(&:to_f).pack("f*"))
    NetCDF.nc_put_vara(@ncid, v, [0, 0], [2, 4], src)

    assert_equal 2, NetCDF.nc_inq_dimlen(@ncid, uid)
  end

  def test_char_var_via_formatted_buffer
    v = NetCDF.nc_def_var(@ncid, "S", NetCDF::NC_CHAR, [@yid, @xid])
    NetCDF.nc_enddef(@ncid)

    src = MemoryBuffer.new([3, 4], format: "c")
    src.write("Hello, World")
    NetCDF.nc_put_vara(@ncid, v, [0, 0], [3, 4], src)

    dst = MemoryBuffer.new([3, 4], format: "c")
    NetCDF.nc_get_vara(@ncid, v, [0, 0], [3, 4], dst)
    assert_equal "Hello, World", dst.to_s
  end

  # --- byte-order markers ----------------------------------------------

  NATIVE_ENDIAN_MARKER  = ([1].pack("s") == [1].pack("s<")) ? "<" : ">"
  FOREIGN_ENDIAN_MARKER = ([1].pack("s") == [1].pack("s<")) ? ">" : "<"

  LITTLE_ENDIAN_HOST = ([1].pack("s") == [1].pack("s<"))

  # Ruby spells a byte order as a suffix on an integer ("q<") and as a
  # letter of its own on a float ("e"/"E" little, "g"/"G" big); PEP 3118
  # spells it as a prefix on the whole format ("<d").  Each names the
  # host's order here.
  def test_native_byte_order_accepted
    d = NetCDF.nc_def_var(@ncid, "T", NetCDF::NC_DOUBLE, [@yid, @xid])
    f = NetCDF.nc_def_var(@ncid, "F", NetCDF::NC_FLOAT,  [@yid, @xid])
    q = NetCDF.nc_def_var(@ncid, "Q", NetCDF::NC_INT64,  [@yid, @xid])
    NetCDF.nc_enddef(@ncid)
    values = (1..12).to_a

    native_double = LITTLE_ENDIAN_HOST ? "E" : "G"
    native_float  = LITTLE_ENDIAN_HOST ? "e" : "g"
    [[d, NATIVE_ENDIAN_MARKER + "d", "d"], [d, "=d", "d"], [d, "@d", "d"],
     [d, native_double, "d"], [f, native_float, "f"],
     [q, "q" + NATIVE_ENDIAN_MARKER, "q"], [q, NATIVE_ENDIAN_MARKER + "q", "q"]].each do |var, fmt, pack|
      src = MemoryBuffer.new([3, 4], format: fmt, item_size: [0].pack(pack).bytesize)
      src.write(values.pack("#{pack}*"))
      NetCDF.nc_put_vara(@ncid, var, [0, 0], [3, 4], src)
      dst = MemoryBuffer.new([3, 4], format: fmt, item_size: [0].pack(pack).bytesize)
      NetCDF.nc_get_vara(@ncid, var, [0, 0], [3, 4], dst)
      assert_equal values, dst.to_s.unpack("#{pack}*"), "format #{fmt.inspect}"
    end
  end

  # Without the byte-order check these transfer as if the bytes were native.
  def test_foreign_byte_order_rejected
    v = NetCDF.nc_def_var(@ncid, "T", NetCDF::NC_DOUBLE, [@yid, @xid])
    NetCDF.nc_enddef(@ncid)

    foreign_double = LITTLE_ENDIAN_HOST ? "G" : "E"
    [FOREIGN_ENDIAN_MARKER + "d", "q" + FOREIGN_ENDIAN_MARKER, foreign_double].each do |fmt|
      buf = MemoryBuffer.new([3, 4], format: fmt, item_size: 8)
      e = assert_raise(ArgumentError, "format #{fmt.inspect}") do
        NetCDF.nc_get_vara(@ncid, v, [0, 0], [3, 4], buf)
      end
      assert_match(/not native-endian/, e.message)
    end
  end

  # Ruby takes a suffix byte order on integers only; "d<" is not a format
  # rb_memory_view_item_size_from_format accepts, so it is not one here.
  def test_suffix_byte_order_on_a_float_rejected
    v = NetCDF.nc_def_var(@ncid, "T", NetCDF::NC_DOUBLE, [@yid, @xid])
    NetCDF.nc_enddef(@ncid)

    ["d" + NATIVE_ENDIAN_MARKER, "f" + NATIVE_ENDIAN_MARKER].each do |fmt|
      buf = MemoryBuffer.new([3, 4], format: fmt, item_size: fmt.start_with?("d") ? 8 : 4)
      e = assert_raise(ArgumentError, "format #{fmt.inspect}") do
        NetCDF.nc_get_vara(@ncid, v, [0, 0], [3, 4], buf)
      end
      assert_match(/unsupported buffer format/, e.message)
    end
  end

  # A format naming more than one element would otherwise transfer the
  # buffer as an array of whatever it starts with.  item_size matches the
  # format, so it is the format alone that is refused.
  def test_compound_format_rejected
    v = NetCDF.nc_def_var(@ncid, "T", NetCDF::NC_DOUBLE, [@yid, @xid])
    NetCDF.nc_enddef(@ncid)

    { "dd" => 16, "2d" => 16, "T{d}" => 8 }.each do |fmt, item_size|
      buf = MemoryBuffer.new([3, 4], format: fmt, item_size: item_size)
      assert_raise(ArgumentError, "format #{fmt.inspect}") do
        NetCDF.nc_get_vara(@ncid, v, [0, 0], [3, 4], buf)
      end
    end
  end

  # --- what the format names ---------------------------------------------

  # A format that disagrees with the buffer's own item_size -- PEP's 64-bit
  # "l" arriving untranslated -- would be misread, so it is refused.
  def test_format_disagreeing_with_item_size_rejected
    v = NetCDF.nc_def_var(@ncid, "N", NetCDF::NC_INT64, [@yid])
    NetCDF.nc_enddef(@ncid)

    buf = MemoryBuffer.new([3], format: "l", item_size: 8)
    e = assert_raise(ArgumentError) { NetCDF.nc_get_vara(@ncid, v, [0], [3], buf) }
    assert_match(/implies 4-byte elements but item_size is 8/, e.message)
  end

  # "l"/"L" are 32 bits in Ruby's vocabulary, and PEP 3118's "b"/"B"/"h"/"H"
  # name the 1- and 2-byte integers.  (In Ruby's they would be bit- and
  # hex-strings, so the item sizes are given rather than packed for.)
  def test_ruby_l_and_pep_spellings_read_by_width
    v = NetCDF.nc_def_var(@ncid, "N", NetCDF::NC_INT, [@yid])
    NetCDF.nc_enddef(@ncid)
    values = [-1, 2, 100]
    NetCDF.nc_put_vara(@ncid, v, [0], [3], MemoryBuffer.new([3], format: "i").write(values.pack("l*")))

    { "l" => [4, "l*"], "h" => [2, "s*"], "b" => [1, "c*"] }.each do |fmt, (isz, pack)|
      dst = MemoryBuffer.new([3], format: fmt, item_size: isz)
      NetCDF.nc_get_vara(@ncid, v, [0], [3], dst)
      assert_equal values, dst.to_s.unpack(pack), "format #{fmt.inspect}"
    end
    { "L" => [4, "L*"], "H" => [2, "S*"], "B" => [1, "C*"] }.each do |fmt, (isz, pack)|
      src = MemoryBuffer.new([3], format: fmt, item_size: isz).write([1, 2, 100].pack(pack))
      NetCDF.nc_put_vara(@ncid, v, [0], [3], src)
      dst = MemoryBuffer.new([3], format: "i")
      NetCDF.nc_get_vara(@ncid, v, [0], [3], dst)
      assert_equal [1, 2, 100], dst.to_s.unpack("l*"), "format #{fmt.inspect}"
    end
  end

  def test_unsupported_format_rejected
    v = NetCDF.nc_def_var(@ncid, "T", NetCDF::NC_DOUBLE, [@yid])
    NetCDF.nc_enddef(@ncid)

    buf = MemoryBuffer.new([3], format: "?", item_size: 1)
    e = assert_raise(ArgumentError) { NetCDF.nc_get_vara(@ncid, v, [0], [3], buf) }
    assert_match(/unsupported buffer format/, e.message)
  end

  # NC_CHAR moves bytes: a format must name a 1-byte element and agree with
  # the buffer's item_size, like any other.
  def test_char_var_buffer_format_checked
    v = NetCDF.nc_def_var(@ncid, "S", NetCDF::NC_CHAR, [@yid])
    NetCDF.nc_enddef(@ncid)

    [["s", 1, /implies 2-byte elements/], ["d" + FOREIGN_ENDIAN_MARKER, 1, /unsupported/],
     [FOREIGN_ENDIAN_MARKER + "d", 1, /not native-endian/], [nil, 2, /1-byte-element/]].each do |fmt, isz, msg|
      buf = MemoryBuffer.new([3], format: fmt, item_size: isz)
      e = assert_raise(ArgumentError, "format #{fmt.inspect}") do
        NetCDF.nc_put_vara(@ncid, v, [0], [3], buf)
      end
      assert_match(msg, e.message)
    end
    NetCDF.nc_put_vara(@ncid, v, [0], [3], MemoryBuffer.new([3], format: "C").write("abc"))
    dst = MemoryBuffer.new([3], format: "B", item_size: 1)
    NetCDF.nc_get_vara(@ncid, v, [0], [3], dst)
    assert_equal "abc", dst.to_s
  end

  # --- producers that do not do what was asked ---------------------------

  # ruby/memory_view.h lets a 1-D view leave shape NULL and still give
  # strides.  Gapped ones would have the bytes land packed, in the wrong
  # places of the view.
  def test_null_shape_with_gapped_strides_rejected
    v = NetCDF.nc_def_var(@ncid, "T", NetCDF::NC_DOUBLE, [@yid])
    NetCDF.nc_enddef(@ncid)

    gapped = NullShapeBuffer.new([3], format: "d", strides: [16])
    e = assert_raise(ArgumentError) { NetCDF.nc_get_vara(@ncid, v, [0], [3], gapped) }
    assert_match(/row-major/, e.message)

    packed = NullShapeBuffer.new([3], format: "d", strides: [8])
    packed.write([1.0, 2.0, 3.0].pack("d*"))
    NetCDF.nc_put_vara(@ncid, v, [0], [3], packed)
  end

  # rb_memory_view_get does not hold a producer to the request for a
  # writable view; one that hands back a read-only view is refused rather
  # than written into.
  def test_read_only_view_from_a_producer_ignoring_writable_rejected
    v = NetCDF.nc_def_var(@ncid, "T", NetCDF::NC_DOUBLE, [@yid])
    NetCDF.nc_enddef(@ncid)

    ro = IgnoresWritableBuffer.new([3], format: "d", readonly: true)
    e = assert_raise(TypeError) { NetCDF.nc_get_vara(@ncid, v, [0], [3], ro) }
    assert_match(/read-only/, e.message)
    assert_equal [0.0, 0.0, 0.0], ro.to_s.unpack("d*")
    NetCDF.nc_put_vara(@ncid, v, [0], [3], ro)    # still fine as a source
  end

  # A producer whose release_func refuses keeps its buffer exported for
  # good; the call says so instead of returning as if it had been released.
  def test_refused_release_reported
    v = NetCDF.nc_def_var(@ncid, "T", NetCDF::NC_DOUBLE, [@yid])
    NetCDF.nc_enddef(@ncid)

    buf = UnreleasableBuffer.new([3], format: "d")
    e = assert_raise(RuntimeError) { NetCDF.nc_put_vara(@ncid, v, [0], [3], buf) }
    assert_match(/refused to release/, e.message)
  end

  def test_string_buffer_no_longer_accepted
    v = NetCDF.nc_def_var(@ncid, "T", NetCDF::NC_DOUBLE, [@yid, @xid])
    NetCDF.nc_enddef(@ncid)
    bytes = (1..12).map(&:to_f).pack("d*")
    assert_raise(TypeError) do
      NetCDF.nc_put_vara(@ncid, v, [0, 0], [3, 4], bytes)
    end
  end
end
