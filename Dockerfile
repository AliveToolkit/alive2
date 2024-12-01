FROM ubuntu:24.04

RUN apt-get update -qq && apt-get dist-upgrade -qq
RUN DEBIAN_FRONTEND="noninteractive" apt-get -y install tzdata
# Setup base dependencies:
RUN apt-get install -y python3 cmake g++ git ninja-build redis redis-server libhiredis-dev libbsd-resource-perl libredis-perl re2c libgtest-dev z3

RUN git clone --depth=1 https://github.com/llvm/llvm-project $HOME/llvm
RUN mkdir $HOME/llvm/build && cd $HOME/llvm/build && \
cmake -G Ninja -DLLVM_ENABLE_RTTI=ON -DLLVM_ENABLE_EH=ON -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo -DLLVM_ENABLE_ASSERTIONS=ON -DLLVM_ENABLE_PROJECTS="llvm;clang" $HOME/llvm/llvm
RUN cd $HOME/llvm/build && ninja

RUN git clone --depth=1 https://github.com/AliveToolkit/alive2 $HOME/alive2
RUN mkdir $HOME/alive2/build && cd $HOME/alive2/build && \
  cmake -G Ninja -DLLVM_DIR=$HOME/llvm/build/lib/cmake/llvm -DBUILD_TV=1 -DCMAKE_BUILD_TYPE=RelWithDebInfo $HOME/alive2
RUN cd $HOME/alive2/build && ninja && ninja check
