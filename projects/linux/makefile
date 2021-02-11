include generic-header.in

LIB_ARCH_BUILD = $(ARCH_BUILD_NAME)
LIBNAME		:= netplus

LIB_DIR 	:= ../..
LIB_PARENT_DIR := ../../..

LIB_INCLUDE_DIR := $(LIB_DIR)/include
LIB_SOURCE_DIR 	:= $(LIB_DIR)/src

LIB_BUILD_DIR	:= $(LIB_DIR)/build/$(LIB_ARCH_BUILD)
LIB_BIN_DIR		:= $(LIB_DIR)/bin/$(LIB_ARCH_BUILD)

LIB_SIMD_CPP_FILES := 

#ifeq ($(ARCH_IS_X86),YES)
#	LIB_CC_SIMD := -mavx2 -mssse3
#	LIB_SIMD_DEFINE := NETP_ENABLE_AVX2 NETP_ENABLE_SSSE3
#else ifeq ($(ARCH_IS_ARMV7A),YES)
#	LIB_CC_SIMD := -mcpu=cortex-a7 -mfloat-abi=hard -mfpu=neon -fno-tree-vectorize
#	LIB_SIMD_DEFINE := NETP_ENABLE_NEON
#endif


LIB_CPP_FILES	:= $(shell find $(LIB_SOURCE_DIR) -name *.cpp -not -name ".*" )
LIB_O_FILES		:= $(LIB_CPP_FILES:.cpp=.$(O_EXT))
LIB_O_FILES		:= $(foreach path,$(LIB_O_FILES), $(subst $(LIB_SOURCE_DIR)/,,$(path)))
LIB_O_FILES		:= $(addprefix $(LIB_BUILD_DIR)/,$(LIB_O_FILES))

#LIB_BOTAN_VER			:= 2.15.0
#LIB_BOTAN_DIR 			:= $(LIB_PARENT_DIR)/botan/$(LIB_BOTAN_VER)/gcc_4.8.5


#ifeq ($(PRJ_ARCH), x86_64)
#	LIB_BOTAN_DIR := $(LIB_BOTAN_DIR)/x86_64/static
#endif

#LIB_BOTAN_INCLUDE := $(LIB_BOTAN_DIR)/include/botan-2
#LIB_BOTAN_A := $(LIB_BOTAN_DIR)/lib/libbotan-2.a


LIB_3RD_DIR				:= $(LIB_DIR)/3rd
LIB_3RD_FILES_DIR		:= $(LIB_3RD_DIR)/http_parser $(LIB_3RD_DIR)/udns/0.4


LIB_3RD_CPP_BUILD_DIR	:= $(LIB_3RD_DIR)/build/cpp/$(LIB_ARCH_BUILD)
LIB_3RD_C_BUILD_DIR		:= $(LIB_3RD_DIR)/build/c/$(LIB_ARCH_BUILD)


LIB_INCLUDE_DIR := $(LIB_INCLUDE_DIR)
LIB_NETP_INCLUDE := $(LIB_INCLUDE_DIR)

# for botan
LIB_3RD_CPP_FILES 		:= $(shell find $(LIB_3RD_FILES_DIR) -name *.cpp -not -name ".*")
LIB_3RD_CPP_O_FILES		:= $(LIB_3RD_CPP_FILES:.cpp=.$(O_EXT))
LIB_3RD_CPP_O_FILES		:= $(foreach path,$(LIB_3RD_CPP_O_FILES), $(subst $(LIB_3RD_DIR)/,,$(path)))
LIB_3RD_CPP_O_FILES 	:= $(addprefix $(LIB_3RD_CPP_BUILD_DIR)/,$(LIB_3RD_CPP_O_FILES))

# for http parser
LIB_3RD_C_FILES 	:= $(shell find $(LIB_3RD_FILES_DIR) -name *.c -not -name ".*")
LIB_3RD_C_O_FILES	:= $(LIB_3RD_C_FILES:.c=.$(O_EXT))
LIB_3RD_C_O_FILES	:= $(foreach path,$(LIB_3RD_C_O_FILES), $(subst $(LIB_3RD_DIR)/,,$(path)))
LIB_3RD_C_O_FILES 	:= $(addprefix $(LIB_3RD_C_BUILD_DIR)/,$(LIB_3RD_C_O_FILES))

LIB_FULL_NAME		:= $(LIBPREFIX)$(LIBNAME).$(LIBEXT)
LIB_FULL_PATH_NAME	:= $(LIB_BIN_DIR)/$(LIB_FULL_NAME)

LIB_NETP_A := $(LIB_FULL_PATH_NAME)

DEFINES	:=__NETP__ $(DEFINES) $(LIB_SIMD_DEFINE)

#we have to output these define to a build.hpp file to be referenced by others
DEFINES :=\
	$(foreach define,$(DEFINES), -D$(define))


INCLUDES:= \
	$(foreach include,$(LIB_INCLUDE_DIR), -I"$(include)")


all: precc_netplus netplus
netplus: $(LIB_FULL_PATH_NAME)

precc:
	@echo "-------------------------------"
	@echo \*\* compiling...
	
	@echo $(CXX) $(CC_MISC) $(CC_SIMD) $(DEFINES) $(INCLUDES) {in} -o {out}
	@echo "-------------------------------"
	@echo ""
precc_netplus: precc
	@echo "LIB_ARCH_BUILD:	$(LIB_ARCH_BUILD)"
	@echo "LIB_BUILD_DIR":	$(LIB_BUILD_DIR)
	@echo "LIB_BIN_DIR":	$(LIB_BIN_DIR)
	
	@echo "LIB_SIMD_CPP_FILES: $(LIB_SIMD_CPP_FILES)"
	@echo "LIB_CPP_FILES: $(LIB_CPP_FILES)"
	@echo "LIB_O_FILES: $(LIB_O_FILES)"
	
	@echo ""
	@echo "LIB_3RD_CPP_FILES: $(LIB_3RD_CPP_FILES)"
	@echo "LIB_3RD_CPP_O_FILES: $(LIB_3RD_CPP_O_FILES)"
	
	@echo ""
	@echo "LIB_3RD_C_FILES: $(LIB_3RD_C_FILES)"
	@echo "LIB_3RD_C_O_FILES: $(LIB_3RD_C_O_FILES)"
	
	@echo ""

$(LIB_FULL_PATH_NAME): $(LIB_O_FILES) $(LIB_3RD_CPP_O_FILES) $(LIB_3RD_C_O_FILES)
	@if [ ! -d $(@D) ] ; then \
		mkdir -p $(@D) ; \
	fi
	@echo \*\* linking...
	$(AR) rs $@ $^

$(LIB_BUILD_DIR)/%.o : $(LIB_SOURCE_DIR)/%.cpp
	@if [ ! -d $(@D) ] ; then \
		mkdir -p $(@D) ; \
	fi
	@echo "------build cpp file begin------"
	@echo $(<F)
	@echo "------build cpp file end------"

	$(if $(findstring $(<F),$(LIB_SIMD_CPP_FILES)),$(CXX) $(CC_MISC) $(CC_C11) $(CC_SIMD) $(LIB_CC_SIMD) $(DEFINES) $(INCLUDES) $< -o $@, $(CXX) $(CC_MISC) $(CC_C11) $(DEFINES) $(INCLUDES) $< -o $@ ) 

$(LIB_3RD_CPP_BUILD_DIR)/%.o : $(LIB_3RD_DIR)/%.cpp
	@if [ ! -d $(@D) ] ; then \
		mkdir -p $(@D) ; \
	fi
	@echo "------build 3rd cpp file begin------"
	@echo $(<F)
	@echo "------build 3rd cpp file end------"
	
	$(CXX) $(CC_MISC) $(CC_C11) $(CC_3RD_CPP_MISC)  $(DEFINES) $(INCLUDES) $< -o $@

$(LIB_3RD_C_BUILD_DIR)/%.o : $(LIB_3RD_DIR)/%.c
	@if [ ! -d $(@D) ] ; then \
		mkdir -p $(@D) ; \
	fi
	@echo "------build 3rd c file begin------"
	@echo $(<F)
	@echo "------build 3rd c file end------"
	
	$(CC) $(CC_MISC) $(DEFINES) $(INCLUDES) $< -o $@
	
# Don't load dependencies when we're cleaning
# ifeq (0, $(words $(findstring $(MAKECMDGOALS), clean)))
#    -include $(LIB_O_FILES:.o=.d)
# endif


clean:
	rm -rf $(LIB_BUILD_DIR)
	rm -rf $(LIB_FULL_PATH_NAME)
	rm -rf $(LIB_3RD_CPP_BUILD_DIR)
	rm -rf $(LIB_3RD_C_BUILD_DIR)
