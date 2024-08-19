# !/bin/bash
# brew install yasm pkg-config for mac

# 在 FFmpeg 源代码目录中运行配置脚本。
./configure --enable-debug=3 --disable-optimizations --disable-stripping

# 使用 make 命令编译源代码。
make -j$(sysctl -n hw.logicalcpu)
