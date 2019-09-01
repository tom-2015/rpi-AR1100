#enter your output filename here
APPNAME=AR1100
#include dirs
INCL= `pkg-config --cflags gtk+-2.0` `pkg-config --cflags libusb-1.0`
#add link libs here and link dirs 
LINK=-L/usr/lib -L/usr/local/lib -lpthread `pkg-config --libs libusb-1.0` `pkg-config --libs gtk+-2.0`
#compiler command
CXX=g++ 
#compiler flags
CXXFLAGS = -DAPP_VERSION="1.1" $(INCL)

#debugging or not (make DEBUG=1)
ifeq (1,$(DEBUG))
  CXXFLAGS += -g -DDEBUG -O0
else
  CXXFLAGS += -O3
endif

#list all source files and create object files for them
SRCS := ${wildcard *.cpp}
OBJS := ${SRCS:.cpp=.o}

#create application
all: $(APPNAME)

# link
$(APPNAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(APPNAME) $(LINK)

# pull in dependency info for *existing* .o files
-include $(OBJS:.o=.d)

# compile and generate dependency info;
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c  $*.cpp -o $*.o
	$(CXX) $(CXXFLAGS) -MM $*.cpp > $*.d
	@mv -f $*.d $*.d.tmp
	@sed -e 's|.*:|$*.o:|' < $*.d.tmp > $*.d
	@sed -e 's/.*://' -e 's/\\$$//' < $*.d.tmp | fmt -1 | \
	  sed -e 's/^ *//' -e 's/$$/:/' >> $*.d
	@rm -f $*.d.tmp

	
# remove compilation files
clean:
	rm -f $(APPNAME) *.o *.d
