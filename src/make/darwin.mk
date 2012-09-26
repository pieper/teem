#
# Teem: Tools to process and visualize scientific data and images              
# Copyright (C) 2008, 2007, 2006, 2005  Gordon Kindlmann
# Copyright (C) 2004, 2003, 2002, 2001, 2000, 1999, 1998  University of Utah
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# (LGPL) as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
# The terms of redistributing and/or modifying this software also
# include exceptions to the LGPL that facilitate static linking.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this library; if not, write to Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
#


AR = libtool
ARFLAGS = -static -o
RANLIB = ranlib

#LD = clang
#CC = clang
#OPT_CFLAG ?= -O2 -fstrict-aliasing -fcatch-undefined-behavior

#LD = clang
#CC = clang
#LD = gcc
#CC = gcc
LD = colorgcc
CC = colorgcc
# OPT_CFLAG ?= -O3 -fast
# see http://www.gnu.org/software/gsl/manual/html_node/GCC-warning-options-for-numerical-programs.html
#
# PUT BACK:
# -Wconversion 
#
# Not useful given the structure of Teem: 
# -Wmissing-prototypes, -Wmissing-declarations wants prototypes even for local static functions
# -Wbad-function-cast doesn't like cast the result of a function (not the function pointer)
# -Wmissing-format-attribute could be interesting, but moot once a printf replacement is in place
OPT_CFLAG ?= -O2 -ansi -pedantic \
  -W -Wall -Wextra -Wno-long-long -Wno-overlength-strings \
  -fstrict-aliasing -Wstrict-aliasing=9 -fstrict-overflow -Wstrict-overflow=9 \
  -Wpointer-arith -Wcast-qual -Wcast-align -ftrapv \
  -Wstrict-prototypes -Wshadow -Wwrite-strings -fshort-enums -fno-common -Wnested-externs \
  -Wmissing-field-initializers -Wunreachable-code 
STATIC_CFLAG = -Wl,-prebind
SHARED_CFLAG = -fPIC
SHARED_LDFLAG = -dynamic -dynamiclib -fno-common
SHARED_INSTALL_NAME = -install_name

ARCH_CFLAG = -W -Wall -Wextra
ARCH_LDFLAG =

ifeq ($(SUBARCH),64)
# -Wconversion 
  ARCH_CFLAG = -arch x86_64
else
  ifeq ($(SUBARCH),32)
  ARCH_CFLAG = -W -Wall -arch i386
  else
    $(error darwin sub-architecture "$(SUBARCH)" not recognized)
  endif
endif

TEEM_ENDIAN = 1234
TEEM_QNANHIBIT = 1
TEEM_DIO = 0

TEEM_ZLIB.IPATH ?=
TEEM_ZLIB.LPATH ?=

TEEM_BZIP2.IPATH ?=
TEEM_BZIP2.LPATH ?=
