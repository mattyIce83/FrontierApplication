APP_DIR=../app
CFLAGS  = -I${APP_DIR}
CZMQFLAGS  = `pkg-config --cflags libzmq`
CPBFLAGS  = `pkg-config --cflags protobuf`
LZMQFLAGS   = `pkg-config --libs libzmq`
LPBFLAGS   = `pkg-config --libs protobuf`
LDFLAGS =-L/usr/lib/arm-linux-gnueabihf
LDLIBS=-lprotobuf -lzmq -lstdc++ -lpthread
GG = arm-linux-gnueabihf-gcc
GPP = arm-linux-gnueabihf-g++

default: build

build: pwrmonTest gpioread gpioset rtdmux adcread pwrmon pwrmonrw

pwrmonTest: pwrmonTest.o atm90e26.o
	${GPP} ${CFLAGS} pwrmonTest.o  atm90e26.o ${LDFLAGS} ${LDLIBS} -o pwrmonTest

pwrmonTest.o: pwrmonTest.cpp ${APP_DIR}/atm90e26.h
	${GPP} ${CFLAGS} -c pwrmonTest.cpp

gpioread: gpioread.o
	${GPP} ${CFLAGS} gpioread.o ${LDFLAGS} ${LDLIBS} -o gpioread

gpioread.o: gpioread.cpp
	${GPP} ${CFLAGS} -c gpioread.cpp

gpioset: gpioset.o
	${GPP} ${CFLAGS} gpioset.o ${LDFLAGS} ${LDLIBS} -o gpioset

gpioset.o: gpioset.cpp
	${GPP} ${CFLAGS} -c gpioset.cpp

rtdmux: rtdmux.o
	${GPP} ${CFLAGS} rtdmux.o ${LDFLAGS} ${LDLIBS} -o rtdmux

rtdmux.o: rtdmux.cpp
	${GPP} ${CFLAGS} -c rtdmux.cpp

adcread: adcread.o
	${GPP} ${CFLAGS} adcread.o ${LDFLAGS} ${LDLIBS} -o adcread

adcread.o: adcread.cpp
	${GPP} ${CFLAGS} -c adcread.cpp

pwrmon: pwrmon.o atm90e26.o
	${GPP} ${CFLAGS} pwrmon.o atm90e26.o ${LDFLAGS} ${LDLIBS} -o pwrmon

pwrmon.o: pwrmon.cpp ${APP_DIR}/atm90e26.h
	${GPP} ${CFLAGS} -c pwrmon.cpp

pwrmonrw: pwrmonrw.o atm90e26.o
	${GPP} ${CFLAGS} pwrmonrw.o atm90e26.o ${LDFLAGS} ${LDLIBS} -o pwrmonrw

pwrmonrw.o: pwrmonrw.cpp ${APP_DIR}/atm90e26.h
	${GPP} ${CFLAGS} -c pwrmonrw.cpp

atm90e26.o: ${APP_DIR}/atm90e26.c ${APP_DIR}/atm90e26.h
	${GG} ${CFLAGS} -c ${APP_DIR}/atm90e26.c

clean:
	rm -f *.o *.exe
