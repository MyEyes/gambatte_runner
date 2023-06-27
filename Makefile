CXX = g++
#CXX = clang++

EXE = gambatte_runner

EMULATOR_INCLUDE = gambatte-core/libgambatte/include
EMULATOR_LIB = gambatte-core/libgambatte/libgambatte.so
LOCAL_LIB = $(notdir $(EMULATOR_LIB))

SOURCES += workitem.cpp runner.cpp main.cpp
OBJS = $(addprefix $(EMULATOR_HEADLESS_SRC)/, $(addsuffix .o, $(basename $(notdir $(CORE_SOURCES)))))

OBJS = $(addsuffix .o, $(basename $(notdir $(SOURCES))))

CXXFLAGS = -I$(EMULATOR_INCLUDE)
CXXFLAGS += -Wall -Wextra -Wformat -std=c++11 -DHAVE_STDINT_H
LIBS = -lgambatte

DEBUG ?= 0
ifeq ($(DEBUG), 1)
    CXXFLAGS +=-DDEBUG -g3 -fPIC
else
    CXXFLAGS +=-DNDEBUG -O3 -flto -fPIC
endif

SANITIZE ?= 0
ifeq ($(SANITIZE), 1)
	CXXFLAGS +=-fsanitize=address
    LIBS += -lasan
endif

##---------------------------------------------------------------------
## BUILD RULES
##---------------------------------------------------------------------
# %.o:./%.cpp
# 	$(CXX) $(CXXFLAGS) -c -o $@ $<
LDFLAGS = -fPIC -L. #check for libgambatte in cwd

%.o:%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

all: $(EXE)
	@echo Build complete for gambatte_runner

$(LOCAL_LIB): $(EMULATOR_LIB)
	cp $(EMULATOR_LIB) $(LOCAL_LIB)

$(EXE): $(OBJS) $(LOCAL_LIB)
	$(CXX) -o $@ $(OBJS) $(LDFLAGS) $(LIBS)

clean:
	rm -f $(EXE) $(OBJS)
