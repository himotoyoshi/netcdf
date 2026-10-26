require "test-unit"
require "tmpdir"
require "netcdf"
require_relative "memory_buffer"

# libnetcdf is not thread-safe, and array I/O runs with the GVL released,
# so every NetCDF call is serialized on one lock.  Without it the stress
# test below crashes the process within a few hundred iterations.
class TestThreads < Test::Unit::TestCase
  def setup
    @tmp = Dir.mktmpdir
  end

  def teardown
    FileUtils.remove_entry(@tmp) if File.directory?(@tmp)
  end

  # Threads on separate files still share libnetcdf's and HDF5's state.
  def test_concurrent_io_on_separate_files
    errors = Queue.new
    threads = 4.times.map do |t|
      Thread.new do
        nc = NetCDF.nc_create(File.join(@tmp, "t#{t}.nc"),
                              NetCDF::NC_CLOBBER | NetCDF::NC_NETCDF4)
        d = NetCDF.nc_def_dim(nc, "x", 1000)
        v = NetCDF.nc_def_var(nc, "v", NetCDF::NC_DOUBLE, [d])
        NetCDF.nc_enddef(nc)
        src = MemoryBuffer.new([1000], format: "d")
        src.write(([t.to_f] * 1000).pack("d*"))
        dst = MemoryBuffer.new([1000], format: "d")
        500.times do
          NetCDF.nc_put_var(nc, v, src)
          got = NetCDF.nc_get_var(nc, v, dst).to_s.unpack("d*").uniq
          errors << "thread #{t} read #{got.inspect}" unless got == [t.to_f]
          NetCDF.nc_inq_varname(nc, v)
        end
        NetCDF.nc_close(nc)
      rescue => e
        errors << "thread #{t}: #{e.class}: #{e.message}"
      end
    end
    threads.each(&:join)
    assert_true errors.empty?, (errors.pop unless errors.empty?)
  end

  # An interrupt that arrives while libnetcdf runs is raised only after
  # the buffer's MemoryView is released.  Raised as the GVL came back, it
  # skipped the release, and Ruby's table of exported views kept the
  # buffer alive for good.
  def test_interrupt_during_read_releases_buffer
    n = 2_000_000
    nc = NetCDF.nc_create(File.join(@tmp, "i.nc"), NetCDF::NC_CLOBBER | NetCDF::NC_NETCDF4)
    d = NetCDF.nc_def_dim(nc, "x", n)
    v = NetCDF.nc_def_var(nc, "v", NetCDF::NC_DOUBLE, [d])
    # Small compressed chunks, so the read takes long enough to interrupt.
    NetCDF.nc_def_var_chunking(nc, v, NetCDF::NC_CHUNKED, [64])
    NetCDF.nc_def_var_deflate(nc, v, 1, 1, 9)
    NetCDF.nc_enddef(nc)
    src = MemoryBuffer.new([n], format: "d")
    src.write(Array.new(n) { |i| i.to_f }.pack("d*"))
    NetCDF.nc_put_var(nc, v, src)

    refs = ObjectSpace::WeakMap.new
    raised = nil
    # The buffer lives only on threads that are gone by the GC below, so
    # no stale stack slot keeps it alive.
    Thread.new do
      started = Queue.new
      reader = Thread.new do
        Thread.current.report_on_exception = false
        buf = MemoryBuffer.new([n], format: "d")
        refs[:buf] = buf
        started << true
        NetCDF.nc_get_var(nc, v, buf)
      end
      started.pop
      sleep 0.02
      reader.raise(Interrupt)
      begin
        reader.join
      rescue Interrupt => e
        raised = e
      end
    end.join

    assert_kind_of Interrupt, raised
    4.times { GC.start(full_mark: true, immediate_sweep: true) }
    assert_false refs.key?(:buf), "buffer is still exported after the interrupt"

    # The file is still usable.
    got = NetCDF.nc_get_vara(nc, v, [n - 2], [2],
                             MemoryBuffer.new([2], format: "d")).to_s.unpack("d*")
    assert_equal [n - 2.0, n - 1.0], got
    NetCDF.nc_close(nc)
  end

  # A conversion that calls back into NetCDF re-enters the lock.
  def test_reentrant_call_from_conversion
    nc = NetCDF.nc_create(File.join(@tmp, "r.nc"), NetCDF::NC_CLOBBER | NetCDF::NC_NETCDF4)
    d = NetCDF.nc_def_dim(nc, "x", 1)
    v = NetCDF.nc_def_var(nc, "v", NetCDF::NC_DOUBLE, [d])
    NetCDF.nc_enddef(nc)
    NetCDF.nc_put_var1(nc, v, [0], 2.5)

    index = Object.new
    def index.to_int
      NetCDF.nc_inq_libvers
      0
    end
    assert_equal 2.5, NetCDF.nc_get_var1(nc, v, [index])
    NetCDF.nc_close(nc)
  end

  # Including NetCDF still gives private methods, and they are the locked
  # ones.
  def test_included_methods_stay_private
    klass = Class.new do
      include NetCDF
      def version = nc_inq_libvers
    end
    assert_kind_of String, klass.new.version
    assert_false klass.new.respond_to?(:nc_inq_libvers)
    assert_not_equal NetCDF, klass.new.method(:nc_inq_libvers).owner
  end
end
