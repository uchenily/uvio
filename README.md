# uvio

![GitHub License](https://img.shields.io/github/license/uchenily/uvio)
![Static Badge](https://img.shields.io/badge/c%2B%2B20-Coroutines-orange)
![Static Badge](https://img.shields.io/badge/standard-c%2B%2B20-blue?logo=cplusplus)
![GitHub top language](https://img.shields.io/github/languages/top/uchenily/uvio)
![GitHub Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/uchenily/uvio/ci.yaml)

C++20 coroutines + libuv

## 示例

TODO

## 功能

- [ ] 异步TCP
- [ ] 异步UDP
- [ ] 异步DNS解析
- [ ] 异步文件IO
- [x] 计时器
- [ ] TTY
- [ ] 管道
- [ ] 信号处理
- [ ] 子进程
- [ ] 线程池
- [ ] 同步原语

## 原始目标

- 尝试复用基于回调的网络库(libuv)基础设施, 使用C++20协程做一层封装, 提供符合直觉的接口.
- 探索能否实现以及实现的难易程度、两种方式实际使用上的对比.

## 开始

```shell
meson setup build
meson compile -C build
```

## 编译器版本

clang (>= 17) 或 gcc (>= 13)

## 致谢

项目设计与实现参考了以下开源项目, 在此表示感谢!

- [Zedio](https://github.com/8sileus/zedio): A runtime for writing asychronous applications with Modern C++, based on C++20 coroutine and liburing (io-uring)
- [async_simple](https://github.com/alibaba/async_simple): Simple, light-weight and easy-to-use asynchronous components
