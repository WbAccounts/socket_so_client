CC=gcc
CXX=g++
AR=ar
DEBUG=1

###########################################################
CPPFLAGS += -D__QAXVERSION__="\"${VER}\""
CPPFLAGS += -D__REVISION__="\"${REVISION}\""

ifeq ($(DEBUG), 1)
	CPPFLAGS += -Werror -Wno-deprecated-declarations
else
	GCC_VERSION = $(shell gcc -dumpversion | sed -e 's/\.\([0-9][0-9]\)/\1/g' -e 's/\.\([0-9]\)/0\1/g' -e 's/^[0-9]\{3,4\}$$/&00/')
	ifeq ($(shell expr $(GCC_VERSION) \> 40102), 1)
		ifeq ($(ARCH), arm64)
			CPPFLAGS += -Wall -Wattributes
		else
			CPPFLAGS += -Werror -Wno-deprecated-declarations
		endif
	else
		CPPFLAGS += -Wall
	endif
endif

CPPFLAGS += -fPIC -pthread -std=c++14 
CPPFLAGS += -ffunction-sections -fdata-sections -fvisibility=hidden -fno-strict-aliasing

ifeq ($(DEBUG), 1)
    CPPFLAGS += -g3 -DDEBUG
else
    CPPFLAGS += -g3 -O3 -Os -DNDEBUG
endif

##########################################################################
ROOTDIR=./
COMMONDIR=./common/
DEPENDLIBSDIR=./common/dependlibs/
TESTDIR=./test/

##########################################################################
INCPATH += -I${ROOTDIR}/
INCPATH += -I${COMMONDIR}/
INCPATH += -I${COMMONDIR}/ASFramework/
INCPATH += -I${COMMONDIR}/log/
INCPATH += -I${DEPENDLIBSDIR}/
INCPATH += -I${DEPENDLIBSDIR}/libjsoncpp/include
INCPATH += -I${DEPENDLIBSDIR}/libjsoncpp/include/jsoncpp
CPPFLAGS += $(INCPATH)

##########################################################################
LIBS += -Wl,--as-needed -Wl,-Bsymbolic
LIBS += -Wl,--hash-style=sysv
LIBS += ${DEPENDLIBSDIR}/CentOS_jsoncpp/libjsoncpp.a
# LIBS += ${DEPENDLIBSDIR}/minizip/libminizip.a
# LIBS += ${DEPENDLIBSDIR}/libz/lib/libz.a
LIBS += -lrt -lpthread -ldl

##########################################################################
# VERSION_OBJS = ${COMMONDIR}/compile_info.o
# SO_OBJS += $(VERSION_OBJS)

# COMMON_OBJS += ${COMMONDIR}/ASFramework/util/ASLogImpl.o
COMMON_OBJS += ${COMMONDIR}/AsFramework/util/ASBase64.o
COMMON_OBJS += ${COMMONDIR}/utils/file_utils.o
COMMON_OBJS += ${COMMONDIR}/utils/proc_info_utils.o
COMMON_OBJS += ${COMMONDIR}/qh_thread/thread.o
COMMON_OBJS += ${COMMONDIR}/qh_thread/multi_thread.o
COMMON_OBJS += ${COMMONDIR}/cpu_limit/cpu_limit_mgr.o
COMMON_OBJS += ${COMMONDIR}/cpu_limit/src/actor.o
COMMON_OBJS += ${COMMONDIR}/cpu_limit/src/cpu_limit.o
COMMON_OBJS += ${COMMONDIR}/cpu_limit/src/process.o
# COMMON_OBJS += ${COMMONDIR}/log/log.o
# COMMON_OBJS += ${COMMONDIR}/uuid.o
# COMMON_OBJS += ${COMMONDIR}/ini_parser.o
COMMON_OBJS += ${COMMONDIR}/json/cJSON.o
COMMON_OBJS += ${COMMONDIR}/socket/socket_utils.o
SO_OBJS+=$(COMMON_OBJS)

SOCKET_CLIENT_OBJS += init_helper.o
SOCKET_CLIENT_OBJS += msg_handle.o
SOCKET_CLIENT_OBJS += socket_mgr.o
SOCKET_CLIENT_OBJS += socket_pool.o
SO_OBJS+=$(SOCKET_CLIENT_OBJS)

TEST_OBJS += ${TESTDIR}/test.o
TEST_OBJS += ${TESTDIR}/test_utils.o
TEST_OBJS += ${TESTDIR}/test_class_1.o
TEST_OBJS += ${TESTDIR}/test_class_2.o

#########################################################################
SO_TARGET = libSocketClientMgr.so
TEST_TARGET = test_client

.PHONY:all
all: depend_libs $(SO_TARGET) $(TEST_TARGET) check

depend_libs:
	cd common/dependlibs && chmod +x ./build.sh && ./build.sh tar sirius_socket_client

$(SO_TARGET) : $(SO_OBJS)
	$(CXX) $(CPPFLAGS) -shared -o $(SO_TARGET) $(SO_OBJS) $(LIBS)

$(TEST_TARGET) : $(SO_OBJS) $(TEST_OBJS)
	$(CXX) $(CPPFLAGS) -o $(TEST_TARGET) $(SO_OBJS) $(TEST_OBJS) $(LIBS)

check:
	nm $(SO_TARGET) | grep -v "GLIB" | grep -v "CXX" | grep -v "GCC" | grep " U " || echo "make success."

.PHONY:clean
clean:
	rm -f $(SO_TARGET) $(TEST_TARGET) $(SO_OBJS) $(TEST_OBJS)

