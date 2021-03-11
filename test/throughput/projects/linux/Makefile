include _generic-header.inc
include _libs-path.inc


DEFINES :=\
	$(foreach define,$(DEFINES), -D$(define))
	
INCLUDES:= \
	$(foreach include,$(LIB_INCLUDE_PATH_ALL_LIBS), -I"$(include)") \
	


LINK_LIBS := $(LINK_CXX_LIBS) $(LIB_LINK_LIBS_ALL_LIBS)

include _module-app-throughput.inc

include _module-libs.inc

dumpinfo:
	@echo 'CC' $(CC)
	@echo ''
	@echo 'CXX' $(CXX)
	@echo ''
	@echo 'CC_MISC' $(CC_MISC)
	@echo 'CC_NATIVE' $(CC_NATIVE)
	@echo ''
	@echo 'DEFINES' $(DEFINES)
	@echo ''
	@echo 'INCLUDES' $(INCLUDES)
	@echo ''
	@echo 'LIB_LINK_LIBS_ALL_LIBS' $(LIB_LINK_LIBS_ALL_LIBS)
	@echo ''
	
