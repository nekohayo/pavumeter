# $Id$
#
# This file is part of Polypaudio Volume Meter.
#
# Polypaudio Volume Meter is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# Polypaudio Volume Meter is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Polypaudio Volume Meter; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
# USA.

EXTRA_DIST = bootstrap.sh README LICENSE
SUBDIRS=src doc

MAINTAINERCLEANFILES=README
noinst_DATA = README

README:
	rm -f README
	$(MAKE) -C doc README
	cd $(srcdir) && ln -s doc/README README

homepage: all dist
	test -d $$HOME/homepage/private
	mkdir -p $$HOME/homepage/private/projects/pavumeter
	cp *.tar.gz $$HOME/homepage/private/projects/pavumeter
	cp doc/README.html doc/screenshot.png doc/style.css $$HOME/homepage/private/projects/pavumeter
	cp $$HOME/homepage/private/projects/pavumeter/README.html $$HOME/homepage/private/projects/paman/index.html

distcleancheck:
	@:

.PHONY: homepage distcleancheck