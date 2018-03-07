UNAME := $(shell uname -s)
OBJDIRR=.build-release
OBJDIRD=.build-debug

INC = -I.
ifeq ($(UNAME),Linux)
  PKG  =pkg-config
  MOC  =moc
  LIBS =-L/usr/lib64 -lm -lOpenGL
  INC  += -I/usr/include
  RPATH=-Wl,-z,origin -Wl,--disable-new-dtags -Wl,-rpath,
endif

#INC     += `$(PKG) --cflags glm`
LIBS    += -lstdc++ $(RPATH) `$(PKG) --libs egl`
INC     += $(INCLUDE_FLAGS)
LIBS    += $(LIB_FLAGS)

CC=gcc-5
LINK=gcc-5
#AR=ar ru
AR=ar rcs
AR_OUT=
LINK_OUT= -o
MINUS_O = -o
CFLAGS   = -std=c++11 -Wall -Wno-unused-variable -Wno-unused-function -Wno-deprecated-register -fPIC -DBT_USE_DOUBLE_PRECISION -g -O3 -march=native $(INC)
CFLAGSD  = -std=c++11 -Wall -Wno-unused-variable -Wno-unused-function -Wno-deprecated-register -fPIC -DBT_USE_DOUBLE_PRECISION -g -DDEBUG $(INC)

SHARED  = -shared
DEPENDS = -MMD -MF $@.dep

EVERY_BIN=../libroboschool.so ../libroboschool_d.so

SIM = \
 utils.cpp \
 gl_context.cpp \
 assets-mesh.cpp \
 render-camera.cpp \
 render-simple.cpp \
 render-simple-primitives.cpp \
 physics-bullet.cpp

ifneq ("$(wildcard /usr/lib/x86_64-linux-gnu/libGLX_nvidia.so.0)", "")
$(info Hardware render (turn on shadows))
SIM     += render-ssao.cpp
CFLAGS  += -DUSE_SSAO
CFLAGSD += -DUSE_SSAO
else
$(info Slow hardware or software render (no shadows))
endif

CPP = roboschool.cpp

SIM_R = $(patsubst %.cpp, $(OBJDIRR)/%.o, $(SIM))
SIM_D = $(patsubst %.cpp, $(OBJDIRD)/%.o, $(SIM))
CPP_R = $(patsubst %.cpp, $(OBJDIRR)/%.o, $(CPP))
CPP_D = $(patsubst %.cpp, $(OBJDIRD)/%.o, $(CPP))

EVERY_OBJ_R = $(SIM_R) $(CPP_R)
EVERY_OBJ_D = $(SIM_D) $(CPP_D)
DEP = $(patsubst %.o,%.o.dep, $(EVERY_OBJ_R) $(EVERY_OBJ_D))

all: dirs $(EVERY_BIN)

../libroboschool.so: $(SIM_R) $(CPP_R)
	$(LINK) $(SHARED) $(LINK_OUT)$@ $^ $(LIBS)

../libroboschool_d.so: $(SIM_D) $(CPP_D)
	$(LINK) $(SHARED) $(LINK_OUT)$@ $^ $(LIBS)

$(OBJDIRR)/%.o: %.cpp
	$(CC) $(CFLAGS) -c $<  $(MINUS_O)$@ $(DEPENDS)
$(OBJDIRD)/%.o: %.cpp
	$(CC) $(CFLAGSD) -c $<  $(MINUS_O)$@ $(DEPENDS)

.PHONY: depends clean dirs

clean:
	$(RM) $(EVERY_BIN) $(EVERY_OBJ_R) $(EVERY_OBJ_D) $(CPP_R) $(CPP_D) .generated/*.moc *.ilk *.pdb $(DEP)
	rm -rf .generated
	rm -rf $(OBJDIRD)
	rm -rf $(OBJDIRR)

depends:
	cat  $(DEP) > Makefile.dep

.generated:
	mkdir -p .generated
$(OBJDIRR):
	mkdir -p $@
$(OBJDIRD):
	mkdir -p $@

dirs: .generated $(OBJDIRR) $(OBJDIRD)


-include Makefile.dep
