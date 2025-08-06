#include <linux/module.h>
#include <linux/export-internal.h>
#include <linux/compiler.h>

MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};



static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0xbb41b493, "i2c_smbus_write_byte_data" },
	{ 0x60d8647c, "i2c_unregister_device" },
	{ 0xb477e7f4, "device_destroy" },
	{ 0x4a41ecb3, "class_destroy" },
	{ 0x607587f4, "cdev_del" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0x122c3a7e, "_printk" },
	{ 0x12a4e128, "__arch_copy_from_user" },
	{ 0xdcb764ad, "memset" },
	{ 0xf0fdf6cb, "__stack_chk_fail" },
	{ 0xe3ec2f2b, "alloc_chrdev_region" },
	{ 0x5d9d9fd4, "cdev_init" },
	{ 0xcc335c1c, "cdev_add" },
	{ 0xf311fc60, "class_create" },
	{ 0xad100f12, "device_create" },
	{ 0x4537dddf, "i2c_get_adapter" },
	{ 0xdab9418e, "i2c_new_client_device" },
	{ 0xcffd5996, "i2c_put_adapter" },
	{ 0xf9a482f9, "msleep" },
	{ 0x39ff040a, "module_layout" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "A44607CD2B297971D9790FB");
