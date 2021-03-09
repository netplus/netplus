APP_TEST_PATH					:= ../../..
APP_PROJECTS_PATH				:= ../../projects
APP_BUILD_BIN_PATH				:= $(APP_PROJECTS_PATH)/build
APP_TMP_PATH					:= $(APP_PROJECTS_PATH)/build/tmp/$(ARCH_BUILD_NAME)

ifndef $(O_EXT)
	O_EXT=o
endif

APP_NAME = promise

${APP_NAME}_SRC				:= $(APP_TEST_PATH)/${APP_NAME}/src
${APP_NAME}_INCLUDE_PATH	+= $(LIB_NETP_INCLUDE_PATH)
${APP_NAME}_TARGET			:= $(APP_BUILD_BIN_PATH)/$(APP_NAME).$(ARCH_BUILD_NAME)
${APP_NAME}_BIN_PATH		:= $(APP_TMP_PATH)/$(APP_NAME)

APP_TARGET = $(${APP_NAME}_TARGET)
APP_TARGET_PATH = $(${APP_NAME}_BIN_PATH)

	
${APP_NAME}: netplus $(APP_TARGET)

all: ${APP_NAME}
	@echo 'build' $(APP_NAME)


clean:
	rm -rf $(APP_TARGET)
	rm -rf $(APP_TARGET_PATH)/*
	

${APP_NAME}_INCLUDES			:= \
	$(foreach path, $(${APP_NAME}_INCLUDE_PATH),-I"$(path)" )

${APP_NAME}_ALL_CPP_FILES :=\
	$(foreach path, $(${APP_NAME}_SRC), $(shell find $(path) -name *.cpp) )

${APP_NAME}_ALL_O_FILES	:= $(${APP_NAME}_ALL_CPP_FILES:.cpp=.$(O_EXT))
${APP_NAME}_ALL_O_FILES := $(foreach path, $(${APP_NAME}_ALL_O_FILES), $(subst $(${APP_NAME}_SRC)/,,$(path)))
${APP_NAME}_ALL_O_FILES	:= $(addprefix $(${APP_NAME}_BIN_PATH)/,$(${APP_NAME}_ALL_O_FILES))


#custome for codeblock
#CC_MISC := $(CC_MISC) -finput-charset=GBK -fexec-charset=GBK

#ifeq ($(PRJ_BUILD),debug)
LINK_MISC := $(LINK_MISC)
#endif


$(APP_TARGET): $(${APP_NAME}_ALL_O_FILES)
	@if [ ! -d $(@D) ] ; then \
		mkdir -p $(@D) ; \
	fi
	
	@echo "---"
	@echo \*\* assembling $@...
	@echo $(CXX) $(LINK_MISC) $^ -o $@ $(LINK_LIBS)
	@$(CXX) $(LINK_MISC) $^ -o $@ $(LINK_LIBS) 
	@echo "---"
	


$(APP_TARGET_PATH)/%.o : $(${APP_NAME}_SRC)/%.cpp
	@if [ ! -d $(@D) ] ; then \
		mkdir -p $(@D) ; \
	fi
	
	@echo 'compiling $$<F ' $(<F)
	@echo '$$@ '$@
	@echo ''
	@echo $(CXX) $(CC_MISC) $(CC_C11) $(DEFINES) $(${APP_NAME}_INCLUDES) $< -o $@
	@$(CXX) $(CC_MISC) $(CC_C11) $(DEFINES) $(${APP_NAME}_INCLUDES) $< -o $@
	
