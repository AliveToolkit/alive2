FROM ubuntu:24.04

# We install some useful packages.
RUN apt-get update -qq
RUN DEBIAN_FRONTEND="noninteractive" apt-get -y install tzdata
# Setup base dependencies:
RUN apt-get install -y vim-tiny clang-format sudo python3 wget cmake g++ git clang linux-tools-generic ninja-build lldb

# Setup project-specific dependencies:
RUN apt-get -y install cmake ninja-build gcc-10 g++-10 redis redis-server libhiredis-dev libbsd-resource-perl libredis-perl re2c libgtest-dev z3

RUN git clone --depth=1 https://github.com/zhengyang92/llvm.git $HOME/llvm
RUN mkdir $HOME/llvm/build && cd $HOME/llvm/build && \
cmake -G Ninja -DLLVM_ENABLE_RTTI=ON -DLLVM_ENABLE_EH=ON -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo -DLLVM_TARGETS_TO_BUILD=X86 -DLLVM_ENABLE_ASSERTIONS=ON -DLLVM_ENABLE_PROJECTS="llvm;clang" $HOME/llvm/llvm
RUN cd $HOME/llvm/build && ninja # clang

# To fetch and build the Alive2 with the semantic for intrinsics, use the following command.
RUN git clone --depth=1 https://github.com/minotaur-toolkit/alive2-intrinsics.git $HOME/alive2-intrinsics
RUN mkdir $HOME/alive2-intrinsics/build && cd $HOME/alive2-intrinsics/build && \
  cmake -G Ninja -DLLVM_DIR=$HOME/llvm/build/lib/cmake/llvm -DBUILD_TV=1 -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_BUILD_TYPE=RelWithDebInfo $HOME/alive2-intrinsics
RUN cd $HOME/alive2-intrinsics/build && ninja

# To build Minotaur, use the following command.
RUN git clone --depth=1 https://github.com/minotaur-toolkit/minotaur.git $HOME/minotaur
RUN mkdir $HOME/minotaur/build && cd $HOME/minotaur/build && \
  cmake $HOME/minotaur -DALIVE2_SOURCE_DIR=$HOME/alive2-intrinsics -DALIVE2_BUILD_DIR=$HOME/alive2-intrinsics/build -DCMAKE_PREFIX_PATH=$HOME/llvm/build -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_BUILD_TYPE=RelWithDebInfo -G Ninja
RUN cd $HOME/minotaur/build && ninja
