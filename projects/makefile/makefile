include _mk-generic.inc

LIB_ARCH_BUILD = $(ARCH_BUILD_NAME)
LIBNAME		:= netplus

LIB_DIR 				:= ../..
LIB_3RD_DIR 			:= ../../3rd
LIB_DEPS_DIR			:= ../../../deps

LIB_INCLUDE_DIR := $(LIB_DIR)/include
LIB_SOURCE_DIR 	:= $(LIB_DIR)/src

LIB_BUILD_DIR	:= $(LIB_DIR)/build/$(LIB_ARCH_BUILD)
LIB_BIN_DIR		:= $(LIB_DIR)/bin/$(LIB_ARCH_BUILD)

LIB_SIMD_CPP_FILES := 


LIB_CPP_FILES	:= $(shell find $(LIB_SOURCE_DIR) -name *.cpp -not -name ".*" )
LIB_O_FILES		:= $(LIB_CPP_FILES:.cpp=.$(O_EXT))
LIB_O_FILES		:= $(foreach path,$(LIB_O_FILES), $(subst $(LIB_SOURCE_DIR)/,,$(path)))
LIB_O_FILES		:= $(addprefix $(LIB_BUILD_DIR)/,$(LIB_O_FILES))

CC_INC 			:= $(LIB_INCLUDE_DIR) $(CC_INC)


LIB_3RD_C_ARES_VERSION	:= c-ares-1.19.0
LIB_3RD_FILES_DIR		:= $(LIB_3RD_DIR)/llhttp $(LIB_3RD_DIR)/c-ares/$(LIB_3RD_C_ARES_VERSION)/src/lib


LIB_3RD_CPP_BUILD_DIR	:= $(LIB_3RD_DIR)/build/cpp/$(LIB_ARCH_BUILD)
LIB_3RD_C_BUILD_DIR		:= $(LIB_3RD_DIR)/build/c/$(LIB_ARCH_BUILD)


LIB_3RD_CPP_FILES 		:= $(shell find $(LIB_3RD_FILES_DIR) -name *.cpp -not -name ".*")
LIB_3RD_CPP_O_FILES		:= $(LIB_3RD_CPP_FILES:.cpp=.$(O_EXT))
LIB_3RD_CPP_O_FILES		:= $(foreach path,$(LIB_3RD_CPP_O_FILES), $(subst $(LIB_3RD_DIR)/,,$(path)))
LIB_3RD_CPP_O_FILES 	:= $(addprefix $(LIB_3RD_CPP_BUILD_DIR)/,$(LIB_3RD_CPP_O_FILES))


LIB_3RD_CPP_FILES_INCLUDES := $(LIB_3RD_DIR)/c-ares/$(LIB_3RD_C_ARES_VERSION)/ares_config/generic $(LIB_3RD_DIR)/c-ares/$(LIB_3RD_C_ARES_VERSION)/include

CC_INC := $(CC_INC) $(LIB_3RD_CPP_FILES_INCLUDES)

C_3RD_INCLUDES := \
	$(foreach include,$(LIB_3RD_CPP_FILES_INCLUDES), -I"$(include)")

C_ARES_DEFINES := HAVE_CONFIG_H

C_3RD_DEFINES :=\
	$(foreach define,$(C_ARES_DEFINES), -D$(define))

# for llhttp/c-ares parser
LIB_3RD_C_FILES 	:= $(shell find $(LIB_3RD_FILES_DIR) -name *.c -not -name ".*")
LIB_3RD_C_O_FILES	:= $(LIB_3RD_C_FILES:.c=.$(O_EXT))
LIB_3RD_C_O_FILES	:= $(foreach path,$(LIB_3RD_C_O_FILES), $(subst $(LIB_3RD_DIR)/,,$(path)))
LIB_3RD_C_O_FILES 	:= $(addprefix $(LIB_3RD_C_BUILD_DIR)/,$(LIB_3RD_C_O_FILES))

LIB_FULL_NAME		:= $(LIBPREFIX)$(LIBNAME).$(LIBEXT)
LIB_FULL_PATH_NAME	:= $(LIB_BIN_DIR)/$(LIB_FULL_NAME)

LIB_NETP_A := $(LIB_FULL_PATH_NAME)

DEFINES	:= __NETP__ $(DEFINES) $(LIB_SIMD_DEFINE) $(C_ARES_DEFINES)

#we have to output these define to a build.hpp file to be referenced by others
DEFINES :=\
	$(foreach define,$(DEFINES), -D$(define))


INCLUDES:= \
	$(foreach include,$(CC_INC), -I"$(include)")


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
	
	@echo "OS_NAME: $(OS_NAME)"
	@echo "CXX_VERSION: $(CXX_VERSION)"
	@echo "DEPS_INC: $(DEPS_INC)"
	@echo "DEPS_LIB: $(DEPS_LIB)"
	@echo "MK_GENERIC_DIR: $(MK_GENERIC_DIR)"
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

	$(if $(findstring $(<F),$(LIB_SIMD_CPP_FILES)),$(CXX) $(CC_MISC) $(CC_LANG_VERSION) $(CC_SIMD) $(SIMD_DEFINES) $(DEFINES) $(INCLUDES) $< -o $@, $(CXX) $(CC_MISC) $(CC_LANG_VERSION) $(DEFINES) $(INCLUDES) $< -o $@ ) 

$(LIB_3RD_CPP_BUILD_DIR)/%.o : $(LIB_3RD_DIR)/%.cpp
	@if [ ! -d $(@D) ] ; then \
		mkdir -p $(@D) ; \
	fi
	@echo "------build 3rd cpp file begin------"
	@echo $(CXX) $(CC_MISC) $(CC_LANG_VERSION) $(CC_3RD_CPP_MISC) $(DEFINES) $(INCLUDES) $< -o $@
	@echo "------build 3rd cpp file end------"
	
	$(CXX) $(CC_MISC) $(CC_LANG_VERSION) $(CC_3RD_CPP_MISC) $(DEFINES) $(INCLUDES) $< -o $@

$(LIB_3RD_C_BUILD_DIR)/%.o : $(LIB_3RD_DIR)/%.c
	@if [ ! -d $(@D) ] ; then \
		mkdir -p $(@D) ; \
	fi
	@echo "------build 3rd c file begin------"
	@echo $(CC) $(CC_MISC) $(DEFINES) $(C_3RD_DEFINES) $(INCLUDES) $(C_3RD_INCLUDES) $< -o $@
	@echo "------build 3rd c file end------"
	
	$(CC) $(CC_MISC) $(DEFINES) $(C_3RD_DEFINES) $(INCLUDES) $(C_3RD_INCLUDES) $< -o $@
	
# Don't load dependencies when we're cleaning
# ifeq (0, $(words $(findstring $(MAKECMDGOALS), clean)))
#    -include $(LIB_O_FILES:.o=.d)
# endif


clean:
	rm -rf $(LIB_BUILD_DIR)
	rm -rf $(LIB_FULL_PATH_NAME)
	rm -rf $(LIB_3RD_CPP_BUILD_DIR)
	rm -rf $(LIB_3RD_C_BUILD_DIR)
