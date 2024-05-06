# uvio

C++20 coroutines + libuv

## 示例

TODO

## 原始目标

- 尝试复用基于回调的网络库(libuv)基础设施, 使用C++20协程做一层封装, 提供符合直觉的接口.
- 探索能否实现以及实现的难易程度、两种方式实际使用上的对比.

## 开始

```shell
meson setup build
meson compile -C build
```

## 编译器版本

clang (>= 10.0.0) 或 gcc (>= 10.3)

## 致谢

项目设计与实现参考了以下开源项目, 在此表示感谢!

- [Zedio](https://github.com/8sileus/zedio): A runtime for writing asychronous applications with Modern C++, based on C++20 coroutine and liburing (io-uring)
- [async_simple](https://github.com/alibaba/async_simple): Simple, light-weight and easy-to-use asynchronous components
