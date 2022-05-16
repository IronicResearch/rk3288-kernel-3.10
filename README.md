# rk3288-kernel-3.10
Development snapshot for RK3288 kernel 3.10

Uses gcc ARM cross-compiler package for Ubuntu 16.

```
$ sudo apt-get install gcc-arm-linux-gnueabihf
```

Refer to root-level build script for compiling kernel config + binary images.

```
$ make dpf097_mbv10_defconfig
$ make dpf097_mbv10.img -j4
```

Resulting kernel.img + resource.img binaries compatible with Rockchip flash tools and AOSP packing tools.
