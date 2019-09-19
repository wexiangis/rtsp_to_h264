
### arm ###
# cross=arm-linux-gnueabihf-
# CXX=$(cross)g++
# live555Type=armlinux

### ubuntu ###
CXX=g++
live555Type=linux

RPATH=$(shell pwd)

INC += -I$(RPATH)/libs/include/BasicUsageEnvironment
INC += -I$(RPATH)/libs/include/liveMedia
INC += -I$(RPATH)/libs/include/groupsock
INC += -I$(RPATH)/libs/include/UsageEnvironment
LIB += -L$(RPATH)/libs/lib
CFLAGS += -lliveMedia -lgroupsock -lBasicUsageEnvironment -lUsageEnvironment

target:
	@$(CXX) -O3 -Wall -o demo $(RPATH)/rtsp_to_h264.cpp $(RPATH)/h26x_sps_dec.c $(INC) $(LIB) $(CFLAGS)

live555:
	@tar -xzf $(RPATH)/live.2019.08.12.tar.gz -C $(RPATH)/libs && \
	cd $(RPATH)/libs/live && \
	sed -i "s\CROSS_COMPILE?=		arm-elf-\CROSS_COMPILE?=$(cross)\g" ./config.armlinux && \
	./genMakefiles $(live555Type) && \
	chmod 777 ./* -R && \
	sed -i "s\/usr/local\$(RPATH)/libs\g" ./BasicUsageEnvironment/Makefile && \
	sed -i "s\/usr/local\$(RPATH)/libs\g" ./liveMedia/Makefile && \
	sed -i "s\/usr/local\$(RPATH)/libs\g" ./liveMedia/Makefile.head && \
	sed -i "s\/usr/local\$(RPATH)/libs\g" ./mediaServer/Makefile && \
	sed -i "s\/usr/local\$(RPATH)/libs\g" ./mediaServer/Makefile.tail && \
	sed -i "s\/usr/local\$(RPATH)/libs\g" ./proxyServer/Makefile && \
	sed -i "s\/usr/local\$(RPATH)/libs\g" ./proxyServer/Makefile.tail && \
	sed -i "s\/usr/local\$(RPATH)/libs\g" ./testProgs/Makefile && \
	sed -i "s\/usr/local\$(RPATH)/libs\g" ./testProgs/Makefile.tail && \
	sed -i "s\/usr/local\$(RPATH)/libs\g" ./UsageEnvironment/Makefile && \
	sed -i "s\/usr/local\$(RPATH)/libs\g" ./UsageEnvironment/Makefile.head && \
	sed -i "s\/usr/local\$(RPATH)/libs\g" ./groupsock/Makefile && \
	sed -i "s\/usr/local\$(RPATH)/libs\g" ./groupsock/Makefile.head && \
	make -j4 && make install && \
	cd - && \
	rm $(RPATH)/libs/live -rf

clean:
	@rm -rf ./demo

cleanall:
	@rm -rf ./libs/* ./demo


