# Partix —— Modern OOP OS
此仓库为Partix系统的内核部分

## 什么是Partix？
Partix是我(们)提出的一个猜想——OOP语言是否可以写操作系统？  
我一直都有一个用Java写操作系统的梦想，但Oracle官方从未做过类似解决方案  
所以我设计了Partic语言——一门运行在裸机上的OOP语言，类似Java  
并开发了一个内核(也就是这个仓库)，我称其为Partix  
目前Partix仍在进展中，并且已经帮助Partic编译器修复了大量bug  
Partix被设计为OOP风格的架构和原生的多平台支持以及简单的贡献性  
在这之前有一个旧版但比较完整的内核，但是由于代码结构过于混乱已被放弃

## 关于这个项目
整个项目使用了**4种语言**  
分别为:
 - Partic —— 内核核心语言
 - C —— 次核心语言，用于底层交互(如Bootloader/HAL等)
 - ASM —— 用于比C更底层的交互
 - Python —— 用于编写构建系统

**bootloader**文件夹存放着各个平台的对应UEFI Bootloader程序实现，用于驱动内核  
**buildsystem**文件夹是这个项目的构建系统，用Python语言编写，平常只需使用python3 build.py(根目录)即可使用  
**hal**文件夹存放着关于硬件抽象层的相关内容，比如底层IO实现，需要注意的是绝大部分抽象层都在kernel本身中，因为Partic被设计为类似Java的高抽象OOP风格  
**kernel**文件夹存放着内核的核心，通常由Partic占主导，Java风格  
**driver**文件夹存放着一些使用非Partic语言编写的设备驱动，它们通常会被Partic直接或间接调用

## 当前进度？
目前仍在架构搭建过程中，尚未可以生产使用(还远得很)

## 需要注意什么
Partic是一门全堆语言，这意味着所有new和绝大多数内容都是堆分配  
你可以在Partic中使用异常系统和绝大多数Java特性，但你必须手动管理内存并且不能有内存泄漏  
你可以调用core stdlib中的Memory类，它会自动桥接到编译时指定的Allocator(在这里是KrAlloc)上  
只需要```Memory.free((long) obj)```你就可以释放一个堆上对象  
如果类实现了Disposable接口(比如Additional Stdlib里的```StrBuilder```)，你还可以直接调用```.dispose()```方法，按照规范，对象应清理自己的资源然后调用```Memory.free```释放自己

## 我该如何进行贡献？
非常抱歉的是Partic编译器和工具链尚未被设计好为可以公开，所以截至目前你没有办法参与到项目的开发中  
除非你不进行kernel相关更改

## 贡献名单
- NekoSora (我)
- CYAN-HEX

## 文档信息
最后更新: 2026/8/28 20:12:52  
最后更新者: NekoSora