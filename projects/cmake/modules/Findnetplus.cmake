message("now using Findnetplus.cmake find netplus lib")

# 指定netplus头文件目录
#FIND_PATH(netplus_INCLUDE_DIR netp.hpp /usr/local/netplus_demo/include)
FIND_PATH(netplus_INCLUDE_DIR netp.hpp ${CMAKE_INSTALL_PREFIX}/netplus_demo/include)
message("./h dir ${netplus_INCLUDE_DIR}")

# 指定netplus安装目录
if (WIN32)
	FIND_LIBRARY(netplus_LIBRARY netplus.lib ${CMAKE_INSTALL_PREFIX}/netplus_demo/lib)
else()
	FIND_LIBRARY(netplus_LIBRARY libnetplus.a ${CMAKE_INSTALL_PREFIX}/netplus_demo/lib)
endif()
message("lib dir: ${netplus_LIBRARY}")

if(netplus_INCLUDE_DIR AND netplus_LIBRARY)
    set(netplus_FOUND TRUE)
endif()