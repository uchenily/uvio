# uvio

![GitHub License](https://img.shields.io/github/license/uchenily/uvio)
![Static Badge](https://img.shields.io/badge/c%2B%2B20-Coroutines-orange)
![Static Badge](https://img.shields.io/badge/standard-c%2B%2B20-blue?logo=cplusplus)
![GitHub top language](https://img.shields.io/github/languages/top/uchenily/uvio)
[![Actions Status](https://github.com/uchenily/uvio/actions/workflows/linux.yaml/badge.svg?branch=main)](https://github.com/uchenily/uvio/actions)
[![Actions Status](https://github.com/uchenily/uvio/actions/workflows/macos.yaml/badge.svg?branch=main)](https://github.com/uchenily/uvio/actions)
[![Actions Status](https://github.com/uchenily/uvio/actions/workflows/windows.yaml/badge.svg?branch=main)](https://github.com/uchenily/uvio/actions)


C++20 coroutines + libuv

## 示例

tcp echo server

```c++
#include "uvio/core.hpp"
#include "uvio/net.hpp"

using namespace uvio;
using namespace uvio::net;

// Ignore errors
auto process(TcpStream stream) -> Task<> {
    while (true) {
        std::array<char, 1024> buf{};

        if (auto ret = co_await stream.read(buf); !ret) {
            console.error(ret.error().message());
            break;
        }
        console.info("Received: `{}`", buf.data());
        if (auto ret = co_await stream.write(buf); !ret) {
            console.error(ret.error().message());
            break;
        }
    }
    co_return;
}

auto server() -> Task<> {
    std::string host{"0.0.0.0"};
    int         port{8000};

    auto listener = TcpListener();
    listener.bind(host, port);
    console.info("Listening on {}:{} ...", host, port);
    while (true) {
        auto stream = (co_await listener.accept()).value();
        spawn(process(std::move(stream)));
    }
}

auto main() -> int {
    block_on(server());
}
```

```bash
# 客户端
telnet localhost 8000
# 或者
nc localhost 8000 -v
```

## 功能

- [x] 异步TCP
- [ ] 异步UDP
- [ ] 异步DNS解析
- [ ] 异步文件IO
- [x] 计时器
- [ ] TTY
- [ ] 管道
- [ ] 信号处理
- [ ] 子进程
- [x] 线程池
- [ ] 同步原语

## 原始目标

- 尝试复用基于回调的网络库(libuv)基础设施, 使用C++20协程做一层封装, 提供符合直觉的接口.
- 探索能否实现以及实现的难易程度、两种方式实际使用上的对比.

## 开始

```shell
meson setup build
meson compile -C build
```

## 安装

```shell
meson install -C build [--destdir xxx_path]
```

或者直接拷贝目录:

```shell
cp -r uvio xxx_path
```

## 编译器版本

clang (>= 17) 或 gcc (>= 13) 或 apple clang (>= 15) 或 MSVC (>= 19)

## 基准测试

[benchmark](./benchmark/benchmark.md)

## 致谢

项目设计与实现参考了以下开源项目, 在此表示感谢!

- [Zedio](https://github.com/8sileus/zedio): A runtime for writing asychronous applications with Modern C++, based on C++20 coroutine and liburing (io-uring)
- [async_simple](https://github.com/alibaba/async_simple): Simple, light-weight and easy-to-use asynchronous components
- [ws28](https://github.com/Matheus28/ws28): C++17 WebSocket server library (uses libuv)
