/****************************************************************************
*
*    Copyright (C) 2005 - 2014 by Vivante Corp.
*
*    This program is free software; you can redistribute it and/or modify
*    it under the terms of the GNU General Public License as published by
*    the Free Software Foundation; either version 2 of the license, or
*    (at your option) any later version.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*    GNU General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with this program; if not write to the Free Software
*    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*
*****************************************************************************/


#include "gc_hal_kernel_linux.h"
#include "gc_hal_kernel_platform.h"
#include "gc_hal_kernel_device.h"
#include "gc_hal_driver.h"

#if USE_PLATFORM_DRIVER
#   include <linux/platform_device.h>
#endif
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#define CLK_ENABLE_BITS_3D_CORE         1
#define CLK_PLL_SEL_BITS_3D_CORE        3
#define CLK_PLL_SWITCH_BITS_3D_CORE     1
#define CLK_SWITCH_BITS_3D_CORE         1
#define CLK_D3_SWITCH_BITS_3D_CORE      1
#define CLK_DIV_SEL_BITS_3D_CORE        3

#define CLK_ENABLE_OFFSET_3D_CORE       0
#define CLK_PLL_SEL_OFFSET_3D_CORE      (CLK_ENABLE_OFFSET_3D_CORE + CLK_ENABLE_BITS_3D_CORE)
#define CLK_PLL_SWITCH_OFFSET_3D_CORE   (CLK_PLL_SEL_OFFSET_3D_CORE + CLK_PLL_SEL_BITS_3D_CORE)
#define CLK_SWITCH_OFFSET_3D_CORE       (CLK_PLL_SWITCH_OFFSET_3D_CORE + CLK_PLL_SWITCH_BITS_3D_CORE)
#define CLK_D3_SWITCH_OFFSET_3D_CORE    (CLK_SWITCH_OFFSET_3D_CORE +  CLK_SWITCH_BITS_3D_CORE)
#define CLK_DIV_SEL_OFFSET_3D_CORE      (CLK_D3_SWITCH_OFFSET_3D_CORE + CLK_D3_SWITCH_BITS_3D_CORE)

#define CLK_ENABLE_BITS_3D_SYS         1
#define CLK_PLL_SEL_BITS_3D_SYS        3
#define CLK_PLL_SWITCH_BITS_3D_SYS     1
#define CLK_SWITCH_BITS_3D_SYS         1
#define CLK_D3_SWITCH_BITS_3D_SYS      1
#define CLK_DIV_SEL_BITS_3D_SYS        3

#define CLK_ENABLE_OFFSET_3D_SYS       0
#define CLK_PLL_SEL_OFFSET_3D_SYS      (CLK_ENABLE_OFFSET_3D_SYS + CLK_ENABLE_BITS_3D_SYS)
#define CLK_PLL_SWITCH_OFFSET_3D_SYS   (CLK_PLL_SEL_OFFSET_3D_SYS + CLK_PLL_SEL_BITS_3D_SYS)
#define CLK_SWITCH_OFFSET_3D_SYS       (CLK_PLL_SWITCH_OFFSET_3D_SYS + CLK_PLL_SWITCH_BITS_3D_SYS)
#define CLK_D3_SWITCH_OFFSET_3D_SYS    (CLK_SWITCH_OFFSET_3D_SYS +  CLK_SWITCH_BITS_3D_SYS)
#define CLK_DIV_SEL_OFFSET_3D_SYS      (CLK_D3_SWITCH_OFFSET_3D_SYS + CLK_D3_SWITCH_BITS_3D_SYS)

#define CLK_ENABLE_BITS_3D_SH         1
#define CLK_PLL_SEL_BITS_3D_SH        3
#define CLK_PLL_SWITCH_BITS_3D_SH     1
#define CLK_SWITCH_BITS_3D_SH         1
#define CLK_D3_SWITCH_BITS_3D_SH      1
#define CLK_DIV_SEL_BITS_3D_SH        3

#define CLK_ENABLE_OFFSET_3D_SH       0
#define CLK_PLL_SEL_OFFSET_3D_SH      (CLK_ENABLE_OFFSET_3D_SH + CLK_ENABLE_BITS_3D_SH)
#define CLK_PLL_SWITCH_OFFSET_3D_SH   (CLK_PLL_SEL_OFFSET_3D_SH + CLK_PLL_SEL_BITS_3D_SH)
#define CLK_SWITCH_OFFSET_3D_SH       (CLK_PLL_SWITCH_OFFSET_3D_SH + CLK_PLL_SWITCH_BITS_3D_SH)
#define CLK_D3_SWITCH_OFFSET_3D_SH    (CLK_SWITCH_OFFSET_3D_SH +  CLK_SWITCH_BITS_3D_SH)
#define CLK_DIV_SEL_OFFSET_3D_SH      (CLK_D3_SWITCH_OFFSET_3D_SH + CLK_D3_SWITCH_BITS_3D_SH)

struct bg2q_priv {
    ulong coreClkRegister2D;
    ulong coreClkRegister3D;
    ulong sysClkRegister3D;
    ulong shClkRegister3D;
    ulong coreClkBitfield2D;
    ulong coreClkBitfield3D;
    ulong sysClkBitfield3D;
    ulong shClkBitfield3D;
};

struct bg2q_priv bg2qPriv;

static inline void gckOS_SetRegister(unsigned int addr, unsigned int bit)
{
    unsigned long value = 0;

    if (addr != 0)
    {
        value = readl_relaxed((const volatile void *)addr);
        value |= 1 << bit;
        writel_relaxed(value, (volatile void *)addr);
    }
}

static inline void gckOS_ClrRegister(unsigned int addr, unsigned int bit)
{
    unsigned long value = 0;

    if (addr != 0)
    {
        value = readl_relaxed((const volatile void *)addr);
        value &= ~(1 << bit);
        writel_relaxed(value, (volatile void *)addr);
    }
}

gceSTATUS
_AdjustParam(
    IN gckPLATFORM Platform,
    OUT gcsMODULE_PARAMETERS *Args
    )
{
    struct bg2q_priv* priv = Platform->priv;

    struct device_node *np;
    struct resource res;
    int rc = 0;
    u32 value;

    /* get the gpu3d information */
    np = of_find_compatible_node(NULL, NULL, "marvell,berlin-gpu3d");
    if (!np)
    {
        printk("gpu error: of_find_compatible_node for berlin-gpu3d failed!\n");
        goto err_exit;
    }

    /* get the reg section */
    rc = of_address_to_resource(np, 0, &res);
    if (rc)
    {
        printk("gpu warning: of_address_to_resource for berlin-gpu3d failed!\n");
    }
    else
    {
        Args->registerMemBase = res.start;
        Args->registerMemSize = resource_size(&res);
    }

    /* get the interrupt section */
    rc = of_irq_to_resource(np, 0, &res);
    if (!rc)
    {
        printk("gpu warning: of_irq_to_resource for berlin-gpu3d failed, rc=%d!\n", rc);
    }
    else
    {
        Args->irqLine = rc;
    }

    /* get the nonsecure-mem-base section */
    rc = of_property_read_u32(np, "marvell,nonsecure-mem-base", &value);
    if (rc)
    {
        printk("gpu warning: of_property_read_u32 for nonsecure-mem-base failed!\n");
    }
    else
    {
        Args->contiguousBase = value;
    }

    /* get the nonsecure-mem-size section */
    rc = of_property_read_u32(np, "marvell,nonsecure-mem-size", &value);
    if (rc)
    {
        printk("gpu warning: of_property_read_u32 for nonsecure-mem-size failed!\n");
    }
    else
    {
        Args->contiguousSize = value;
    }

    /* get the phy-mem-size section */
    rc = of_property_read_u32(np, "marvell,phy-mem-size", &value);
    if (rc)
    {
        printk("gpu warning: of_property_read_u32 for phy-mem-size failed!\n");
    }
    else
    {
        Args->physSize = value;
    }

    /* get the 3D clock registers and corresponding bitfields */
    rc = of_property_read_u32(np, "marvell,core-clock-register", &value);
    if (rc)
    {
        printk("gpu warning: of_property_read_u32 for 3D core-clock-register failed!\n");
        priv->coreClkRegister3D = 0;
    }
    else
    {
        priv->coreClkRegister3D = value;
    }

    rc = of_property_read_u32(np, "marvell,sys-clock-register", &value);
    if (rc)
    {
        printk("gpu warning: of_property_read_u32 for 3D sys-clock-register failed!\n");
        priv->sysClkRegister3D = 0;
    }
    else
    {
        priv->sysClkRegister3D = value;
    }

    rc = of_property_read_u32(np, "marvell,sh-clock-register", &value);
    if (rc)
    {
        printk("gpu warning: of_property_read_u32 for 3D sh-clock-register failed!\n");
        priv->shClkRegister3D = 0;
    }
    else
    {
        priv->shClkRegister3D = value;
    }

    rc = of_property_read_u32(np, "marvell,core-clock-bitfield", &value);
    if (rc)
    {
        printk("gpu warning: of_property_read_u32 for 3D core-clock-bitfield failed!\n");
        priv->coreClkBitfield3D = 0;
    }
    else
    {
        priv->coreClkBitfield3D = value;
    }

    rc = of_property_read_u32(np, "marvell,sys-clock-bitfield", &value);
    if (rc)
    {
        printk("gpu warning: of_property_read_u32 for 3D sys-clock-bitfield failed!\n");
        priv->sysClkBitfield3D = 0;
    }
    else
    {
        priv->sysClkBitfield3D = value;
    }

    rc = of_property_read_u32(np, "marvell,sh-clock-bitfield", &value);
    if (rc)
    {
        printk("gpu warning: of_property_read_u32 for 3D sh-clock-bitfield failed!\n");
        priv->shClkBitfield3D = 0;
    }
    else
    {
        priv->shClkBitfield3D = value;
    }

    of_node_put(np);

    /* get the gpu2d information */
    np = of_find_compatible_node(NULL, NULL, "marvell,berlin-gpu2d");
    if (!np)
    {
        /* no 2d gpu, just goto exit */
        goto err_exit;
    }

    /* get the reg section */
    rc = of_address_to_resource(np, 0, &res);
    if (rc)
    {
        printk("gpu warning: of_address_to_resource for berlin-gpu2d failed!\n");
    }
    else
    {
        Args->registerMemBase2D = res.start;
        Args->registerMemSize2D = resource_size(&res);
    }

    /* get the interrupt section */
    rc = of_irq_to_resource(np, 0, &res);
    if (!rc)
    {
        printk("gpu warning: of_irq_to_resource for berlin-gpu2d failed!\n");
    }
    else
    {
        Args->irqLine2D = rc;
    }

    /* get 2D core clock register and corresponding bitfield */
    rc = of_property_read_u32(np, "marvell,core-clock-register", &value);
    if (rc)
    {
        printk("gpu warning: of_property_read_u32 for 2D core-clock-register failed!\n");
        priv->coreClkRegister2D = 0;
    }
    else
    {
        priv->coreClkRegister2D = value;
    }

    rc = of_property_read_u32(np, "marvell,core-clock-bitfield", &value);
    if (rc)
    {
        printk("gpu warning: of_property_read_u32 for 2D core-clock-bitfield failed!\n");
        priv->coreClkBitfield2D = 0;
    }
    else
    {
        priv->coreClkBitfield2D = value;
    }

    of_node_put(np);



    return gcvSTATUS_OK;
err_exit:
    return gcvSTATUS_OUT_OF_RESOURCES;
}

gceSTATUS
_AllocPriv(
    IN gckPLATFORM Platform
    )
{
    Platform->priv = &bg2qPriv;
    return gcvSTATUS_OK;
}

gceSTATUS
_FreePriv(
    IN gckPLATFORM Platform
    )
{
    return gcvSTATUS_OK;
}

gctBOOL
_NeedAddDevice(
    IN gckPLATFORM Platform
    )
{
    return gcvTRUE;
}

gceSTATUS
_SetClock(
    IN gckPLATFORM Platform,
    IN gceCORE GPU,
    IN gctBOOL Enable
    )
{
    struct bg2q_priv* priv = Platform->priv;

    if (GPU == gcvCORE_MAJOR)
    {
        if (Enable == gcvTRUE)
        {
           gckOS_SetRegister(priv->coreClkRegister3D, priv->coreClkBitfield3D);
           gckOS_SetRegister(priv->sysClkRegister3D, priv->sysClkBitfield3D);
        }
        else
        {
            gckOS_ClrRegister(priv->coreClkRegister3D, priv->coreClkBitfield3D);
            gckOS_ClrRegister(priv->sysClkRegister3D, priv->sysClkBitfield3D);
        }
    }
    else if (GPU == gcvCORE_2D)
    {
        if (Enable == gcvTRUE)
        {
            gckOS_SetRegister(priv->coreClkRegister2D, priv->coreClkBitfield2D);
        }
        else
        {
            gckOS_ClrRegister(priv->coreClkRegister2D, priv->coreClkBitfield2D);
        }
    }

    return gcvSTATUS_OK;
}

inline unsigned long get_ulong_mask(unsigned int bits, unsigned int offset)
{
    return (((unsigned long)0x1 << bits) - 1) << offset;
}

inline void set_ulong_bits
(
    unsigned long *to,
    unsigned int bits,
    unsigned int offset,
    unsigned long value
)
{
    *to &= ~(get_ulong_mask(bits, offset));
    *to |= value << offset;
}

inline unsigned long get_ulong_bits
(
    unsigned int bits,
    unsigned int offset,
    unsigned long from
)
{
    return from >> offset & (((unsigned long)0x1 << bits) - 1);
}

inline gceSTATUS set_ext_clock_source_3D_core
(
    unsigned long               *value,
    gceEXT_3D_CoreClockSource   source
)
{
    unsigned long clkPllSel = 0;
    unsigned long clkPllSwitch = 0;

    switch (source)
    {
    case gcvEXT_3D_CORE_CLOCK_AVPLLB4:
        clkPllSel = 0;
        clkPllSwitch = 1;
        break;
    case gcvEXT_3D_CORE_CLOCK_AVPLLB5:
        clkPllSel = 1;
        clkPllSwitch = 1;
        break;
    case gcvEXT_3D_CORE_CLOCK_AVPLLB6:
        clkPllSel = 2;
        clkPllSwitch = 1;
        break;
    case gcvEXT_3D_CORE_CLOCK_AVPLLB7:
        clkPllSel = 3;
        clkPllSwitch = 1;
        break;
    case gcvEXT_3D_CORE_CLOCK_SYSPLL:
        clkPllSel = 4;
        clkPllSwitch = 0;
        break;
    default:
        printk(KERN_ERR "gfx driver: invalid 3D core clock source 0x%x\n", (unsigned int)source);
        return gcvSTATUS_INVALID_ARGUMENT;
    }

    set_ulong_bits(value, CLK_PLL_SEL_BITS_3D_CORE, CLK_PLL_SEL_OFFSET_3D_CORE, clkPllSel);
    set_ulong_bits(value, CLK_PLL_SWITCH_BITS_3D_CORE, CLK_PLL_SWITCH_OFFSET_3D_CORE, clkPllSwitch);

    return gcvSTATUS_OK;
}

inline gceSTATUS set_ext_clock_div_3D_core(unsigned long *value, gceEXT_3D_CoreClockDiv div)
{
    unsigned long clkDivSel = 0;

    if (div == gcvEXT_3D_CORE_CLOCK_DIV3)
    {
        set_ulong_bits(value, CLK_D3_SWITCH_BITS_3D_CORE, CLK_D3_SWITCH_OFFSET_3D_CORE, 0x1);
        return gcvSTATUS_OK;
    }

    switch (div)
    {
    case gcvEXT_3D_CORE_CLOCK_DIV4:
        clkDivSel = 2;
        break;
    case gcvEXT_3D_CORE_CLOCK_DIV6:
        clkDivSel = 3;
        break;
    case gcvEXT_3D_CORE_CLOCK_DIV8:
        clkDivSel = 4;
        break;
    case gcvEXT_3D_CORE_CLOCK_DIV12:
        clkDivSel = 5;
        break;
    default:
        printk(KERN_ERR "gfx driver: invalid division value 0x%x set to 3D core clock register\n",
               (unsigned int)clkDivSel);
        return gcvSTATUS_INVALID_ARGUMENT;
    }

    set_ulong_bits(value, CLK_D3_SWITCH_BITS_3D_CORE, CLK_D3_SWITCH_OFFSET_3D_CORE, 0x0);
    set_ulong_bits(value, CLK_SWITCH_BITS_3D_CORE, CLK_SWITCH_OFFSET_3D_CORE, 0x1);
    set_ulong_bits(value, CLK_DIV_SEL_BITS_3D_CORE, CLK_DIV_SEL_OFFSET_3D_CORE, clkDivSel);

    return gcvSTATUS_OK;
}

inline gceSTATUS set_ext_clock_source_3D_sys
(
    unsigned long *value,
    gceEXT_3D_SysClockSource source
)
{
    unsigned long clkPllSel = 0;
    unsigned long clkPllSwitch = 0;

    switch (source)
    {
    case gcvEXT_3D_SYS_CLOCK_AVPLLB4:
        clkPllSel = 0;
        clkPllSwitch = 1;
        break;
    case gcvEXT_3D_SYS_CLOCK_AVPLLB5:
        clkPllSel = 1;
        clkPllSwitch = 1;
        break;
    case gcvEXT_3D_SYS_CLOCK_AVPLLB6:
        clkPllSel = 2;
        clkPllSwitch = 1;
        break;
    case gcvEXT_3D_SYS_CLOCK_AVPLLB7:
        clkPllSel = 3;
        clkPllSwitch = 1;
        break;
    case gcvEXT_3D_SYS_CLOCK_SYSPLL:
        clkPllSel = 4;
        clkPllSwitch = 0;
        break;
    default:
        printk(KERN_ERR "gfx driver: invalid 3D sys clock source 0x%x\n", (unsigned int)source);
        return gcvSTATUS_INVALID_ARGUMENT;
    }

    set_ulong_bits(value, CLK_PLL_SEL_BITS_3D_SYS, CLK_PLL_SEL_OFFSET_3D_SYS, clkPllSel);
    set_ulong_bits(value, CLK_PLL_SWITCH_BITS_3D_SYS, CLK_PLL_SWITCH_OFFSET_3D_SYS, clkPllSwitch);

    return gcvSTATUS_OK;
}

inline gceSTATUS set_ext_clock_div_3D_sys(unsigned long *value, gceEXT_3D_SysClockDiv div)
{
    unsigned long clkDivSel = 0;

    if (div == gcvEXT_3D_SYS_CLOCK_DIV3)
    {
        set_ulong_bits(value, CLK_D3_SWITCH_BITS_3D_SYS, CLK_D3_SWITCH_OFFSET_3D_SYS, 0x1);
        return gcvSTATUS_OK;
    }

    switch (div)
    {
    case gcvEXT_3D_SYS_CLOCK_DIV2:
        clkDivSel = 1;
        break;
    case gcvEXT_3D_SYS_CLOCK_DIV4:
        clkDivSel = 2;
        break;
    case gcvEXT_3D_SYS_CLOCK_DIV6:
        clkDivSel = 3;
        break;
    case gcvEXT_3D_SYS_CLOCK_DIV8:
        clkDivSel = 4;
        break;
    case gcvEXT_3D_SYS_CLOCK_DIV12:
        clkDivSel = 5;
        break;
    default:
        printk(KERN_ERR "gfx driver: invalid division value 0x%x set to 3D sys clock register\n",
               (unsigned int)clkDivSel);
        return gcvSTATUS_INVALID_ARGUMENT;
    }

    set_ulong_bits(value, CLK_D3_SWITCH_BITS_3D_SYS, CLK_D3_SWITCH_OFFSET_3D_SYS, 0x0);
    set_ulong_bits(value, CLK_SWITCH_BITS_3D_SYS, CLK_SWITCH_OFFSET_3D_SYS, 0x1);
    set_ulong_bits(value, CLK_DIV_SEL_BITS_3D_SYS, CLK_DIV_SEL_OFFSET_3D_SYS, clkDivSel);

    return gcvSTATUS_OK;
}


inline gceSTATUS set_ext_clock_source_3D_sh
(
    unsigned long *value,
    gceEXT_3D_ShClockSource source
)
{
    unsigned long clkPllSel = 0;
    unsigned long clkPllSwitch = 0;

    switch (source)
    {
    case gcvEXT_3D_SH_CLOCK_AVPLLB4:
        clkPllSel = 0;
        clkPllSwitch = 1;
        break;
    case gcvEXT_3D_SH_CLOCK_AVPLLB5:
        clkPllSel = 1;
        clkPllSwitch = 1;
        break;
    case gcvEXT_3D_SH_CLOCK_AVPLLB6:
        clkPllSel = 2;
        clkPllSwitch = 1;
        break;
    case gcvEXT_3D_SH_CLOCK_AVPLLB7:
        clkPllSel = 3;
        clkPllSwitch = 1;
        break;
    case gcvEXT_3D_SH_CLOCK_SYSPLL:
        clkPllSel = 4;
        clkPllSwitch = 0;
        break;
    default:
        printk(KERN_ERR "gfx driver: invalid 3D sh clock source 0x%x\n", (unsigned int)source);
        return gcvSTATUS_INVALID_ARGUMENT;
    }

    set_ulong_bits(value, CLK_PLL_SEL_BITS_3D_SH, CLK_PLL_SEL_OFFSET_3D_SH, clkPllSel);
    set_ulong_bits(value, CLK_PLL_SWITCH_BITS_3D_SH, CLK_PLL_SWITCH_OFFSET_3D_SH, clkPllSwitch);

    return gcvSTATUS_OK;
}

inline gceSTATUS set_ext_clock_div_3D_sh(unsigned long *value, gceEXT_3D_ShClockDiv div)
{
    unsigned long clkDivSel = 0;

    if (div == gcvEXT_3D_SH_CLOCK_DIV3)
    {
        set_ulong_bits(value, CLK_D3_SWITCH_BITS_3D_SH, CLK_D3_SWITCH_OFFSET_3D_SH, 0x1);
        return gcvSTATUS_OK;
    }

    switch (div)
    {
    case gcvEXT_3D_SH_CLOCK_DIV4:
        clkDivSel = 2;
        break;
    case gcvEXT_3D_SH_CLOCK_DIV6:
        clkDivSel = 3;
        break;
    case gcvEXT_3D_SH_CLOCK_DIV8:
        clkDivSel = 4;
        break;
    case gcvEXT_3D_SH_CLOCK_DIV12:
        clkDivSel = 5;
        break;
    default:
        printk(KERN_ERR "gfx driver: invalid division value 0x%x set to 3D sh clock register\n",
               (unsigned int)clkDivSel);
        return gcvSTATUS_INVALID_ARGUMENT;
    }

    set_ulong_bits(value, CLK_D3_SWITCH_BITS_3D_SH, CLK_D3_SWITCH_OFFSET_3D_SH, 0x0);
    set_ulong_bits(value, CLK_SWITCH_BITS_3D_SH, CLK_SWITCH_OFFSET_3D_SH, 0x1);
    set_ulong_bits(value, CLK_DIV_SEL_BITS_3D_SH, CLK_DIV_SEL_OFFSET_3D_SH, clkDivSel);

    return gcvSTATUS_OK;
}

inline gceSTATUS get_ext_clock_source_3D_core
(
    unsigned long value,
    gceEXT_3D_CoreClockSource *source
)
{
    unsigned int clkPllSel = 0;
    unsigned int clkPllSwitch = 0;

    clkPllSel = get_ulong_bits(CLK_PLL_SEL_BITS_3D_CORE, CLK_PLL_SEL_OFFSET_3D_CORE, value);
    clkPllSwitch = get_ulong_bits(CLK_PLL_SWITCH_BITS_3D_CORE, CLK_PLL_SWITCH_OFFSET_3D_CORE, value);
    if ((clkPllSel == 0x4 && clkPllSwitch != 0x0) ||
        (clkPllSel != 0x4 && clkPllSwitch == 0x0))
    {
        printk(KERN_ERR "gfx driver: conflict source PLL configuration\n");
        return gcvSTATUS_INVALID_CONFIG;
    }

    switch (clkPllSel)
    {
    case 0:
        *source = gcvEXT_3D_CORE_CLOCK_AVPLLB4;
        break;
    case 1:
        *source = gcvEXT_3D_CORE_CLOCK_AVPLLB5;
        break;
    case 2:
        *source = gcvEXT_3D_CORE_CLOCK_AVPLLB6;
        break;
    case 3:
        *source = gcvEXT_3D_CORE_CLOCK_AVPLLB7;
        break;
    case 4:
        *source = gcvEXT_3D_CORE_CLOCK_SYSPLL;
        break;
    default:
        printk(KERN_ERR "gfx driver: invalid PLL clock source 0x%x\n", clkPllSel);
        return gcvSTATUS_INVALID_CONFIG;
    }

    return gcvSTATUS_OK;
}

inline gceSTATUS get_ext_clock_div_3D_core(unsigned long value, gceEXT_3D_CoreClockDiv *div)
{
    unsigned int clkDivSel = 0;
    unsigned int clkD3Switch = 0;
    unsigned int clkSwitch = 0;

    clkD3Switch = get_ulong_bits(CLK_D3_SWITCH_BITS_3D_CORE, CLK_D3_SWITCH_OFFSET_3D_CORE, value);
    if (clkD3Switch == 0x1)
    {
        *div = gcvEXT_3D_CORE_CLOCK_DIV3;
        return gcvSTATUS_OK;
    }

    clkSwitch = get_ulong_bits(CLK_SWITCH_BITS_3D_CORE, CLK_SWITCH_OFFSET_3D_CORE, value);
    if (clkSwitch == 0x0)
    {
        printk(KERN_ERR "gfx driver: no divider is enabled\n");
        return gcvSTATUS_INVALID_CONFIG;
    }

    clkDivSel = get_ulong_bits(CLK_DIV_SEL_BITS_3D_CORE, CLK_DIV_SEL_OFFSET_3D_CORE, value);
    switch (clkDivSel)
    {
    case 2:
        *div = gcvEXT_3D_CORE_CLOCK_DIV4;
        break;
    case 3:
        *div = gcvEXT_3D_CORE_CLOCK_DIV6;
        break;
    case 4:
        *div = gcvEXT_3D_CORE_CLOCK_DIV8;
        break;
    case 5:
        *div = gcvEXT_3D_CORE_CLOCK_DIV12;
        break;
    default:
        printk(KERN_ERR "gfx driver: invalid division factor 0x%x in 3D core clock register\n",
               (unsigned int)clkDivSel);
        return gcvSTATUS_INVALID_CONFIG;
    }

    return gcvSTATUS_OK;
}

inline gceSTATUS get_ext_clock_source_3D_sys
(
    unsigned long value,
    gceEXT_3D_SysClockSource *source
)
{
    unsigned int clkPllSel = 0;
    unsigned int clkPllSwitch = 0;

    clkPllSel = get_ulong_bits(CLK_PLL_SEL_BITS_3D_SYS, CLK_PLL_SEL_OFFSET_3D_SYS, value);
    clkPllSwitch = get_ulong_bits(CLK_PLL_SWITCH_BITS_3D_SYS, CLK_PLL_SWITCH_OFFSET_3D_SYS, value);
    if ((clkPllSel == 0x4 && clkPllSwitch != 0x0) ||
        (clkPllSel != 0x4 && clkPllSwitch == 0x0))
    {
        printk(KERN_ERR "gfx driver: conflict source PLL configuration\n");
        return gcvSTATUS_INVALID_CONFIG;
    }

    switch (clkPllSel)
    {
    case 0:
        *source = gcvEXT_3D_SYS_CLOCK_AVPLLB4;
        break;
    case 1:
        *source = gcvEXT_3D_SYS_CLOCK_AVPLLB5;
        break;
    case 2:
        *source = gcvEXT_3D_SYS_CLOCK_AVPLLB6;
        break;
    case 3:
        *source = gcvEXT_3D_SYS_CLOCK_AVPLLB7;
        break;
    case 4:
        *source = gcvEXT_3D_SYS_CLOCK_SYSPLL;
        break;
    default:
        printk(KERN_ERR "gfx driver: invalid PLL clock source 0x%x\n", clkPllSel);
        return gcvSTATUS_INVALID_CONFIG;
    }

    return gcvSTATUS_OK;
}

inline gceSTATUS get_ext_clock_div_3D_sys(unsigned long value, gceEXT_3D_SysClockDiv *div)
{
    unsigned int clkDivSel = 0;
    unsigned int clkD3Switch = 0;
    unsigned int clkSwitch = 0;

    clkD3Switch = get_ulong_bits(CLK_D3_SWITCH_BITS_3D_SYS, CLK_D3_SWITCH_OFFSET_3D_SYS, value);
    if (clkD3Switch == 0x1)
    {
        *div = gcvEXT_3D_SYS_CLOCK_DIV3;
        return gcvSTATUS_OK;
    }

    clkSwitch = get_ulong_bits(CLK_SWITCH_BITS_3D_SYS, CLK_SWITCH_OFFSET_3D_SYS, value);
    if (clkSwitch == 0x0)
    {
        printk(KERN_ERR "gfx driver: no divider is enabled\n");
        return gcvSTATUS_INVALID_CONFIG;
    }

    clkDivSel = get_ulong_bits(CLK_DIV_SEL_BITS_3D_SYS, CLK_DIV_SEL_OFFSET_3D_SYS, value);
    switch (clkDivSel)
    {
    case 1:
        *div = gcvEXT_3D_SYS_CLOCK_DIV2;
        break;
    case 2:
        *div = gcvEXT_3D_SYS_CLOCK_DIV4;
        break;
    case 3:
        *div = gcvEXT_3D_SYS_CLOCK_DIV6;
        break;
    case 4:
        *div = gcvEXT_3D_SYS_CLOCK_DIV8;
        break;
    case 5:
        *div = gcvEXT_3D_SYS_CLOCK_DIV12;
        break;
    default:
        printk(KERN_ERR "gfx driver: invalid division factor 0x%x in 3D sys clock register\n",
               (unsigned int)clkDivSel);
        return gcvSTATUS_INVALID_CONFIG;
    }

    return gcvSTATUS_OK;
}

inline gceSTATUS get_ext_clock_source_3D_sh
(
    unsigned long value,
    gceEXT_3D_ShClockSource *source
)
{
    unsigned int clkPllSel = 0;
    unsigned int clkPllSwitch = 0;

    clkPllSel = get_ulong_bits(CLK_PLL_SEL_BITS_3D_SH, CLK_PLL_SEL_OFFSET_3D_SH, value);
    clkPllSwitch = get_ulong_bits(CLK_PLL_SWITCH_BITS_3D_SH, CLK_PLL_SWITCH_OFFSET_3D_SH, value);
    if ((clkPllSel == 0x4 && clkPllSwitch != 0x0) ||
        (clkPllSel != 0x4 && clkPllSwitch == 0x0))
    {
        printk(KERN_ERR "gfx driver: conflict source PLL configuration\n");
        return gcvSTATUS_INVALID_CONFIG;
    }

    switch (clkPllSel)
    {
    case 0:
        *source = gcvEXT_3D_SH_CLOCK_AVPLLB4;
        break;
    case 1:
        *source = gcvEXT_3D_SH_CLOCK_AVPLLB5;
        break;
    case 2:
        *source = gcvEXT_3D_SH_CLOCK_AVPLLB6;
        break;
    case 3:
        *source = gcvEXT_3D_SH_CLOCK_AVPLLB7;
        break;
    case 4:
        *source = gcvEXT_3D_SH_CLOCK_SYSPLL;
        break;
    default:
        printk(KERN_ERR "gfx driver: invalid PLL clock source 0x%x\n", clkPllSel);
        return gcvSTATUS_INVALID_CONFIG;
    }

    return gcvSTATUS_OK;
}

inline gceSTATUS get_ext_clock_div_3D_sh(unsigned long value, gceEXT_3D_ShClockDiv *div)
{
    unsigned int clkDivSel = 0;
    unsigned int clkD3Switch = 0;
    unsigned int clkSwitch = 0;

    clkD3Switch = get_ulong_bits(CLK_D3_SWITCH_BITS_3D_SH, CLK_D3_SWITCH_OFFSET_3D_SH, value);
    if (clkD3Switch == 0x1)
    {
        *div = gcvEXT_3D_SH_CLOCK_DIV3;
        return gcvSTATUS_OK;
    }

    clkSwitch = get_ulong_bits(CLK_SWITCH_BITS_3D_SH, CLK_SWITCH_OFFSET_3D_SH, value);
    if (clkSwitch == 0x0)
    {
        printk(KERN_ERR "gfx driver: no divider is enabled\n");
        return gcvSTATUS_INVALID_CONFIG;
    }

    clkDivSel = get_ulong_bits(CLK_DIV_SEL_BITS_3D_SH, CLK_DIV_SEL_OFFSET_3D_SH, value);
    switch (clkDivSel)
    {
    case 2:
        *div = gcvEXT_3D_SH_CLOCK_DIV4;
        break;
    case 3:
        *div = gcvEXT_3D_SH_CLOCK_DIV6;
        break;
    case 4:
        *div = gcvEXT_3D_SH_CLOCK_DIV8;
        break;
    case 5:
        *div = gcvEXT_3D_SH_CLOCK_DIV12;
        break;
    default:
        printk(KERN_ERR "gfx driver: invalid division factor 0x%x in 3D sh clock register\n",
               (unsigned int)clkDivSel);
        return gcvSTATUS_INVALID_CONFIG;
    }

    return gcvSTATUS_OK;
}

static gceSTATUS _SetClockExt(
    IN gckPLATFORM Platform,
    IN gcsHAL_ExtClock *extClk
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    unsigned long value = 0;
    struct bg2q_priv* priv = Platform->priv;

    if (extClk->eFlags & gcvEXT_PM_3D_CORE_CLOCK_SOURCE ||
        extClk->eFlags & gcvEXT_PM_3D_CORE_CLOCK_DIV)
    {
        if (priv->coreClkRegister3D == 0)
        {
            return gcvSTATUS_OK;
        }
        value = readl_relaxed((const volatile void *)priv->coreClkRegister3D);
        if (extClk->eFlags & gcvEXT_PM_3D_CORE_CLOCK_SOURCE)
        {
            gcmkONERROR(set_ext_clock_source_3D_core(&value, extClk->e3DCoreSource));
        }

        if (extClk->eFlags & gcvEXT_PM_3D_CORE_CLOCK_DIV)
        {
            gcmkONERROR(set_ext_clock_div_3D_core(&value, extClk->e3DCoreDiv));
        }
        writel_relaxed(value, (volatile void *)priv->coreClkRegister3D);
    }

    if (extClk->eFlags & gcvEXT_PM_3D_SYS_CLOCK_SOURCE ||
        extClk->eFlags & gcvEXT_PM_3D_SYS_CLOCK_DIV)
    {
        if (priv->sysClkRegister3D == 0)
        {
            return gcvSTATUS_OK;
        }
        value = readl_relaxed((const volatile void *)priv->sysClkRegister3D);
        if (extClk->eFlags & gcvEXT_PM_3D_SYS_CLOCK_SOURCE)
        {
            gcmkONERROR(set_ext_clock_source_3D_sys(&value, extClk->e3DSysSource));
        }

        if (extClk->eFlags & gcvEXT_PM_3D_SYS_CLOCK_DIV)
        {
            gcmkONERROR(set_ext_clock_div_3D_sys(&value, extClk->e3DSysDiv));
        }
        writel_relaxed(value, (volatile void *)priv->sysClkRegister3D);
    }

    if (extClk->eFlags & gcvEXT_PM_3D_SH_CLOCK_SOURCE ||
        extClk->eFlags & gcvEXT_PM_3D_SH_CLOCK_DIV)
    {
        if (priv->shClkRegister3D == 0)
        {
            return gcvSTATUS_OK;
        }
        value = readl_relaxed((const volatile void *)priv->shClkRegister3D);
        if (extClk->eFlags & gcvEXT_PM_3D_SH_CLOCK_SOURCE)
        {
            gcmkONERROR(set_ext_clock_source_3D_sh(&value, extClk->e3DShSource));
        }

        if (extClk->eFlags & gcvEXT_PM_3D_SH_CLOCK_DIV)
        {
            gcmkONERROR(set_ext_clock_div_3D_sh(&value, extClk->e3DShDiv));
        }
        writel_relaxed(value, (volatile void *)priv->shClkRegister3D);
    }

    return gcvSTATUS_OK;

OnError:
    return status;
}

static gceSTATUS _GetClockExt(
    IN      gckPLATFORM Platform,
    INOUT   gcsHAL_ExtClock *extClk
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    unsigned long value = 0;
    struct bg2q_priv* priv = Platform->priv;

    if (extClk->eFlags & gcvEXT_PM_3D_CORE_CLOCK_SOURCE ||
        extClk->eFlags & gcvEXT_PM_3D_CORE_CLOCK_DIV)
    {
        if (priv->coreClkRegister3D == 0)
        {
            return gcvSTATUS_OK;
        }
        value = readl_relaxed((const volatile void *)priv->coreClkRegister3D);
        if (extClk->eFlags & gcvEXT_PM_3D_CORE_CLOCK_SOURCE)
        {
            gcmkONERROR(get_ext_clock_source_3D_core(value, &extClk->e3DCoreSource));
        }

        if (extClk->eFlags & gcvEXT_PM_3D_CORE_CLOCK_DIV)
        {
            gcmkONERROR(get_ext_clock_div_3D_core(value, &extClk->e3DCoreDiv));
        }
    }

    if (extClk->eFlags & gcvEXT_PM_3D_SYS_CLOCK_SOURCE ||
        extClk->eFlags & gcvEXT_PM_3D_SYS_CLOCK_DIV)
    {
        if (priv->sysClkRegister3D == 0)
        {
            return gcvSTATUS_OK;
        }
        value = readl_relaxed((const volatile void *)priv->sysClkRegister3D);
        if (extClk->eFlags & gcvEXT_PM_3D_SYS_CLOCK_SOURCE)
        {
            gcmkONERROR(get_ext_clock_source_3D_sys(value, &extClk->e3DSysSource));
        }

        if (extClk->eFlags & gcvEXT_PM_3D_SYS_CLOCK_DIV)
        {
            gcmkONERROR(get_ext_clock_div_3D_sys(value, &extClk->e3DSysDiv));
        }
    }

    if (extClk->eFlags & gcvEXT_PM_3D_SH_CLOCK_SOURCE ||
        extClk->eFlags & gcvEXT_PM_3D_SH_CLOCK_DIV)
    {
        if (priv->shClkRegister3D == 0)
        {
            return gcvSTATUS_OK;
        }
        value = readl_relaxed((const volatile void *)priv->shClkRegister3D);
        if (extClk->eFlags & gcvEXT_PM_3D_SH_CLOCK_SOURCE)
        {
            gcmkONERROR(get_ext_clock_source_3D_sh(value, &extClk->e3DShSource));
        }

        if (extClk->eFlags & gcvEXT_PM_3D_SH_CLOCK_DIV)
        {
            gcmkONERROR(get_ext_clock_div_3D_sh(value, &extClk->e3DShDiv));
        }
    }

    return gcvSTATUS_OK;

OnError:
    return status;
}

gcsPLATFORM_OPERATIONS platformOperations = {
    .adjustParam   = _AdjustParam,
    .needAddDevice = _NeedAddDevice,
    .setClock      = _SetClock,
    .allocPriv     = _AllocPriv,
    .setClockExt   = _SetClockExt,
    .getClockExt   = _GetClockExt,
};

void
gckPLATFORM_QueryOperations(
    IN gcsPLATFORM_OPERATIONS ** Operations
    )
{
     *Operations = &platformOperations;
}

