# mySocketApp

这是一个简单的使用纯C构建的Socket demo

也是我在学习C语言的过程中几乎是独立完成的第一个项目

## 当前的完成(学习)进度
- [x] 构建基本的socket连接，并且实现数据的双向传输
- [x] 自定义数据包，以数据包的形式发送数据
- [x] 实现文件的发送
- [x] 使用多线程，通过消息队列使文件扫描和文件发送异步执行
- [x] 使用多进程，让服务端使用不同进程处理客户端连接
- [ ] 重新组织项目结构，完善多线程，发送文件时建立多个socket连接
- [ ] 优化显示，做一个进度条

## 如何构建此项目？
首先，此项目只能在`*nix`系统上运行，您需要安装以下软件
```
# ArchLinux
sudo pacman -S git cmake make glibc gcc
```
然后执行以下命令即可
```
git clone https://github.com/2c2048d2/MySocketAPP
cd MySocketAPP
bash scripts/Clean.sh
bash scripts/Build.sh
```

## 食用方法
构建完成之后在out目录下能看到服务端`server`和客户端`client`，首先启动服务端，然后启动客户端，根据提示输入操作即可
