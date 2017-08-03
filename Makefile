CC=gcc
CXX=g++
RM=rm -f
CXXFLAGS=-Wall -std=c++14 -I./src -I./include $(shell pkg-config --cflags libpq)
LDFLAGS=-L./lib/
LDLIBS=-lm -lrt -lgflags -lpqxx -lboost_system -lpthread -lboost_thread -lzmqpp -lpq -lzmq

BIN=data-processing

TESTSBIN=test-data-processing

SRCDIR=src/
OBJDIR=obj/
vpath %.cpp $(SRCDIR)
vpath %.h $(SRCDIR)

SRC=data-functions.cpp
OBJS = $(addsuffix .o,$(basename $(SRC)))


all: $(BIN)

tests: $(TESTSBIN)

test-data-processing: src/tests/test-data-processing.cpp $(addprefix $(OBJDIR),$(OBJS))
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)


$(BIN): data-processing.cpp $(addprefix $(OBJDIR),$(OBJS))
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(OBJDIR)%.o: %.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

.PHONY: clean

clean:
	rm -rf $(OBJDIR)

cleanall: clean
	$(RM) $(BIN)

cleantests:
	$(RM) $(TESTSBIN)
