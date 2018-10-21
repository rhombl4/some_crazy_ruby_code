EXES := sim light full dep_test void recon gvoid_sim gvoid conv_slice merge_fr vanish line
SRCS := \
	command.cc \
	conv_slice.cc \
	dep.cc \
	dep_test.cc \
	gvoid.cc \
	gvoid_sim.cc \
	light.cc \
	line.cc \
	full.cc \
	full_main.cc \
	matrix.cc \
	recon.cc \
	scheduler.cc \
	sim.cc \
	state.cc \
	vanish.cc \
	void.cc \
	void2.cc \
	void_main.cc

OBJS := $(SRCS:.cc=.o)
CXXFLAGS := -g -W -Wall -MMD -MP -O2

all: test

test: $(EXES)
	./dep_test
	./sim models/LA001_tgt.mdl test.nbt --test
	./full fmodels/FA010_tgt.mdl x.nbt 20 --test
#	./void fmodels/FD074_src.mdl y.nbt 39 --test

conv_slice: conv_slice.o command.o
	$(CXX) $(CXXFLAGS) $^ -o $@

sim: sim.o matrix.o command.o state.o
	$(CXX) $(CXXFLAGS) $^ -o $@

merge_fr: merge_fr.o matrix.o command.o state.o
	$(CXX) $(CXXFLAGS) $^ -o $@

gvoid_sim: gvoid_sim.o matrix.o dep.o
	$(CXX) $(CXXFLAGS) $^ -o $@

light: light.o matrix.o command.o state.o
	$(CXX) $(CXXFLAGS) $^ -o $@

full: full.o full_main.o matrix.o command.o state.o scheduler.o
	$(CXX) $(CXXFLAGS) $^ -o $@

void: void.o void2.o void_main.o matrix.o command.o state.o dep.o scheduler.o
	$(CXX) $(CXXFLAGS) $^ -o $@

vanish: vanish.o matrix.o command.o state.o dep.o scheduler.o
	$(CXX) $(CXXFLAGS) $^ -o $@

line: line.o matrix.o command.o state.o dep.o scheduler.o
	$(CXX) $(CXXFLAGS) $^ -o $@

gvoid: gvoid.o gvoid_main.o matrix.o command.o state.o dep.o scheduler.o
	$(CXX) $(CXXFLAGS) $^ -o $@

recon: recon.o void.o void2.o full.o matrix.o command.o state.o dep.o scheduler.o
	$(CXX) $(CXXFLAGS) $^ -o $@

dep_test: dep.o dep_test.o matrix.o
	$(CXX) $(CXXFLAGS) $^ -o $@

$(OBJS): %.o: %.cc Makefile
	$(CXX) -c $(CXXFLAGS) $< -o $@

-include *.d
