## 协议开发环境搭建

#### 准备工作

客户端安装 Qt 5

服务器端准备好VirtualBox源码及其开发环境

在客户端和服务端主机上，从版本库中检出SVP协议源码

>	SVP协议版本库：采用git，使用gitlab 当做Web UI

>	版本库软件运行在192.168.11.241上的一台虚拟机gitlab，

>	开机后占用IP 192.168.11.242与192.168.0.242

>	该虚拟机用户名: bitnami，密码: bitnami
	
>	gitlab管理员用户名: wangkai，密码: elffire1
 	 

#### 服务器端构建及运行步骤

1. 编译SVP协议基础库
	
	`cmake -DBUILD_SVP_SERVER=ON -DBUILD_SVP_CLIENT=OFF -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=/usr`

	`make`

	`make install`	

2. 创建符号链接

	`ln -sv [SVP 源码目录]/vbox [VBox 源码目录]/src/VBox/ExtPacks/SVP`

3. 修改Makefile

 	编辑 `[VBox 源码目录]/src/VBox/ExtPacks/Makefile.kmk`

 	在 `include $(FILE_KBUILD_SUB_FOOTER)` 该行之前	

 	添加 `include $(PATH_SUB_CURRENT)/SVP/Makefile.kmk`

4. 编译VirtualBox

5. 配置某台虚拟机使用该扩展

	./VBoxManage modifyvm [虚拟机] --vrdeextpack SVP

6. 打开虚拟机

#### 客户端构建及运行步骤

1. Qt Creator打开 `[SVP 源码目录]/CMakeLists.txt`

2. 执行cmake，指定参数	
	`-DBUILD_SVP_SERVER=OFF -DBUILD_SVP_CLIENT=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=[客户端安装路径]`

3. 修改项目构建选项，在`make`后，添加`make install`步骤

4. 构建

5. 修改项目运行选项，指定为 `[客户端安装路径]/SvpClient.exe`
