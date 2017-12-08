#++
# Makefile - Makefile for the MBS project...
#
#DESCRIPTION:
#   This is a fairly simple Makefile for building MBS on Debian using GNU
# Make, gcc (or rather, g++), and the PLX library.  
#
# Bob Armstrong <bob@jfcl.com>   [16-MAY-2017]
#
#TARGETS:
#  make all	- rebuild MBS executable
#  make depends - recreate all dependencies
#  make clean	- delete all generated files 
#
# REVISION HISTORY:
# dd-mmm-yy	who     description
# 17-MAY-17	RLA	New file.
#--

# Compiler preprocessor DEFINEs for the entire project ...
DEFINES = _DEBUG


# Define the PLX library path and options ...
PLXDEFS   = PLX_LITTLE_ENDIAN PLX_LINUX PLX_64BIT
PLXINC    = /usr/local/PlxSdk/Include/
PLXLIB    = /usr/local/PlxSdk/PlxApi/Library/


# Define the target program and source files ...
#   WARNING - currently all source files must be .cpp C++ files - we don't
# know how to compile anything else!
TARGET    = mbs
SOURCES   = MBS.cpp BaseDrive.cpp DECUPE.cpp DiskDrive.cpp DriveType.cpp \
            MBA.cpp TapeDrive.cpp UserInterface.cpp
UPEPATH   = ../../UPELIB/src/
INCLUDES  = $(PLXINC) $(UPEPATH)
OBJECTS   = $(SOURCES:.cpp=.o)
LIBRARIES = -lupe -lPlxApi -lstdc++ -lm -ldl


# Define the standard tool paths and options.
CC       = /usr/bin/gcc
CPP      = $(CC) -x c++
LD       = $(CC)
CPPFLAGS = -std=c++0x
CFLAGS   = -ggdb3 -O3 -pthread -Wall \
            -funsigned-char -funsigned-bitfields -fshort-enums \
	    $(foreach inc,$(INCLUDES),-I$(inc)) \
	    $(foreach def,$(DEFINES) $(PLXDEFS),-D$(def))
LDFLAGS  = -L$(UPEPATH) -L$(PLXLIB) -pthread


# Rule to build the executable ...
all:		$(TARGET)

$(TARGET):	$(OBJECTS)
	$(LD) $(LDFLAGS) -o $(TARGET) $(OBJECTS) $(LIBRARIES)

# Rule to compile C++ files ...
.cpp.o:
	@echo Compiling $<
	@$(CPP) -c $(CPPFLAGS) $(CFLAGS) $<


# Rule to clean up everything ...
clean:
	rm -f $(TARGET) $(OBJECTS) *~ *.core core Makefile.dep

# And a rule to rebuild the dependencies ...
Makefile.dep: $(SOURCES)
	$(CPP) -M $(CPPFLAGS) $(CFLAGS) $(SOURCES) >Makefile.dep

include Makefile.dep
