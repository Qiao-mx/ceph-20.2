## Generated for RGW VinylCache VMOD
## Based on vmodtool.py --boilerplate pattern

noinst_LTLIBRARIES += libvmod_rgw_vinyl.la
dist_noinst_DATA += vcc_vmod.vcc

libvmod_rgw_vinyl_la_SOURCES = \
	vmod.c

libvmod_rgw_vinyl_la_CFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/bin/vinyld \
	-I$(top_builddir)/include

vmodtoolargs_rgw_vinyl ?= --strict --boilerplate -o vcc_rgw_vinyl_if
vmod_rgw_vinyl_symbols_regex ?= Vmod_rgw_vinyl_Data

libvmod_rgw_vinyl_la_LDFLAGS = \
	-export-symbols-regex $(vmod_rgw_vinyl_symbols_regex) \
	$(AM_LDFLAGS) \
	$(VMOD_LDFLAGS) \
	-rpath $(vmoddir)

nodist_libvmod_rgw_vinyl_la_SOURCES = vcc_rgw_vinyl_if.c vcc_rgw_vinyl_if.h

EXTRA_libvmod_rgw_vinyl_la_DEPENDENCIES = $(nodist_libvmod_rgw_vinyl_la_SOURCES)

EXTRA_DIST += Makefile.am.rgw_vinyl

$(libvmod_rgw_vinyl_la_OBJECTS): vcc_rgw_vinyl_if.h

vcc_rgw_vinyl_if.h vmod_rgw_vinyl.rst vmod_rgw_vinyl.man.rst: vcc_rgw_vinyl_if.c

# A doc-change will not update mtime on the .h and .c files, so a
# touch(1) is necessary to signal that vmodtool was in fact run.
vcc_rgw_vinyl_if.c: $(VMODTOOL) vcc_vmod.vcc
	@PYTHON@ $(VMODTOOL) $(vmodtoolargs_rgw_vinyl) `test -f 'vcc_vmod.vcc' || echo '$(srcdir)/'`vcc_vmod.vcc
	touch vcc_rgw_vinyl_if.c

clean-local: clean-vmod-rgw_vinyl

clean-vmod-rgw_vinyl:
	rm -f $(nodist_libvmod_rgw_vinyl_la_SOURCES)
	rm -f vmod_rgw_vinyl.rst vmod_rgw_vinyl.man.rst