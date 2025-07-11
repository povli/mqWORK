# 开发环境搭建


### 基本工具

**首先需要以下基本工具：**
- linux系统（开发所用为ubuntu22.04）
- 高于17的`gcc/g++`版本, git, cmake 等

### 安装`protobuf`

是一个序列化和反序列化工具。

安装依赖：
```sh
# centos
sudo yum install autoconf automake libtool curl make gcc-c++ unzip
# ubuntu
sudo apt update
sudo apt install autoconf automake libtool curl make g++ unzip
```
下载`protobuf`包：
```sh
wget https://github.com/protocolbuffers/protobuf/releases/download/v3.20.2/protobuf-all-3.20.2.tar.gz
```
编译安装：
```sh
# 解压
tar -zxf protobuf-all-3.20.2.tar.gz
cd protobuf-3.20.2/
# 运行目录配置脚本
./autogen.sh
# 运行配置脚本
./configure
# 编译(时间较长)
make
# 安装
sudo make install
# 确认是否安装成功
protoc --version
```

### 安装muduo库

#### 注意 ：请将此库安装在tools目录下
下载源代码：

```sh
git clone https://github.com/chenshuo/muduo.git
```

安装依赖：
```sh
# centos
sudo yum install gcc-c++ cmake make zlib zlib-devel boost-devel
# ubuntu
sudo apt update
sudo apt install g++ cmake make zlib1g zlib1g-dev libboost-all-dev
```
编译安装：
```
# ① 构建
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DCMAKE_INSTALL_PREFIX=../_install \
         -DMUDUO_BUILD_EXAMPLES=OFF \
         -DMUDUO_BUILD_TESTS=OFF
make -j$(nproc)

# ② 安装到 tools/muduo/_install/
make install
```

### 安装SQLite3

这是一个轻量级的数据库。

```sh
# centos
sudo yum install sqlite-devel
# ubuntu
sudo apt install sqlite3
# 验证安装
sqlite3 --version
```
