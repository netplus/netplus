include ../../../../projects/makefile/_mk-generic.inc
include ../../../_libs-config.inc

APP_TEST_PATH					:= ../../..
APP_PROJECTS_PATH				:= ../../projects
APP_BUILD_BIN_PATH				:= $(APP_PROJECTS_PATH)/build
APP_TMP_PATH					:= $(APP_PROJECTS_PATH)/build/tmp/$(ARCH_BUILD_NAME)

APP_NAME = httpclient

APP_SRC				:= $(APP_TEST_PATH)/$(APP_NAME)/src
APP_TARGET			:= $(APP_BUILD_BIN_PATH)/$(APP_NAME).$(ARCH_BUILD_NAME)
APP_BIN_PATH		:= $(APP_TMP_PATH)/$(APP_NAME)


	
${APP_NAME}: netplus $(APP_TARGET)

all: ${APP_NAME}
	@echo 'build' $(APP_NAME)


clean:
	rm -rf $(APP_TARGET)
	rm -rf $(APP_BIN_PATH)/*
	


APP_ALL_CPP_FILES :=\
	$(foreach path, $(APP_SRC), $(shell find $(path) -name *.cpp) )

APP_ALL_O_FILES	:= $(APP_ALL_CPP_FILES:.cpp=.$(O_EXT))
APP_ALL_O_FILES := $(foreach path, $(APP_ALL_O_FILES), $(subst $(APP_SRC)/,,$(path)))
APP_ALL_O_FILES	:= $(addprefix $(APP_BIN_PATH)/,$(APP_ALL_O_FILES))


#custome for codeblock
#CC_MISC := $(CC_MISC) -finput-charset=GBK -fexec-charset=GBK

DEFINES :=\
	$(foreach define,$(DEFINES), -D$(define))
	
INCLUDES:= \
	$(foreach include,$(CC_INC), -I"$(include)") \


$(APP_TARGET): $(APP_ALL_O_FILES)
	@if [ ! -d $(@D) ] ; then \
		mkdir -p $(@D) ; \
	fi
	
	@echo "---"
	@echo \*\* assembling $@ ...
	@echo $(CXX) $(LINK_MISC) $^ -o $@ $(LINK_LIBS)
	@$(CXX) -rdynamic $(LINK_MISC) $^ -o $@ $(LINK_LIBS) 
	@echo "---"
	


$(APP_BIN_PATH)/%.o : $(APP_SRC)/%.cpp
	@if [ ! -d $(@D) ] ; then \
		mkdir -p $(@D) ; \
	fi
	
	@echo 'compiling $$<F ' $(<F)
	@echo '$$@ '$@
	@echo ''
	@echo $(CXX) $(CC_MISC) $(CC_LANG_VERSION) $(DEFINES) $(INCLUDES) $< -o $@
	@$(CXX) $(CC_MISC) $(CC_LANG_VERSION) $(DEFINES) $(INCLUDES) $< -o $@
	

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
	
