
CXX=g++

INC += -I./libs/include/BasicUsageEnvironment
INC += -I./libs/include/liveMedia
INC += -I./libs/include/groupsock
INC += -I./libs/include/UsageEnvironment
LIB += -L./libs/lib
CFLAGS += -lliveMedia -lgroupsock -lBasicUsageEnvironment -lUsageEnvironment

target:
	@$(CXX) -O3 -Wall -o app rtsp_to_h264.cpp $(INC) $(LIB) $(CFLAGS)

clean:
	@rm -rf ./app


