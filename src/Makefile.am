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

desktopdir = $(datadir)/applications

bin_PROGRAMS=pavumeter

pavumeter_SOURCES=vumeter.cc

pavumeter_LDADD=$(AM_LDADD) $(GUILIBS_LIBS) $(POLYP_LIBS)
pavumeter_CXXFLAGS=$(AM_CXXFLAGS) $(GUILIBS_CFLAGS) $(POLYP_CFLAGS) 

dist_desktop_DATA=pavumeter.desktop
