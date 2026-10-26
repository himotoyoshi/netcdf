require "fiddle"
require "fiddle/import"

# The buffer the suite reads into and writes from: a block of memory that
# publishes itself through the MemoryView protocol, written with nothing
# but the standard library.  Fiddle registers the class as a producer with
# rb_memory_view_register and answers rb_memory_view_get from a Ruby
# closure, so a test can hand netcdf any format, item size, shape, strides
# or writability it wants to exercise -- without depending on an array
# library for it.
#
#   buf = MemoryBuffer.new([3, 4], format: "d")
#   buf.write((1..12).map(&:to_f).pack("d*"))
#   NetCDF.nc_put_vara(ncid, varid, [0, 0], [3, 4], buf)
#   buf.to_s.unpack("d*")
#
# A buffer with no format is raw bytes (item_size 1 unless given), as a
# Fiddle::Pointer is.  A frozen buffer, or one made with readonly: true,
# refuses a writable view.  One made with strict: true refuses, as numpy
# does, a request that does not take strides when its own strides are not
# row-major.
class MemoryBuffer
  # rb_memory_view_t and rb_memory_view_entry_t, from ruby/memory_view.h.
  # Fiddle's struct parser has no bool; readonly is a one-byte char in the
  # same place.
  module C
    extend Fiddle::Importer
    dlload Fiddle::Handle::DEFAULT

    View = struct([
      "uintptr_t obj", "void *data", "ssize_t byte_size", "char readonly",
      "void *format", "ssize_t item_size",
      "void *components", "size_t length",
      "ssize_t ndim", "void *shape", "void *strides", "void *sub_offsets",
      "void *private_data", "void *entry",
    ])
    Entry = struct(["void *get_func", "void *release_func", "void *available_p_func"])

    extern "int rb_memory_view_register(uintptr_t, void *)"
  end

  WRITABLE = 1    # RUBY_MEMORY_VIEW_WRITABLE
  STRIDES  = 12   # RUBY_MEMORY_VIEW_STRIDES

  attr_reader :shape, :format, :item_size, :strides, :byte_size

  def initialize(shape, format: nil, item_size: nil, strides: nil, readonly: false, strict: false)
    @shape     = shape.dup.freeze
    @format    = format&.dup&.freeze
    @item_size = item_size || (format ? [0].pack(format).bytesize : 1)
    @strides   = strides&.dup&.freeze
    @readonly  = readonly
    @strict    = strict
    # A strided view spans from its first element to the end of its last.
    @byte_size =
      if @shape.include?(0) then 0
      elsif @strides then @shape.zip(@strides).sum { |n, s| (n - 1) * s } + @item_size
      else @shape.inject(1, :*) * @item_size
      end
    @data        = c_bytes("\0".b * @byte_size)
    @format_c    = @format && c_bytes(@format + "\0")
    @shape_c     = c_bytes(@shape.pack("q*"))
    @strides_c   = c_bytes(row_major_strides.pack("q*")) unless @strides
    @strides_c ||= c_bytes(@strides.pack("q*"))
  end

  def readonly? = @readonly || frozen?

  def write(bytes)
    raise ArgumentError, "#{bytes.bytesize} bytes do not fit in #{@byte_size}" if bytes.bytesize > @byte_size
    @data[0, bytes.bytesize] = bytes
    self
  end

  def to_s = @data.to_str(@byte_size)

  def ==(other)
    other.is_a?(MemoryBuffer) && [shape, format, item_size, to_s] ==
      [other.shape, other.format, other.item_size, other.to_s]
  end

  # @api private -- what the get closure fills a view with.
  def fill_view(view, flags)
    return false if (flags & WRITABLE) != 0 && readonly?
    return false if @strict && (flags & STRIDES) != STRIDES && !row_major?
    view.data        = @data
    view.byte_size   = @byte_size
    view.readonly    = readonly? ? 1 : 0
    view.format      = @format_c || 0
    view.item_size   = @item_size
    view.components  = 0
    view.length      = 0
    view.ndim        = @shape.size
    view.shape       = @shape_c
    view.strides     = @strides_c
    view.sub_offsets = 0
    view.private_data = 0
    true
  end

  private

  def row_major? = @strides.nil? || @strides == row_major_strides

  def row_major_strides
    @shape.reverse.inject([[], @item_size]) { |(acc, step), n| [[step] + acc, step * n] }.first
  end

  # A malloc'd copy of bytes, so the address stays put whatever GC does to
  # Strings; freed with the buffer.
  def c_bytes(bytes)
    ptr = Fiddle::Pointer.malloc([bytes.bytesize, 1].max, Fiddle::RUBY_FREE)
    ptr[0, bytes.bytesize] = bytes unless bytes.empty?
    ptr
  end

  # Exceptions raised inside the get closure: a bug in the fixture, which
  # would otherwise look like a producer refusing the view.  A test that
  # expects a refusal checks that this stayed empty.
  FIXTURE_ERRORS = []

  # The closures and the entries are kept in constants: libruby holds
  # pointers to them for the life of the process.  A closure must not raise
  # into C -- an exception would unwind the consumer's C frame past its
  # cleanup -- so the arguments arrive as plain integers (a pointer argument
  # is converted by calling Ruby code, where a Thread#raise could land),
  # interrupts are deferred while the view is filled, and an error is
  # recorded in FIXTURE_ERRORS and turns into a refused view.  A deferred
  # interrupt is still raised as the closure returns, before the consumer
  # holds the view; nothing is registered or allocated by then.
  GET = Fiddle::Closure::BlockCaller.new(
    Fiddle::TYPE_BOOL, [Fiddle::TYPE_UINTPTR_T, Fiddle::TYPE_UINTPTR_T, Fiddle::TYPE_INT]
  ) do |obj, view_addr, flags|
    Thread.handle_interrupt(Object => :never) do
      view = C::View.new(view_addr)
      view.obj = obj
      Fiddle.dlunwrap(obj).fill_view(view, flags)
    rescue Exception => e
      FIXTURE_ERRORS << e
      warn "MemoryBuffer fixture error: #{e.class}: #{e.message}"
      false
    end
  end
  AVAILABLE = Fiddle::Closure::BlockCaller.new(
    Fiddle::TYPE_BOOL, [Fiddle::TYPE_UINTPTR_T]
  ) { |_obj| true }

  # Registers the class it is called in as a producer.  There is no release
  # closure by default: nothing needs undoing, and a consumer releases its
  # view in its cleanup, often with an interrupt pending from the GVL-free
  # call it just made -- which a Ruby block would raise on entry, unwinding
  # past the rest of that cleanup.  libruby skips a NULL release_func.  A
  # test that needs one passes it as `release:`, a block returning what
  # release_func returns.
  def self.register_memory_view(release: nil)
    entry = C::Entry.malloc(Fiddle::RUBY_FREE)
    entry.get_func         = GET
    entry.available_p_func = AVAILABLE
    if release
      closure = Fiddle::Closure::BlockCaller.new(
        Fiddle::TYPE_BOOL, [Fiddle::TYPE_UINTPTR_T, Fiddle::TYPE_UINTPTR_T]
      ) { |_obj, _view| release.call }
      entry.release_func = closure
    else
      entry.release_func = 0
    end
    const_set(:MEMORY_VIEW_ENTRY, [entry, closure])
    C.rb_memory_view_register(Fiddle.dlwrap(self), entry)
  end

  register_memory_view
end
