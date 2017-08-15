CC=gcc
CXX=g++
RM=rm -f
LIBS=libpq libpqxx python3 libzmq
CXXFLAGS=-Wall -std=c++14 -I./src -I./include $(shell pkg-config --cflags libpq python3)
LDFLAGS=-L./lib/
LDLIBS=-lm -lrt -lgflags -lboost_system -lpthread -lboost_thread -lzmqpp $(shell pkg-config --libs $(LIBS))

BIN=data-processing

TESTSBIN=test-data-processing

SRCDIR=src/
OBJDIR=obj/
vpath %.cpp $(SRCDIR)
vpath %.h $(SRCDIR)

SRC=data-functions.cpp data-processing.cpp
OBJS = $(addsuffix .o,$(basename $(SRC)))


all: $(BIN)

tests: $(TESTSBIN)

test-data-processing: $(addprefix $(OBJDIR),$(OBJS))
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)


$(BIN): $(addprefix $(OBJDIR),$(OBJS))
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
