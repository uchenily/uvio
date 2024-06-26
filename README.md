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

<details open><summary>tcp echo server</summary>

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

</details>

<details open><summary>http server</summary>

```cpp
#include "uvio/net/http.hpp"

using namespace uvio::net::http;

auto main() -> int {
    HttpServer server{"0.0.0.0", 8000};
    server.add_route("/", [](const HttpRequest &req, HttpResponse &resp) {
        resp.body = std::format("{} {}\r\nhello", req.method, req.uri);
    });
    server.add_route("/test", [](const HttpRequest &req, HttpResponse &resp) {
        resp.body = std::format("{} {}\r\ntest route", req.method, req.uri);
    });
    server.run();
}
```

```bash
# 客户端
curl localhost:8000/ -v
```

</details>

<details open><summary>websocket server/client</summary>

```cpp
#include "uvio/net/http.hpp"
#include "uvio/net/websocket.hpp"

using namespace uvio::net::http;
using namespace uvio::net;

auto process_message(websocket::WebsocketFramed &channel) -> Task<> {
    // Ignore errors
    for (int i = 0; i < 64; i++) {
        auto msg = (co_await channel.recv()).value();
        LOG_INFO("Received: `{}`", std::string_view{msg.data(), msg.size()});

        co_await channel.send(msg);
    }
    co_await channel.close();
    co_return;
}

auto main() -> int {
    websocket::WebsocketServer server{"0.0.0.0", 8000};
    server.add_route("/", [](const HttpRequest &req, HttpResponse &resp) {
        resp.body = std::format("{} {}\r\nhello", req.method, req.uri);
    });
    server.handle_message(process_message);
    server.run();
}
```

```cpp
#include "uvio/net/websocket.hpp"

using namespace uvio::net;
using namespace uvio;

auto process_message(websocket::WebsocketFramed &channel) -> Task<> {
    // Ignore errors
    for (int i = 0; i < 64; i++) {
        auto msg = std::format("test message {} from client", i);
        co_await channel.send(msg);

        auto msg2 = (co_await channel.recv()).value();
        LOG_INFO("Received: `{}`", std::string_view{msg2.data(), msg2.size()});
    }
    co_await channel.close();
    co_return;
}

auto main() -> int {
    websocket::WebsocketClient client{"127.0.0.1", 8000};
    client.handle_message(process_message);
    client.run();
}
```

</details>

<details><summary>socks5 server</summary>

完整代码请查看 [socks5_server.cpp](./examples/socks5_server.cpp)

```cpp
auto main() -> int {
    socks5::Socks5Server proxy{"0.0.0.0", 1080};
    proxy.run();
}
```

在使用浏览器情况下, 可以安装 `SwitchyOmega` 插件测试

在终端下可以使用 curl 命令进行测试: `curl -v http://www.baidu.com/ --proxy socks5://127.0.0.1:1080`

</details>

## 功能

- [x] 异步TCP
- [ ] 异步UDP
- [x] 异步DNS解析
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

## 参与开发

如果有使用上的疑问或者任何ideas, 欢迎通过 [Issues](https://github.com/uchenily/uvio/issues) 或者 [Discussions](https://github.com/uchenily/uvio/discussions) 分享

## 致谢

项目设计与实现参考了以下开源项目, 在此表示感谢!

- [Zedio](https://github.com/8sileus/zedio): A runtime for writing asychronous applications with Modern C++, based on C++20 coroutine and liburing (io-uring)
- [async_simple](https://github.com/alibaba/async_simple): Simple, light-weight and easy-to-use asynchronous components
- [ws28](https://github.com/Matheus28/ws28): C++17 WebSocket server library (uses libuv)
