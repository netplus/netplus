LIBS_PATH := ./../../../../..

LIB_ARCH_BUILD				:= $(ARCH_BUILD_NAME)

LIB_NETP_PATH				:= $(LIBS_PATH)/netplus
LIB_NETP_MAKEFILE_PATH		:= $(LIB_NETP_PATH)/projects/makefile
LIB_NETP_CONFIG_PATH		:= $(LIB_NETP_PATH)/../netplus_config
LIB_NETP_BIN_PATH			:= $(LIB_NETP_PATH)/bin/$(LIB_ARCH_BUILD)/libnetplus.a
LIB_NETP_INCLUDE_PATH		:= $(LIB_NETP_PATH)/include $(LIB_NETP_CONFIG_PATH)

CC_INC 		:= $(LIB_NETP_INCLUDE_PATH) $(CC_INC)
LINK_LIBS 	:= $(LIB_NETP_BIN_PATH) $(LINK_LIBS)

libs: netplus
libs_clean: netplus_clean

netplus:
	@echo "building netplus begin"
	make -C$(LIB_NETP_MAKEFILE_PATH) build=$(PRJ_BUILD) arch=$(PRJ_ARCH) simd=$(PRJ_SIMD)
	@echo "building netplus finish"
	@echo 

netplus_clean:
	@echo "make -C$(LIB_NETP_MAKEFILE_PATH) build=$(PRJ_BUILD) arch=$(PRJ_ARCH) simd=$(PRJ_SIMD) clean"
	make -C$(LIB_NETP_MAKEFILE_PATH) build=$(PRJ_BUILD) arch=$(PRJ_ARCH) simd=$(PRJ_SIMD) clean
