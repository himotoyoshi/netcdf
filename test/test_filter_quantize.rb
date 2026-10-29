require "test-unit"
require "tmpdir"
require "netcdf"
require_relative "memory_buffer"

class TestFilterQuantize < Test::Unit::TestCase
  def setup
    @tmp  = Dir.mktmpdir
    @path = File.join(@tmp, "t.nc")
    @ncid = NetCDF.nc_create(@path, NetCDF::NC_CLOBBER | NetCDF::NC_NETCDF4)
    @y    = NetCDF.nc_def_dim(@ncid, "y", 10)
    @x    = NetCDF.nc_def_dim(@ncid, "x", 20)
    @v    = NetCDF.nc_def_var(@ncid, "T", NetCDF::NC_FLOAT, [@y, @x])
    # chunking is required for filters
    NetCDF.nc_def_var_chunking(@ncid, @v, NetCDF::NC_CHUNKED, [10, 20])
  end

  def teardown
    NetCDF.nc_close(@ncid) rescue nil
    FileUtils.remove_entry(@tmp) if File.directory?(@tmp)
  end

  def test_inq_filter_avail_for_deflate
    omit "linked libnetcdf lacks nc_inq_filter_avail" unless NetCDF.respond_to?(:nc_inq_filter_avail)
    assert_equal true, NetCDF.nc_inq_filter_avail(@ncid, 1)
  end

  def test_filter_no_filter_returns_nil
    omit "linked libnetcdf lacks nc_inq_var_filter_info" unless NetCDF.respond_to?(:nc_inq_var_filter_info)
    assert_nil NetCDF.nc_inq_var_filter_info(@ncid, @v, 1)
  end

  def test_zlib_filter_via_def_var_filter
    omit "linked libnetcdf lacks nc_def_var_filter" unless NetCDF.respond_to?(:nc_def_var_filter)
    NetCDF.nc_def_var_filter(@ncid, @v, 1, [4])
    info = NetCDF.nc_inq_var_filter_info(@ncid, @v, 1)
    assert_equal [4], info
    ids = NetCDF.nc_inq_var_filter_ids(@ncid, @v)
    assert_includes ids, 1
  end

  def test_quantize_bitgroom
    omit "linked libnetcdf lacks nc_def_var_quantize" unless NetCDF.respond_to?(:nc_def_var_quantize)
    NetCDF.nc_def_var_quantize(@ncid, @v, NetCDF::NC_QUANTIZE_BITGROOM, 3)
    mode, nsd = NetCDF.nc_inq_var_quantize(@ncid, @v)
    assert_equal NetCDF::NC_QUANTIZE_BITGROOM, mode
    assert_equal 3, nsd
  end

  def test_quantize_off_by_default
    omit "linked libnetcdf lacks nc_inq_var_quantize" unless NetCDF.respond_to?(:nc_inq_var_quantize)
    mode, _nsd = NetCDF.nc_inq_var_quantize(@ncid, @v)
    assert_equal NetCDF::NC_NOQUANTIZE, mode
  end

  # --- compression filters with their own calls --------------------------

  def omit_unless_filter (name, id_const)
    omit "linked libnetcdf lacks nc_def_var_#{name}" unless NetCDF.respond_to?("nc_def_var_#{name}")
    omit "linked libnetcdf has no #{name} filter" unless NetCDF.nc_inq_filter_avail(@ncid, NetCDF.const_get(id_const))
  end

  def test_szip
    omit_unless_filter("szip", :H5Z_FILTER_SZIP)
    NetCDF.nc_def_var_szip(@ncid, @v, NetCDF::NC_SZIP_NN, 32)
    mask, ppb = NetCDF.nc_inq_var_szip(@ncid, @v)
    assert_equal NetCDF::NC_SZIP_NN, mask & NetCDF::NC_SZIP_NN
    assert_equal 32, ppb
    assert_includes NetCDF.nc_inq_var_filter_ids(@ncid, @v), NetCDF::H5Z_FILTER_SZIP
  end

  def test_zstandard
    omit_unless_filter("zstandard", :H5Z_FILTER_ZSTD)
    NetCDF.nc_def_var_zstandard(@ncid, @v, 5)
    assert_equal [true, 5], NetCDF.nc_inq_var_zstandard(@ncid, @v)
    assert_includes NetCDF.nc_inq_var_filter_ids(@ncid, @v), NetCDF::H5Z_FILTER_ZSTD
  end

  def test_bzip2
    omit_unless_filter("bzip2", :H5Z_FILTER_BZIP2)
    NetCDF.nc_def_var_bzip2(@ncid, @v, 9)
    assert_equal [true, 9], NetCDF.nc_inq_var_bzip2(@ncid, @v)
    assert_includes NetCDF.nc_inq_var_filter_ids(@ncid, @v), NetCDF::H5Z_FILTER_BZIP2
  end

  def test_blosc
    omit_unless_filter("blosc", :H5Z_FILTER_BLOSC)
    NetCDF.nc_def_var_blosc(@ncid, @v, NetCDF::BLOSC_LZ4, 5, 0, NetCDF::BLOSC_SHUFFLE)
    has, sub, level, _blocksize, shuffle = NetCDF.nc_inq_var_blosc(@ncid, @v)
    assert_equal [true, NetCDF::BLOSC_LZ4, 5, NetCDF::BLOSC_SHUFFLE], [has, sub, level, shuffle]
    assert_includes NetCDF.nc_inq_var_filter_ids(@ncid, @v), NetCDF::H5Z_FILTER_BLOSC
  end

  # A blosc-less libnetcdf returns NC_NOERR from nc_def_var_blosc and adds
  # nothing; the gem raises instead.
  def test_def_without_the_filter_raises
    tried = 0
    [["szip",      :H5Z_FILTER_SZIP,  [NetCDF::NC_SZIP_NN, 32]],
     ["zstandard", :H5Z_FILTER_ZSTD,  [5]],
     ["bzip2",     :H5Z_FILTER_BZIP2, [9]],
     ["blosc",     :H5Z_FILTER_BLOSC, [NetCDF::BLOSC_LZ4, 5, 0, NetCDF::BLOSC_SHUFFLE]]].each do |name, id, args|
      next unless NetCDF.respond_to?("nc_def_var_#{name}")
      next if NetCDF.nc_inq_filter_avail(@ncid, NetCDF.const_get(id))
      err = assert_raise(NetCDF::Error) do
        NetCDF.send("nc_def_var_#{name}", @ncid, @v, *args)
      end
      assert_equal(-136, err.status)   # NC_ENOFILTER
      assert_not_include NetCDF.nc_inq_var_filter_ids(@ncid, @v), NetCDF.const_get(id)
      tried += 1
    end
    omit "linked libnetcdf has every filter" if tried == 0
  end

  # Also where libnetcdf lacks the filter: a blosc-less build sets no
  # output at all, and the variable must still read as not having it.
  def test_inq_without_the_filter
    assert_equal [0, 0],          NetCDF.nc_inq_var_szip(@ncid, @v)          if NetCDF.respond_to?(:nc_inq_var_szip)
    assert_equal [false, 0],      NetCDF.nc_inq_var_zstandard(@ncid, @v)     if NetCDF.respond_to?(:nc_inq_var_zstandard)
    assert_equal [false, 0],      NetCDF.nc_inq_var_bzip2(@ncid, @v)         if NetCDF.respond_to?(:nc_inq_var_bzip2)
    assert_equal false,           NetCDF.nc_inq_var_blosc(@ncid, @v).first   if NetCDF.respond_to?(:nc_inq_var_blosc)
  end

  def test_zstandard_round_trip
    omit_unless_filter("zstandard", :H5Z_FILTER_ZSTD)
    NetCDF.nc_def_var_zstandard(@ncid, @v, 5)
    NetCDF.nc_enddef(@ncid)
    src = MemoryBuffer.new([10, 20], format: "f")
    src.write((0...200).map { |i| (i / 10).to_f }.pack("f*"))
    NetCDF.nc_put_var(@ncid, @v, src)
    NetCDF.nc_close(@ncid)

    @ncid = NetCDF.nc_open(@path)
    assert_equal [true, 5], NetCDF.nc_inq_var_zstandard(@ncid, @v)
    dst = MemoryBuffer.new([10, 20], format: "f")
    NetCDF.nc_get_var(@ncid, @v, dst)
    assert_equal src.to_s, dst.to_s
  end
end
