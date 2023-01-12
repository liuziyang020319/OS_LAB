# 项目介绍
本次实验完成了一个最基础的操作系统引导，并利用这个引导和核心，从而可以执行一些简单的小程序。
## 完成的实验任务与重要BUG
### 实验任务1：第一块引导块的制作
引导块部分实验完成费时许久，主要原因是尚未理解实验的流程。最开始我没有理解如何调用BIOS的命令的方法，而是直接用call某个命令的方法，尝试调用，后来在阅读了指引后意识到实际应该对参数赋值，然后集中调用。只要明白了如何调用bios，打印字符串操作就变得很简单了。
### 实验任务2：开始制作镜像文件
实验任务2主要的要求是改写head.S和前往内核，改写head.S中需要设置C语言运行的栈和清空bss段。

实验任务二部分对我难度比较大，因为我没有学习过汇编语言相关内容。由于最初我对bss段的理解不够深刻，我最初获得bss的起始末位指令代码如下：
```
  ld s0, __bss_start //start of bss
  ld s1, __BSS_END__ //end
```
事实上这么写程序也能正常运行但我发现gdb过程中这些值都是0.在design review 过程中，助教老师告诉我bss有意义的是内存地址的值，应该使用la指令。
```
  la s0, __bss_start //start of bss
  la s1, __BSS_END__ //end
```
### 实验任务3：加载并选择启动多个用户程序之一
实验任务三的难点在于自己书写createimage.c,由于准许将所有的程序padding起来，所以我直接选用每个程序块padding到15个sector，main部分按照程序的id计算得到sector的起始块编号，然后将其load进内存即可调用。

在上版部分我遇到了一个很深刻的bug。最开始的时候，我为app清栈的crt是这么书写的：
```
    addi sp, sp, -4       # 分配空间
    sw ra, 0(sp)          # 储存返回地址
```
在qumu中，这么书写没有问题，但是在上版过程中，应用程序执行异常。这是因为板上烧写的CPU是64位的，所以应该按照64位位宽设置栈。
```
    addi sp, sp, -8         # 分配空间
    sd ra, 0(sp)          # 储存返回地址
```
### 实验任务4：镜像文件压缩
镜像压缩部分难度并不大，我的想法是将APPinfo页附在kernal的后面，在第一个引导块的末2个byte处，存储APPinfo的起始地址和块大小等必要信息，方便读入。在main函数内部，我首先将APPinfo页载入到内存中，然后用bios提供的getchar指令交互得到键盘输入，然后将得到的需要执行的程序名字在load.c中载入。载入的方式和APPinfo类似，我的APPinfo中记录了每个任务的首地址和大小，所以我直接计算得到从SD卡中需要读入的值，然后通过移位操作将其移动到必要的位置。

镜像文件压缩部分我也遇到了奇怪的bug，在最开始的时候createimage.c中我使用的变量全部定义为int类型。观察得到的二进制文件后我发现虽然int类的stream确实移动了32位，但不知道为什么，高16位地址出现了乱码（即本来是全0，但实际上不是，是内存空间里某个地方的值），随后我将int全改成了long，问题解决。

### 实验任务5：批处理运行多个用户程序
本阶段和实验任务4高度重合，难度较小，我只是在create部分添加了个--bat指令的识别，从而判断是否是bat指令。然后我利用taskid是否为-1判断一个文件是否为批处理文件，如果是的话则将批处理文件的内容引导到内存里，然后调用任务4的程序即可。

## 运行代码的流程：
### 脚本运行
```
make all
make run
```
### 或者输入：
```
gcc createimage.c -o createimage
make clean
make dirs
make elf
make asm
cp createimage build/
cp bat1 build/
cp bat2 build/
cd build && ./createimage --extended bootblock main bss auipc data 2048 --bat bat1 bat2 && cd ..
```
这里我添加的特性是--bat指令，它会让镜像制作程序认为后面跟着的文件内部全部存储全是执行某个命令文件的名字。