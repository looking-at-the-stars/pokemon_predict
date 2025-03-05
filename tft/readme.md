# 驱动tft屏幕

这里使用的屏幕驱动芯片是st7789，内核中自带fbtft驱动可以点亮屏幕，具体操作步骤参考[Luckfox pico 驱动st7789显示屏_rv1106 st7789-CSDN博客](https://blog.csdn.net/nehcrong/article/details/140361929?fromshare=blogdetail&sharetype=blogdetail&sharerId=140361929&sharerefer=PC&sharesource=qq_36547861&sharefrom=from_link)

## 修改dts文件

打开/SDK目录/sysdrv/source/kernel/arch/arm/boot/dts目录，修改对应的dts文件

```
stars@stars-virtual-machine:~/luckfox-pico/sysdrv/source/kernel/arch/arm/boot/dts$ ls | grep luckfox*
rv1103g-luckfox-pico.dts
rv1103g-luckfox-pico-mini.dts
rv1103g-luckfox-pico-plus.dts
rv1103g-luckfox-pico-webbee.dts
rv1103-luckfox-pico-ipc.dtsi
rv1106g-luckfox-pico-max.dts
rv1106g-luckfox-pico-pro.dts
rv1106g-luckfox-pico-pro-max-fastboot.dts
rv1106g-luckfox-pico-ultra.dts
rv1106g-luckfox-pico-ultra-fastboot.dts
rv1106g-luckfox-pico-ultra-w.dts
rv1106-luckfox-pico-pro-max-ipc.dtsi
rv1106-luckfox-pico-ultra-ipc.dtsi
stars@stars-virtual-machine:~/luckfox-pico/sysdrv/source/kernel/arch/arm/boot/dts$ vim rv1103g-luckfox-pico-mini.dts
```

可以参考我给出的dts文件，注意因为我的屏幕没有cs引脚，所以注释了cs相关的代码

注：rv1106-luckfox-pico-pro-max-ipc.dtsi可能会和dts文件的spi0出现重复定义，建议注释掉rv1106-luckfox-pico-pro-max-ipc.dtsi中的spi0

## 修改系统配置

进入内核目录

```
cd /SDK目录/sysdrv/source/kernel
cp ./arch/arm/configs/luckfox_rv1106_linux_defconfig .config
make ARCH=arm menuconfig
```

打开SPI的驱动，framebuffer的驱动以及st7789的驱动

```
Device Drivers  --->  [*] SPI support
Device Drivers  --->  <*> Staging drivers  --->  <*> Support for small TFT LCD display modules  --->  <*> FB driver for the ST7789V LCD Controller
```

保存为defconfig，将defconfig复制到./arch/arm/configs/luckfox_rv1106_linux_defconfig

```
make ARCH=arm savedefconfig
cp defconfig arch/arm/configs/luckfox_rv1106_linux_defconfig
```

## 重新编译镜像

```
cd /SDK目录
./build.sh
```

编译出的镜像重新烧录到开发板

登录到开发板输入以下命令填充随机像素

```
cat /dev/urandom > /dev/fb0
```

如果成功填充随机像素，则成功驱动屏幕。

未成功可以通过dmesg查看st7789或者spi的相应输出内容
