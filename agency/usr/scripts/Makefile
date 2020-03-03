###
# scripts contains sources for various helper programs used throughout
# the kernel for the build process.
# ---------------------------------------------------------------------------
# kallsyms:      Find all symbols in vmlinux
# pnmttologo:    Convert pnm files to logo files
# conmakehash:   Create chartable
# conmakehash:	 Create arrays for initializing the kernel console tables
# docproc:       Used in Documentation/DocBook

HOST_EXTRACFLAGS += -I$(srctree)/tools/include

HOSTCFLAGS_sortextable.o = -I$(srctree)/tools/include
HOSTCFLAGS_asn1_compiler.o = -I$(srctree)/include

always		:= $(hostprogs-y)  

# The following hostprogs-y programs are only build on demand
hostprogs-y += unifdef docproc

# These targets are used internally to avoid "is up to date" messages
PHONY += build_unifdef build_docproc
build_unifdef: $(obj)/unifdef
	@:
build_docproc: $(obj)/docproc
	@:

 
# Let clean descend into subdirs
subdir-	+= basic kconfig package
