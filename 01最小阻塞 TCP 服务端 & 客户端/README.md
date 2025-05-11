# 如何运行
在终端运行
```bash
make run
```
他会先去运行server，然后一秒后，运行client，然后程序就退出了。

# 运行时候遇到的一些问题
当你运行./server时候提示
```shell
bind failed: Address already in use
```
你需要等一会会时间。

然后你运行./server
```bash
Server listening on port 8888...
```
---

或者，你可以查看端口是不是在被占用
```bash
sudo lsof -i :8888
```
```bash
wu@wu:~/code/网络编程/myWebFileServer$ sudo lsof -i :8888
COMMAND    PID USER   FD   TYPE   DEVICE SIZE/OFF NODE NAME
server  398575   wu    3u  IPv4 11645519      0t0  TCP *:8888 (LISTEN)
```
把进程杀掉
```bash
kill -9 398575(PID)
```
这样就会提示
```bash
wu@wu:~/code/网络编程/myWebFileServer/01最小阻塞 TCP 服务端 & 客户端$ ./server 
Server listening on port 8888...
已杀死
```
你就可以重新运行了。