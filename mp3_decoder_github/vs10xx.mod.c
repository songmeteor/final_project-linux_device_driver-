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
	{ 0xe3ec2f2b, "alloc_chrdev_region" },
	{ 0x2a70f0, "gpiod_set_value" },
	{ 0x76d59173, "of_property_read_variable_u32_array" },
	{ 0x92540fbf, "finish_wait" },
	{ 0x4a41ecb3, "class_destroy" },
	{ 0x4829a47e, "memcpy" },
	{ 0x37a0cba, "kfree" },
	{ 0x8c26d495, "prepare_to_wait_event" },
	{ 0xca1caeb6, "gpiod_get_value" },
	{ 0xe2964344, "__wake_up" },
	{ 0x34db050b, "_raw_spin_lock_irqsave" },
	{ 0x122c3a7e, "_printk" },
	{ 0x1000e51, "schedule" },
	{ 0xf0fdf6cb, "__stack_chk_fail" },
	{ 0xd41cd16a, "_dev_info" },
	{ 0xcc335c1c, "cdev_add" },
	{ 0x3364cb49, "spi_sync" },
	{ 0xfe487975, "init_wait_entry" },
	{ 0x6a409ad6, "_dev_err" },
	{ 0xad100f12, "device_create" },
	{ 0xf311fc60, "class_create" },
	{ 0xd51bf3d7, "driver_unregister" },
	{ 0xd35cce70, "_raw_spin_unlock_irqrestore" },
	{ 0xdcb764ad, "memset" },
	{ 0xd9a5ea54, "__init_waitqueue_head" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0xeae3dfd6, "__const_udelay" },
	{ 0xcec1d106, "__spi_register_driver" },
	{ 0x832481e, "devm_gpiod_get" },
	{ 0xb477e7f4, "device_destroy" },
	{ 0xf7a7b9dd, "__kmalloc_cache_noprof" },
	{ 0x12a4e128, "__arch_copy_from_user" },
	{ 0xf9a482f9, "msleep" },
	{ 0x5d9d9fd4, "cdev_init" },
	{ 0x92fc2eae, "kmalloc_caches" },
	{ 0x607587f4, "cdev_del" },
	{ 0x39ff040a, "module_layout" },
};

MODULE_INFO(depends, "");

MODULE_ALIAS("of:N*T*Cvs1003-ctrl");
MODULE_ALIAS("of:N*T*Cvs1003-ctrlC*");
MODULE_ALIAS("of:N*T*Cvs1003-data");
MODULE_ALIAS("of:N*T*Cvs1003-dataC*");

MODULE_INFO(srcversion, "8980836203F508371B8F6A6");
