BUILD_TYPE ?= Release
JOBS       ?= $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Override on the command line: make CC=gcc-14 CXX=g++-14
ifdef CC
  CMAKE_CC  := -DCMAKE_C_COMPILER=$(CC)
endif
ifdef CXX
  CMAKE_CXX := -DCMAKE_CXX_COMPILER=$(CXX)
endif

.PHONY: all collector dashboard clean

all: collector dashboard

collector:
	cmake -B weather-collector-cpp/build -S weather-collector-cpp \
	    -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) $(CMAKE_CC) $(CMAKE_CXX)
	cmake --build weather-collector-cpp/build -j$(JOBS)

dashboard:
	cmake -B weather-dashboard-cpp/build -S weather-dashboard-cpp \
	    -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) $(CMAKE_CC) $(CMAKE_CXX)
	cmake --build weather-dashboard-cpp/build -j$(JOBS)

clean:
	rm -rf weather-collector-cpp/build weather-dashboard-cpp/build
