CONF ?= Debug

ifndef OPSYS
  OPSYS   := $(shell uname -o 2>/dev/null || uname -s)
  ifeq ($(OPSYS),Linux)
    OPSYS := GNU/Linux
  endif
endif
ifndef MACHINE
  MACHINE := $(shell uname -m)
  ifeq ($(MACHINE),armv6l)
    MACHINE := arm
  endif
  ifeq ($(MACHINE),armv7l)
    MACHINE := arm
  endif
  ifeq ($(MACHINE),ppc64)
    MACHINE := powerpc
  endif
  ifeq ($(MACHINE),aarch64)
    MACHINE := a64
  endif
endif

INCDIRS = . system/$(OPSYS) machine/$(MACHINE)
SRCDIRS = . system/$(OPSYS)/tcf machine/$(MACHINE)/tcf tcf tcf/framework tcf/services tcf/main tcf/http
BINDIR  = obj/$(OPSYS)/$(MACHINE)/$(CONF)

ifndef MKDIR
MKDIR   = mkdir -p $(1)
endif
ifndef RMDIR
RMDIR   = rm -rf $(1)
endif

ifeq ($(CONF),Debug)
  OPTS += -g
else
  OPTS += -O3 -DNDEBUG
endif

ifeq ($(OPSYS),Windows)
  CC = @$(TCF_AGENT_DIR)/bin/mcc -p $(BINDIR)/agent.pdb -m $(MACHINE)
  AR = @$(TCF_AGENT_DIR)/bin/mlb -m $(MACHINE)
  AR_FLAGS =
  AR_OUT_F =
  EXTOBJ = .obj
  EXTLIB = .lib
  EXTEXE = .exe
  LIBS = shell32.lib advapi32.lib Iphlpapi.lib WS2_32.lib version.lib psapi.lib
  EXPORT_DYNAMIC =
endif

ifeq ($(OPSYS),Cygwin)
  LIBS = -lws2_32 -liphlpapi -lpsapi
endif

ifeq ($(OPSYS),Msys)
  OPTS += -mwin32
  LIBS = -lws2_32 -liphlpapi -lpsapi
endif

ifeq ($(OPSYS),MinGW)
  OPTS += -mwin32
  EXTOBJ = .obj
  EXTEXE = .exe
  LIBS = -lws2_32 -liphlpapi -lpsapi
  MKDIR = if not exist $(subst /,\,$(1)) mkdir $(subst /,\,$(1))
  RMDIR = if exist $(subst /,\,$(1)) rmdir /s /q $(subst /,\,$(1))
endif

ifeq ($(OPSYS),Darwin)
  LIBS = -lpthread
  RANLIB = ranlib $@
endif

ifeq ($(OPSYS),SunOS)
  LIBS = -lpthread -lsocket -lnsl -lrt
  RANLIB = ranlib $@
endif

ifeq ($(OPSYS),GNU/Linux)
  ifeq ($(NO_SSL),)
    LIBS = -lpthread -lssl -lcrypto -lrt
  else
    LIBS = -lpthread -lrt
    OPTS += -DENABLE_SSL=0
  endif
  ifeq ($(NO_UUID),)
    LIBS += -luuid
  else
    OPTS += -DUSE_uuid_generate=0
  endif
  OPTS += -DENABLE_arch_$(shell uname -m)
endif

ifneq ($(OPSYS),Windows)
  # Solaris: The procfs used in sysmon.c only works without largefile support
  ifneq ($(OPSYS),SunOS)
    OPTS += -D_FILE_OFFSET_BITS=64
  endif
  OPTS += -Wall
endif

# Compiler flags definition

CC		?= gcc
OUT_OBJ_F	?= -o # with a trailing space
NO_LINK_F	?= -c

# Linker definition and flags

LINK		?= $(CC)
LINK_FLAGS	?= $(LDFLAGS) $(CFLAGS)
LINK_OUT_F	?= $(OUT_OBJ_F)

# Archiver definition and flags

AR		?= ar
AR_FLAGS	?= -rc

EXTOBJ ?= .o
EXTLIB ?= .a
EXTEXE ?=
EXPORT_DYNAMIC ?= -rdynamic
LIBS ?= -lpthread -lssl -lrt

ifdef PATH_Plugins
  OPTS += $(EXPORT_DYNAMIC) -DPATH_Plugins="$(PATH_Plugins)"
  LIBS += -ldl
endif

ifdef OpenSSL
  OPTS += -DPATH_OpenSSL="$(OpenSSL)"
  ifeq ($(OPSYS),Windows)
    OPTS += "-I$(OpenSSL)/inc32"
    ifeq ($(CONF),Debug)
      LIBSSL = $(OpenSSL)/out32.dbg/ssleay32.lib
      LIBCRYPTO = $(OpenSSL)/out32.dbg/libeay32.lib
    else
      LIBSSL = $(OpenSSL)/out32/ssleay32.lib
      LIBCRYPTO = $(OpenSSL)/out32/libeay32.lib
    endif
    LIBS += User32.lib Gdi32.lib
  else
    OPTS += "-I$(OpenSSL)/include"
    LIBSSL = $(OpenSSL)/libssl.a
    LIBCRYPTO = $(OpenSSL)/libcrypto.a
  endif
  LIBS += $(LIBSSL) $(LIBCRYPTO)
endif

ifeq ($(OPSYS),MinGW)
  VERSION := 1.7.0
else
  VERSION := $(shell grep "%define version " $(TCF_AGENT_DIR)/tcf/main/tcf-agent.spec | sed -e "s/%define version //")
endif

INSTALLROOT ?= /tmp
INCLUDE = /usr/include
SBIN = /usr/sbin
INIT = /etc/init.d

HFILES = $(foreach dir,$(SRCDIRS),$(wildcard $(TCF_AGENT_DIR)/$(dir)/*.h))
CFILES = $(foreach fnm,$(foreach dir,$(SRCDIRS),$(wildcard $(TCF_AGENT_DIR)/$(dir)/*.c)),$(subst ^$(TCF_AGENT_DIR)/,,^$(fnm)))
OFILES = $(addprefix $(BINDIR)/,$(sort $(addsuffix $(EXTOBJ),$(basename $(filter-out tcf/main/main%,$(CFILES))))))
EXECS  = $(addprefix $(BINDIR)/,agent$(EXTEXE) client$(EXTEXE) tcfreg$(EXTEXE) valueadd$(EXTEXE) tcflog$(EXTEXE))

ifeq ($(OPSYS),Cygwin)
  CFILES += system/Windows/tcf/pthreads-win32.c
  CFILES += system/Windows/tcf/context-win32.c
endif

ifeq ($(OPSYS),Msys)
  CFILES += system/Windows/tcf/pthreads-win32.c
  CFILES += system/Windows/tcf/context-win32.c
endif

ifeq ($(OPSYS),MinGW)
  CFILES += system/Windows/tcf/pthreads-win32.c
  CFILES += system/Windows/tcf/context-win32.c
endif

ifdef SERVICES
  OPTS += $(shell $(TCF_AGENT_DIR)/bin/services-to-cflags $(SERVICES))
endif

OPTS += $(foreach dir,$(INCDIRS),-I$(TCF_AGENT_DIR)/$(dir))

ifeq ($(OPSYS),GNU/Linux)
  CHECK_C_LIB := $(shell $(TCF_AGENT_DIR)/bin/check-c-lib $(CC) $(CFLAGS) $(OPTS))
  OPTS += $(CHECK_C_LIB)
endif
