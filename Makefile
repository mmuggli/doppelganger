# Change this if the default compiler does not work.
#CXX = g++

# Use 64-bit integers in a 64-bit environment.
SIZE_FLAGS = -DMASSIVE_DATA_RLCSA

# Parallelism is supported by either libstdc++ Parallel Mode or MCSTL.
PARALLEL_FLAGS = #-DMULTITHREAD_SUPPORT -D_GLIBCXX_PARALLEL -fopenmp
# MCSTL_ROOT = /fs-3/d/jltsiren/suds/mcstl
# PARALLEL_FLAGS = -DMULTITHREAD_SUPPORT -I$(MCSTL_ROOT)/c++ -fopenmp

# Vectors using nibble codes instead of delta codes are faster, but they also
# take up more space.
VECTOR_FLAGS = $(PSI_FLAGS) $(LCP_FLAGS) $(SA_FLAGS)
# PSI_FLAGS = -DUSE_NIBBLE_VECTORS
# LCP_FLAGS = -DSUCCINCT_LCP_VECTOR
# SA_FLAGS = -DSUCCINCT_SA_VECTOR
# SDSL_PREFIX = /s/chopin/l/grad/muggli/local
SDSL_STUFF =  -I/usr/include/boost/ -I$(SDSL_PREFIX)/include -L /usr/lib64 -L$(SDSL_PREFIX)/lib  -lsdsl -ldivsufsort -ldivsufsort64

CXXFLAGS = -Wall   -O2 -mmmx -msse -msse2 -mssse3 -msse4.1  -std=c++11 $(SIZE_FLAGS) $(PARALLEL_FLAGS) $(VECTOR_FLAGS) $(SDSL_STUFF)
OBJS = rlcsa.o rlcsa_builder.o sasamples.o alphabet.o \
lcpsamples.o sampler.o suffixarray.o adaptive_samples.o docarray.o \
bits/array.o bits/bitbuffer.o bits/multiarray.o bits/bitvector.o bits/charvector.o bits/deltavector.o \
bits/sdslvector.o bits/rlevector.o bits/nibblevector.o bits/succinctvector.o misc/parameters.o misc/utils.o

PROGRAMS = rlcsa_test lcp_test parallel_build build_rlcsa build_sa locate_test \
display_test document_graph read_bwt extract_sequence rlcsa_grep build_plcp sample_lcp \
sampler_test ss_test utils/extract_text utils/convert_patterns utils/split_text \
utils/sort_wikipedia utils/genpatterns

VPATH = bits:misc:utils

LFLAGS =  #-pie
all: default
	$(MAKE) -C om_set1
	$(MAKE) -C gcsa
	$(MAKE) -C tools

default: parallel_build build_rlcsa rlcsa_test sampler_test display_test document_graph


rlcsa.a: $(OBJS)
	ar rcs rlcsa.a $(OBJS)

depend:
	g++ -MM *.cpp bits/*.cpp misc/*.cpp utils/*.cpp > dependencies.mk

rlcsa_test: rlcsa_test.o rlcsa.a
	$(CXX) $(CXXFLAGS) -o rlcsa_test rlcsa_test.o rlcsa.a

lcp_test: lcp_test.o rlcsa.a
	$(CXX) $(CXXFLAGS) -o lcp_test lcp_test.o rlcsa.a

parallel_build: parallel_build.o rlcsa.a
	$(CXX) $(CXXFLAGS) $(LFLAGS) -o parallel_build parallel_build.o rlcsa.a

build_rlcsa: build_rlcsa.o rlcsa.a
	$(CXX) $(CXXFLAGS) -o build_rlcsa build_rlcsa.o rlcsa.a

build_sa: build_sa.o rlcsa.a
	$(CXX) $(CXXFLAGS) -o build_sa build_sa.o rlcsa.a

locate_test: locate_test.o rlcsa.a
	$(CXX) $(CXXFLAGS) -o locate_test locate_test.o rlcsa.a

display_test: display_test.o rlcsa.a
	$(CXX) $(CXXFLAGS) -o display_test display_test.o rlcsa.a

document_graph: document_graph.o rlcsa.a
	$(CXX) $(CXXFLAGS) -o $@ $@.o rlcsa.a

read_bwt: read_bwt.o rlcsa.a
	$(CXX) $(CXXFLAGS) -o read_bwt read_bwt.o rlcsa.a

extract_sequence: extract_sequence.o rlcsa.a
	$(CXX) $(CXXFLAGS) -o extract_sequence extract_sequence.o rlcsa.a

rlcsa_grep: rlcsa_grep.o rlcsa.a
	$(CXX) $(CXXFLAGS) -o rlcsa_grep rlcsa_grep.o rlcsa.a

build_plcp: build_plcp.o rlcsa.a
	$(CXX) $(CXXFLAGS) -o build_plcp build_plcp.o rlcsa.a

sample_lcp: sample_lcp.o rlcsa.a
	$(CXX) $(CXXFLAGS) -o sample_lcp sample_lcp.o rlcsa.a

sampler_test: sampler_test.o rlcsa.a
	$(CXX) $(CXXFLAGS) -o sampler_test sampler_test.o rlcsa.a

ss_test: ss_test.o rlcsa.a
	$(CXX) $(CXXFLAGS) -o ss_test ss_test.o rlcsa.a

extract_text: extract_text.o
	$(CXX) $(CXXFLAGS) -o utils/extract_text extract_text.o

convert_patterns: convert_patterns.o utils.o
	$(CXX) $(CXXFLAGS) -o utils/convert_patterns convert_patterns.o misc/utils.o

split_text: split_text.o utils.o
	$(CXX) $(CXXFLAGS) -o utils/split_text split_text.o misc/utils.o

sort_wikipedia: sort_wikipedia.o utils.o
	$(CXX) $(CXXFLAGS) -o utils/sort_wikipedia sort_wikipedia.o misc/utils.o

genpatterns: genpatterns.c
	gcc -O3 -Wall -o utils/genpatterns utils/genpatterns.c

clean:
	rm -f rlcsa.a
	rm -f $(PROGRAMS)
	rm -f *.o bits/*.o misc/*.o utils/*.o
	$(MAKE) -C gcsa clean
	$(MAKE) -C om_set1 clean
	$(MAKE) -C tools clean

package:
	mkdir rlcsa
	mkdir rlcsa/bits rlcsa/misc rlcsa/utils
	cp LICENSE Makefile README dependencies.mk *.cpp *.h targz rlcsa
	cp bits/*.cpp bits/*.h rlcsa/bits
	cp misc/*.cpp misc/*.h rlcsa/misc
	cp utils/*.cpp utils/*.py rlcsa/utils
	./targz rlcsa
	rm -r rlcsa/*
	rmdir rlcsa

deploy:
	find . -type d -exec chmod 770 {} \;
	find . -type f -not \( -perm +111 \) -exec chmod 660 {} \;
	find . -type f -perm +111 -exec chmod 770 {} \;


include dependencies.mk
