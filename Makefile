ifeq	($(release),1)

FLG := -O3 -Werror -fPIC -D_LINUX_SYS --std=c++11

else

FLG := -O0 -Werror -g -fPIC -D_LINUX_SYS --std=c++11

endif

PRG := ./amof-redis

SOURCEALL :=$(shell find ./ -maxdepth 1 -iname "*.c" -o -iname "*.cpp" -o -iname "*.cc" -o -iname "*.h")
#SRC := $(wildcard src/*.cpp)

OBJCXX := $(patsubst %.cpp,%.o,$(filter %.cpp, $(SOURCEALL)))
OBJC  := $(patsubst %.c,%.o,$(filter %.c, $(SOURCEALL)))

CXX := g++
GCC := gcc

INC := -I./

LIB := -L./hiredis -Wl,-Bstatic -lhiredis \
       -Wl,-Bdynamic -lrt -lz -lc -lpthread

#for c
CFLAGS		= -g -Wall -Werror 

.PHONY: clean	
	
$(PRG): $(OBJCXX) $(OBJC)
	$(CXX) -static-libstdc++ -static-libgcc -o $@ $^ $(LIB)

%.o: %.cpp
	$(CXX) $(FLG) -o $@ -c $< $(INC)

%.o: %.c
	$(GCC) $(CFLAGS) -o $@ -c $<

clean:
	rm -f $(OBJCXX) $(OBJC) $(PRG)


#yum install glibc-static libstdc++-static	
