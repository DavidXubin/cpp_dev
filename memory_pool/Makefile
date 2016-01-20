.PHONY : all clean
TARGET = my_test

all: $(TARGET)
#CC = gcc
CXX = g++
RM := rm     
AR := ar                    
VPATH =.
CXXPPFLAGS := -D__USE_FILE_OFFSET64
CXXFLAGS = -Wall -O2 -I.
CXXLDFLAGS = -lpthread  -lboost_system
LIBDIRS := -L.
                       
					   
DIRS :=.                    
FILES = $(foreach dir, $(DIRS), $(wildcard $(dir)/*.cpp)) 
OBJS = $(patsubst %.cpp, %.o, $(FILES))         

$(TARGET):$(OBJS)                       
	$(CXX) -o $@ $(LIBDIRS) $^ $(CXXLDFLAGS)
	
	
%.o: %.cpp
	$(CXX) $(CXXPPFLAGS) $(CXXFLAGS) -c -o $@ $<
	
%.o: %.cc
	$(CXX) $(CXXPPFLAGS) $(CXXFLAGS) -c -o $@ $<

clean:                                  
	-@$(RM) $(TARGET) $(OBJS)
