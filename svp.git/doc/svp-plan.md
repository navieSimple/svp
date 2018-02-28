## **SVP协议计划**

### **1. 开发平台搭建**

- GitLab 版本控制平台搭建。
 
- 代码规范，开发方式约定。
 
- 测试平台搭建

	- 服务器端: 安装 CentOS 6.5 操作系统，编译安装 VirtualBox 4.3.6
	
	- 客户端: **在物理机中**安装 WinXP 和 Win7 双系统，开发测试时客户端直连服务器端。	

### **2. 基本框架设计与实现**

见 [svp-arch.pdf](svp-arch.pdf) ，主要有3部分需实现：

- 多通道。

- 高效可靠的发送。

- 将数据处理挪到工作线程。

### **3. 更多位图格式支持**

目前所有位图都是先转换成32位ARGB格式再做后续处理的。

修改报文格式，添加位图格式和压缩方式的标识。

添加8,16,24位位图支持。

### **4. 单元测试**

**服务器端**

- 挑选单元测试框架。

- 通信部分的单元测试。

**客户端**

- QTestLib/Squish单元测试框架。

- 通信部分的单元测试。

- 绘图部分的单元测试。

### **5. 客户端界面**

- 远程桌面连接

- 主窗口
 
- 内部工作状态显示
 
- 工作方式设置  

### **6. 记录模块**

记录收到的指令和位图，方便离线分析。

### **7. 信息收集模块**

统计压缩比，发送数据量，数据率等，方便后续压缩算法测试和评估。

### **8. BUG修复**

- windows自带字体预览对话框显示错误。

- 右键菜单文字闪烁。

- ...... 

### **9. 文档**

设计文档和报文格式文档。