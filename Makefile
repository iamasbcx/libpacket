CC=g++
CXXFLAGS=-O3 -std=c++11 -Wall -pedantic -D_GNU_SOURCE -I. -fPIC

LDFLAGS=-shared

MAKEDEPEND=${CC} -MM
LIBRARY=libpacket.so

OBJS = pcap/reader.o pcap/ip/analyzer.o \
       net/ip/fragmented_packet.o net/ip/fragmented_packets.o net/ip/parser.o \
       net/ip/packets.o

DEPS:= ${OBJS:%.o=%.d}

all: $(LIBRARY)

${LIBRARY}: ${OBJS}
	${CC} ${OBJS} ${LIBS} -o $@ ${LDFLAGS}

clean:
	rm -f ${LIBRARY} ${OBJS} ${DEPS}

${OBJS} ${DEPS} ${LIBRARY} : Makefile

.PHONY : all clean

%.d : %.cpp
	${MAKEDEPEND} ${CXXFLAGS} $< -MT ${@:%.d=%.o} > $@

%.o : %.cpp
	${CC} ${CXXFLAGS} -c -o $@ $<

-include ${DEPS}