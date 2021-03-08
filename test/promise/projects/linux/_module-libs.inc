
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
