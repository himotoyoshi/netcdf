require "test-unit"
require "tmpdir"
require "netcdf"

class TestGroups < Test::Unit::TestCase
  def setup
    @tmp  = Dir.mktmpdir
    @path = File.join(@tmp, "t.nc")
    @ncid = NetCDF.nc_create(@path, NetCDF::NC_CLOBBER | NetCDF::NC_NETCDF4)
  end

  def teardown
    NetCDF.nc_close(@ncid) rescue nil
    FileUtils.remove_entry(@tmp) if File.directory?(@tmp)
  end

  def test_def_grp_and_inq
    g1 = NetCDF.nc_def_grp(@ncid, "alpha")
    g2 = NetCDF.nc_def_grp(@ncid, "beta")
    assert_equal [g1, g2].sort, NetCDF.nc_inq_grps(@ncid).sort
  end

  def test_grpname
    g = NetCDF.nc_def_grp(@ncid, "alpha")
    assert_equal "alpha", NetCDF.nc_inq_grpname(g)
    assert_equal "/alpha", NetCDF.nc_inq_grpname_full(g)
    assert_equal "/",      NetCDF.nc_inq_grpname_full(@ncid)
  end

  def test_grp_parent
    g = NetCDF.nc_def_grp(@ncid, "alpha")
    gg = NetCDF.nc_def_grp(g, "beta")
    assert_equal g,    NetCDF.nc_inq_grp_parent(gg)
    assert_equal @ncid, NetCDF.nc_inq_grp_parent(g)
    assert_nil          NetCDF.nc_inq_grp_parent(@ncid)
  end

  def test_inq_ncid
    g = NetCDF.nc_def_grp(@ncid, "alpha")
    assert_equal g, NetCDF.nc_inq_ncid(@ncid, "alpha")
    assert_nil      NetCDF.nc_inq_ncid(@ncid, "nope")
  end

  def test_inq_grp_full_ncid
    g  = NetCDF.nc_def_grp(@ncid, "alpha")
    gg = NetCDF.nc_def_grp(g, "beta")
    assert_equal gg, NetCDF.nc_inq_grp_full_ncid(@ncid, "/alpha/beta")
    assert_nil       NetCDF.nc_inq_grp_full_ncid(@ncid, "/alpha/missing")
  end

  def test_rename_grp
    g = NetCDF.nc_def_grp(@ncid, "alpha")
    NetCDF.nc_rename_grp(g, "renamed")
    assert_equal "renamed", NetCDF.nc_inq_grpname(g)
    assert_nil              NetCDF.nc_inq_ncid(@ncid, "alpha")
    assert_equal g,         NetCDF.nc_inq_ncid(@ncid, "renamed")
  end

  def test_group_owns_dims_and_vars
    g = NetCDF.nc_def_grp(@ncid, "alpha")
    d_root  = NetCDF.nc_def_dim(@ncid, "x", 10)
    d_inner = NetCDF.nc_def_dim(g, "y", 5)
    v_root  = NetCDF.nc_def_var(@ncid, "rv", NetCDF::NC_DOUBLE, [d_root])
    v_inner = NetCDF.nc_def_var(g, "iv", NetCDF::NC_INT, [d_inner])

    assert_equal [d_root],  NetCDF.nc_inq_dimids(@ncid)
    assert_equal [d_inner], NetCDF.nc_inq_dimids(g)

    # include_parents=true sees the parent's dimension too
    assert_equal [d_inner, d_root].sort, NetCDF.nc_inq_dimids(g, true).sort

    assert_equal [v_root],  NetCDF.nc_inq_varids(@ncid)
    assert_equal [v_inner], NetCDF.nc_inq_varids(g)
  end

  def test_group_attributes
    g = NetCDF.nc_def_grp(@ncid, "alpha")
    NetCDF.nc_put_att(g, NetCDF::NC_GLOBAL, "title", "inner group")
    assert_equal "inner group", NetCDF.nc_get_att(g, NetCDF::NC_GLOBAL, "title")
  end
end
