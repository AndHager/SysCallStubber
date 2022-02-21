FROM ubuntu:20.04
LABEL version="1.0"
LABEL description="Image for syscall stubbing"
# apt prompts
ARG DEBIAN_FRONTEND=noninteractive
ARG EXECUTION_TIME=3600
RUN apt-get update
# install deps
RUN apt-get install -y libcap-dev build-essential cmake clang pkg-config libmount-dev python3.8 python3-pip git zip wget dbus ninja-build meson m4 gperf libseccomp-dev gcc nano strace cproto

####################################
#### Create directory structure ####
####################################
WORKDIR /
    RUN mkdir normal
    RUN mkdir static_log
    RUN mkdir static_repl
    RUN mkdir dynamic
    RUN mkdir info

    WORKDIR /info
        RUN touch ReturnValues.json
        RUN chmod ug+rw ReturnValues.json

        RUN mkdir normal
        WORKDIR /info/normal
            RUN mkdir out

    WORKDIR /info
        RUN mkdir static_log
        WORKDIR /info/static_log
            RUN mkdir stubbed
            RUN mkdir out

    WORKDIR /info
        RUN mkdir static_repl
        WORKDIR /info/static_repl
            RUN mkdir stubbed
            RUN mkdir out

    WORKDIR /info
        RUN mkdir dynamic
        WORKDIR /info/dynamic
            RUN mkdir stubbed
            RUN mkdir out

#####################################
#### Copy folders of the context ####
#####################################
WORKDIR /
    # Compile clang, clang++, rvstore, ...
    COPY llvm-project /llvm-project
    COPY scripts /scripts
    ARG EXEC_FUZZ="/scripts/execfuzz.sh"

####################################################
#### Build Clang, Passes, rvstore and set paths ####
####################################################
WORKDIR /llvm-project
    RUN ./compile.sh

    # Set Path to compilde clang, clang++, rvstore, ...
    ARG BUILD_PATH="/llvm-project/build"
    ARG BUILD_BIN="$BUILD_PATH/bin"
    ARG BUILD_LIB="$BUILD_PATH/lib"

    ARG CLANGPP="$BUILD_BIN/clang++"
    ARG CLANG="$BUILD_BIN/clang"

    ARG LOAD_FLAGS="-Xclang -load -Xclang"

WORKDIR /
    RUN export ASAN_SYMBOLIZER=$BUILD_BIN/llvm-symbolizer
    RUN export ASAN_OPTIONS=detect_leaks=0

#######################
#### Clone systemd ####
#######################
WORKDIR /normal
    RUN wget -O systemd.zip https://github.com/systemd/systemd/archive/v247.zip
    RUN unzip systemd.zip && mv systemd-247 systemd
    RUN rm systemd.zip
    ## Copy sytemd to all nedded destinations
    RUN cp -r ./systemd /static_log/
    RUN cp -r ./systemd /static_repl/
    RUN cp -r ./systemd /dynamic/

#############################
#### Build & Exec Fuzzer ####
#############################
# Build & Exec normal fuzztargets
WORKDIR /normal/systemd/
    RUN CXX="$CLANGPP" CC="$CLANG" ./tools/oss-fuzz.sh
    RUN $EXEC_FUZZ /normal/systemd /info/normal/out --execution-time $EXECUTION_TIME

# Build & Exec fuzzer for static logging
WORKDIR /static_log/systemd/
    ARG LOAD_STATIC_LOG="$LOAD_FLAGS $BUILD_LIB/LLVMStaticSysCallLog.so"
    ARG CXX_FSL="$CLANGPP $LOAD_STATIC_LOG"
    ARG CC_FSL="$CLANG $LOAD_STATIC_LOG"

    RUN CXX="$CXX_FSL" CC="$CC_FSL" ./tools/oss-fuzz.sh
    RUN $EXEC_FUZZ /static_log/systemd /info/static_log/out --run-parallel False --execution-time $EXECUTION_TIME

# Prepare ReturnValues.json for parsing
WORKDIR /info
    # Remove null characters
    RUN tr < /info/ReturnValues.json -d '\000' > /info/ReturnValues.tmp1
    # Remove duplicated lines in /info/ReturnValues.json
    RUN awk '!seen[$0]++' /info/ReturnValues.tmp1 > /info/ReturnValues.tmp2
    RUN cp /info/ReturnValues.tmp2 /info/ReturnValues.json

# Build & Exec static replaced fuzztargets
WORKDIR /static_repl/systemd/
    ARG  LOAD_STATIC_REPL="$LOAD_FLAGS $BUILD_LIB/LLVMStaticSysCallRepl.so"
    ARG CXX_FSR="$CLANGPP $LOAD_STATIC_REPL"
    ARG CC_FSR="$CLANG $LOAD_STATIC_REPL"

    RUN CXX="$CXX_FSR" CC="$CC_FSR" ./tools/oss-fuzz.sh
    RUN $EXEC_FUZZ /static_repl/systemd /info/static_repl/out --execution-time $EXECUTION_TIME

# Build & Exec dynamic fuzztargets
WORKDIR /dynamic/systemd/
    ARG LOAD_DYNAMIC_MOD="$LOAD_FLAGS $BUILD_LIB/LLVMDynamicSysCallMod.so"
    ARG LDFLAGS_FDM="-g $BUILD_LIB/clang/12.0.1/lib/linux/libclang_rt.rvstore-x86_64.so -Wl,-rpath,$BUILD_LIB/clang/12.0.1/lib/linux" 
    ARG CXX_FDM="$CLANGPP $LOAD_DYNAMIC_MOD"
    ARG CC_FDM="$CLANG $LOAD_DYNAMIC_MOD"

    RUN LDFLAGS="$LDFLAGS_FDM" CXX="$CXX_FDM" CC="$CC_FDM" ./tools/oss-fuzz.sh
    RUN $EXEC_FUZZ /dynamic/systemd /info/dynamic/out --execution-time $EXECUTION_TIME

#######################
#### Run Evaluator ####
#######################
WORKDIR /info
    RUN pip3 install matplotlib
    RUN python3 /scripts/evaluator.py /info --absolute True --execution-time $EXECUTION_TIME
