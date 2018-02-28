## Spice 分析

### Spice 架构

![](images/spice-arch.png)

**QXL Driver**

虚拟机中图形显示驱动

**VDIPort**

虚拟机操作系统中图形显示驱动的Miniport driver部分与虚拟显卡交互的通道。

**QXL Device**

qemu-kvm中实现的虚拟显卡。

**Spice Server**

处理客户端连接请求，安全认证。

处理QXL指令（包括绘图指令、位图、音频等），压缩，传给客户端。

处理远程客户端的键盘鼠标输入事件。

----

### Spice Server 架构

![](images/spice-server-arch.png)

Spice Server是一个动态链接库，被qemu加载。

**SpiceCoreInterface**

网络事件处理循环在qemu中，Spice抽象了个SpiceCoreInterface接口，包装套接字监听的添加，事件回调函数的注册，定时器的注册等功能。在初始化Spice Server服务器时，将该接口传给Spice Server使用。

**QXLInterface**

接口抽象了QXL光标图形指令的获取等功能，由qemu初始化该接口，之后传给Spice Server使用。SpiceMouseInterface，SpiceKbdInterface等接口的处理方式类似。

**与VirtualBox VRDE对比**

VirtualBox提供了VRDECALLBACKS接口处理键鼠事件输入，这点类似于Spice。

VirtualBox对GDI指令的处理和Spice不同，VirtualBox是push的方式，自己提供了一个线程获取指令，然后调用VRDE扩展包中开发者注册的回调，Spice如上所述，是采用开发者自己pull的方式。

### Spice Server 初始化流程

![](images/spice-channel-create.png)

1. 创建socket，绑定到指定端口，调用qemu提供的SpiceCoreInterface接口中的watch_add函数，让qemu监听该端口。
2. 有客户端连入时，qemu回调我们在watch_add时注册的回调函数。
3. Spice Server accept一个socket。 
4. 初始化主通道，主通道使用该socket和远程客户端通信。
5. 客户端和服务器之间身份认证。
6. 客户端和服务器之间网络带宽，硬件处理能力，支持的协议版本等信息协商。
7. 客户端发送`MAIN_CHANNEL_INIT`消息。
8. 服务器端返回`MAIN_CHANNEL_LIST`消息，即通道列表信息到客户端。。
9. 客户端根据其返回值，每一个通道建立一个连接，发送`XXX_CHANEL_INIT`消息。
10. 服务器accept，watch_add，接收`INIT`消息，返回`XXX_CHANNEL_ACK`消息，通道创建成功。

### Spice 图形通道处理流程

![](images/spice-graphic.png)

**Red Dispatcher**

 负责与Red Worker通信，如：通道创建终止事件通知，压缩算法设置等。

**Red Worker**

后台工作线程，获取QXL命令，优化，压缩等等工作。

**处理流程**

1. qemu创建了QXLInterface，之后调用spice_add_interface将其传给Spice Server使用。
2. Spice Server创建了Red Dispatcher用于与Red Worker通信，
3. Red Dispatcher初始化了QXLWorker接口给qemu的QXL Device使用。
4. Red Worker干QXL指令获取，优化，视频区域检测，有损无损压缩等等工作。它定时做如下事情：
	1. 处理Red Dispatcher通知他的事件。
	2. 处理各种定时器到期事件
	3. 处理最多50个QXL光标相关指令。
		1. 调用QXLInterface接口获取QXL光标相关指令。
		2. 压缩。
		3. 将其放到光标通道的输出队列。
	4. 处理最多50个QXL GDI相关指令。
		1. 调用QXLInterface接口获取QXL GDI相关指令。
		2. 遮挡部分剔除。
		3. 检测可能视频区域。
	 	4. 加入到显示通道的输出队列。
	5. 尝试将光标，显示通道输出队列积压的数据发送到客户端。 
	> 当本轮循环处理耗时超过10ms，或者向客户端发送数据阻塞时，立马开始下轮处理循环。
	> 否则等待某事件发生或10ms超时后执行下轮数据处理循环。  
