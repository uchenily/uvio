# Benchmark

## 测试环境

cpu: Intel(R) Xeon(R) CPU E5-2660 v3 @ 2.60GHz
kernel: 6.8.8-arch1-1
gcc: (GCC) 14.1.1 20240507
wrk: 4.2.0 [epoll] Copyright (C) 2012 Will Glozer


```shell
$ date
Thu May 16 04:30:21 PM CST 2024
$ wrk -t4 -c100 -d30s --latency http://127.0.0.1:8000
Running 30s test @ http://127.0.0.1:8000
  4 threads and 100 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   559.12us  191.85us  29.45ms   95.54%
    Req/Sec    43.70k     3.99k  132.05k    83.85%
  Latency Distribution
     50%  540.00us
     75%  564.00us
     90%  615.00us
     99%    1.09ms
  5222221 requests in 30.10s, 443.25MB read
Requests/sec: 173498.54
Transfer/sec:     14.73MB
```

> 备注: 服务端单线程
