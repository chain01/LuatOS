/*
@module  w5500
@summary w5500以太网驱动
@version 1.0
@date    2022.04.11
*/

#include "luat_base.h"
#ifdef LUAT_USE_W5500
#include "luat_rtos.h"
#include "luat_zbuff.h"
#include "luat_spi.h"
#define LUAT_LOG_TAG "w5500"
#include "luat_log.h"

#include "w5500_def.h"
#include "luat_network_adapter.h"
/*
初始化w5500
@api w5500.init(spiid, speed, cs_pin, irq_pin, rst_pin, link_pin)
@int spi通道号
@int spi速度
@int cs pin
@int irq pin
@int reset pin
@usage
w5500.init(spi.SPI_0, 24000000, pin.PB13, pin.PC08, pin.PC09)
*/
static int l_w5500_init(lua_State *L){
	luat_spi_t spi = {0};
	spi.id = luaL_checkinteger(L, 1);
	spi.bandrate = luaL_checkinteger(L, 2);
	spi.cs = luaL_checkinteger(L, 3);
	spi.CPHA = 0;
	spi.CPOL = 0;
	spi.master = 1;
	spi.mode = 1;
	spi.dataw = 8;
	spi.bit_dict = 1;
	uint32_t irq_pin = luaL_checkinteger(L, 4);
	uint32_t reset_pin = luaL_checkinteger(L, 5);
	uint32_t link_pin = luaL_optinteger(L, 6, 0xff);
	w5500_init(&spi, irq_pin, reset_pin, link_pin);
	return 0;
}

#ifndef LUAT_USE_LWIP
/*
w5500配置网络信息
@api w5500.config(ip, submask, gateway, mac, RTR, RCR, speed)
@string 静态ip地址，如果需要用DHCP获取，请写nil
@string 子网掩码，如果使用动态ip，则忽略
@string 网关，如果使用动态ip，则忽略
@string MAC，写nil则通过MCU唯一码自动生成，如果要写，长度必须是6byte
@int 重试间隔时间，默认2000，单位100us，不懂的不要改
@int 最大重试次数，默认8，不懂的不要改
@int 速度类型，目前只有0硬件配置，1自适应，默认为0
@usage
w5500.config("192.168.1.2", "255.255.255.0", "192.168.1.1", string.fromHex("102a3b4c5d6e"))
*/
static int l_w5500_config(lua_State *L){
	if (!w5500_device_ready())
	{
		lua_pushboolean(L, 0);
		LLOGD("device no ready");
		return 1;
	}

	if (lua_isstring(L, 1))
	{
	    size_t ip_len = 0;
	    const char* ip = luaL_checklstring(L, 1, &ip_len);
	    size_t mask_len = 0;
	    const char* mask = luaL_checklstring(L, 2, &mask_len);
	    size_t gateway_len = 0;
	    const char* gateway = luaL_checklstring(L, 3, &gateway_len);
	    w5500_set_static_ip(network_string_to_ipv4(ip, ip_len), network_string_to_ipv4(mask, mask_len), network_string_to_ipv4(gateway, gateway_len));
	}
	else
	{
		w5500_set_static_ip(0, 0, 0);
	}

	if (lua_isstring(L, 4))
	{
		size_t mac_len = 0;
		const char *mac = luaL_checklstring(L, 4, &mac_len);
		w5500_set_mac(mac);
	}

	w5500_set_param(luaL_optinteger(L, 5, 2000), luaL_optinteger(L, 6, 8), luaL_optinteger(L, 7, 0), 0);


	w5500_reset();
	lua_pushboolean(L, 1);
	return 1;
}
#endif
/*
将w5500注册进通用网络接口
@api w5500.bind(network.xxx)
@int 通用网络通道号
@usage
w5500.bind(network.ETH0)
*/
static int l_w5500_network_register(lua_State *L){

	int index = luaL_checkinteger(L, 1);
	w5500_register_adapter(index);
	return 0;
}


#include "rotable2.h"
static const rotable_Reg_t reg_w5500[] =
{
    { "init",           ROREG_FUNC(l_w5500_init)},
#ifndef LUAT_USE_LWIP
	{ "config",           ROREG_FUNC(l_w5500_config)},
#endif
	{ "bind",           ROREG_FUNC(l_w5500_network_register)},
	{ NULL,            ROREG_INT(0)}
};

LUAMOD_API int luaopen_w5500( lua_State *L ) {
    luat_newlib2(L, reg_w5500);
    return 1;
}

#endif
