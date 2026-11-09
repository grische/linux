/*
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/regulator/of_regulator.h>
#include <linux/slab.h>
#if defined(CONFIG_LTQ_CPUFREQ)
#include <cpufreq/ltq_cpufreq.h>
#endif

//#define DBG 1

#ifdef DBG
#define mp8864_print(fmt, arg...)                                              \
	do {                                                                   \
		printk(KERN_WARNING fmt, ##arg);                               \
	} while (0)
#else
/*--- #define mp8864_print(fmt, arg...) do { printk(KERN_WARNING  fmt, ##arg); } while (0) ---*/
#define mp8864_print(fmt, arg...)
#endif

#define mp8864_err(fmt, arg...)                                                \
	do {                                                                   \
		printk(KERN_ERR fmt, ##arg);                                   \
	} while (0)

/* Registers */
enum { MP8864_VSEL, MP8864_SYSCNTLREG1, MP8864_ID1, MP8864_STATUS };

enum MP_STATUS { MP_ENABLE, MP_DISABLE };

/* Register bit definition */
/* VOUT - Voltage control */
#define MP8864_REG_VSEL_VBOOT_BIT BIT(7)
#define MP8864_REG_VSEL_VBOOT_OFFSET 7
#define MP8864_REG_VSEL_OUT_REF_MASK 0x7F
#define MP8864_REG_VSEL_OUT_REF_SHIFT 0

/* Register bit definition */
/* Sys Cntl*/
#define MP8864_REG_SYSCNTLREG1_EN_BIT BIT(7)
#define MP8864_REG_SYSCNTLREG1_EN_OFFSET 7
#define MP8864_REG_SYSCNTLREG1_GO_BIT BIT(6)
#define MP8864_REG_SYSCNTLREG1_GO_OFFSET 6
#define MP8864_REG_SYSCNTLREG1_SLEWRATE_MASK 0x7
#define MP8864_REG_SYSCNTLREG1_SLEWRATE_OFFSET 3
#define MP8864_REG_SYSCNTLREG1_SWITCHINGFREQ_MASK 0x3
#define MP8864_REG_SYSCNTLREG1_SWITCHINGFREQ_OFFSET 1
#define MP8864_REG_SYSCNTLREG1_MODE_BIT BIT(0)
#define MP8864_REG_SYSCNTLREG1_MODE_OFFSET 0

/* Register bit definition */
/* ID */
#define MP8864_REG_ID_VENDOR_MASK 0xF
#define MP8864_REG_ID_VENDOR_OFFSET 4
#define MP8864_REG_ID_REVISION_MASK 0xF
#define MP8864_REG_ID_REVISION_OFFSET 4

/* Register bit definition */
/* Status */
#define MP8864_REG_STATUS_VIDOK_BIT BIT(4)
#define MP8864_REG_STATUS_VIDOK_OFFSET 4
#define MP8864_REG_STATUS_OC_BIT BIT(3)
#define MP8864_REG_STATUS_OC_OFFSET 3
#define MP8864_REG_STATUS_OTEW_MASK BIT(2)
#define MP8864_REG_STATUS_OTEW_OFFSET 2
#define MP8864_REG_STATUS_OT_BIT BIT(1)
#define MP8864_REG_STATUS_OT_OFFSET 1
#define MP8864_REG_STATUS_PG_BIT BIT(0)
#define MP8864_REG_STATUS_PG_OFFSET 0

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct mp8864_platform_data {
	u32 pfm_mode;
	u32 slew_rate;
	u32 switching_freq;
	u32 vboot;
	u32 vout;
	struct regulator_init_data *reg_data;
};

struct mp8864_data {
	struct i2c_client *client;
	struct device *dev;
	struct mp8864_platform_data *pdata;
	struct regulator_desc desc;
	struct regulator_dev *rdev;
};

/* Regulator specific details */
struct mp_info {
	const char *name;
	u8 table_len;
	const unsigned int *table;
};

/* Struct passed as driver data */
struct mp_driver_data {
	const struct mp_info *info;
};

/*------------------------------------------------------------------------------------------*\
    static defs
 \*------------------------------------------------------------------------------------------*/
static int mp8864_is_enabled(struct regulator_dev *rdev);
static int mp8864_enable(struct regulator_dev *rdev);
static int mp8864_disable(struct regulator_dev *rdev);
static int mp8864_status(struct regulator_dev *rdev);
#ifdef MP8864_SEL_FUNCTIONS
static int mp8864_get_voltage_sel(struct regulator_dev *rdev);
static int mp8864_set_voltage_sel(struct regulator_dev *rdev,
				  unsigned selector);
#else /*--- #ifdef MP8864_SEL_FUNCTIONS ---*/
static int mp8864_get_voltage(struct regulator_dev *rdev);
static int mp8864_set_voltage(struct regulator_dev *rdev, int min_uV,
			      int max_uV, unsigned int *selector);
#endif /*--- #else #ifdef MP8864_SEL_FUNCTIONS ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static struct regulator_ops mp8864_core_voltage_ops = {
	.is_enabled = mp8864_is_enabled,
	.enable = mp8864_enable,
	.disable = mp8864_disable,
	.get_status = mp8864_status,
	.list_voltage = regulator_list_voltage_table,
	.map_voltage = regulator_map_voltage_ascend,
#ifdef MP8864_SEL_FUNCTIONS
	.get_voltage_sel = mp8864_get_voltage_sel,
	.set_voltage_sel = mp8864_set_voltage_sel,
#else /*--- #ifdef MP8864_SEL_FUNCTIONS ---*/
	.get_voltage = mp8864_get_voltage,
	.set_voltage = mp8864_set_voltage,
#endif /*--- #else #ifdef MP8864_SEL_FUNCTIONS ---*/
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static const unsigned int VCORE_VSEL_table[] = {
	600000, /*---  0 ---*/
	610000, /*---  1 ---*/
	620000, /*---  2 ---*/
	630000, /*---  3 ---*/
	640000, /*---  4 ---*/
	650000, /*---  5 ---*/
	660000, /*---  6 ---*/
	670000, /*---  7 ---*/
	680000, /*---  8 ---*/
	690000, /*---  9 ---*/
	700000, /*---  A ---*/
	710000, /*---  B ---*/
	720000, /*---  C ---*/
	730000, /*---  D ---*/
	740000, /*---  E ---*/
	750000, /*---  F ---*/
	760000, /*--- 10 ---*/
	770000, /*--- 11 ---*/
	780000, /*--- 12 ---*/
	790000, /*--- 13 ---*/
	800000, /*--- 14 ---*/
	810000, /*--- 15 ---*/
	820000, /*--- 16 ---*/
	830000, /*--- 17 ---*/
	840000, /*--- 18 ---*/
	850000, /*--- 19 ---*/
	860000, /*--- 1A ---*/
	870000, /*--- 1B ---*/
	880000, /*--- 1C ---*/
	890000, /*--- 1D ---*/
	900000, /*--- 1E ---*/
	910000, /*--- 1F ---*/
	920000, /*--- 20 ---*/
	930000, /*--- 21 ---*/
	940000, /*--- 22 ---*/
	950000, /*--- 23 ---*/
	960000, /*--- 24 ---*/
	970000, /*--- 25 ---*/
	980000, /*--- 26 ---*/
	990000, /*--- 27 ---*/
	1000000, /*--- 28 ---*/
	1010000, /*--- 29 ---*/
	1020000, /*--- 2A ---*/
	1030000, /*--- 2B ---*/
	1040000, /*--- 2C ---*/
	1050000, /*--- 2D ---*/
	1060000, /*--- 2E ---*/
	1070000, /*--- 2F ---*/
	1080000, /*--- 30 ---*/
	1090000, /*--- 31 ---*/
	1100000, /*--- 32 ---*/
	1110000, /*--- 33 ---*/
	1120000, /*--- 34 ---*/
	1130000, /*--- 35 ---*/
	1140000, /*--- 36 ---*/
	1150000, /*--- 37 ---*/
	1160000, /*--- 38 ---*/
	1170000, /*--- 39 ---*/
	1180000, /*--- 3A ---*/
	1190000 /*--- 3B ---*/
};

#define MP8864_MIN_VOLTAGE 600000
#define MP8864_MAX_VOLTAGE 1190000
#define MP8864_STEP_VOLTAGE 10000
#define MAX_ALLOWED_VOLTAGE_INDEX 0x3B

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static const struct mp_info mp8864_regs[] = { {
	.name = "mp8864",
	.table_len = ARRAY_SIZE(VCORE_VSEL_table),
	.table = VCORE_VSEL_table,
} };

static struct mp_driver_data mp8864_drv_data = {
	.info = mp8864_regs,
};

static const struct i2c_device_id mp8864_ids[] = {
	{ .name = "mp8864", .driver_data = (unsigned long)&mp8864_drv_data },
	{},
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int mp8864_read_reg(struct mp8864_data *mp8864, u8 reg)
{
	int ret;
	int loop;

	/*--- pr_debug("[%s] reading reg 0x%x\n", __FUNCTION__, reg); ---*/
	for (loop = 0; loop < 3; loop++) {
		ret = i2c_smbus_read_byte_data(mp8864->client, reg);
		if (ret < 0) {
			pr_warn("[%s] reading reg  0x%x return <0: %d -> retry\n",
				__FUNCTION__, reg, ret);
			continue;
		}
		/*--- pr_debug("[%s] reading reg 0x%x return 0x%x\n", __FUNCTION__, reg, ret); ---*/
		return ret & 0xff;
	}

	pr_err("[%s] I2C read failed after %d retries (ret %d)\n", __FUNCTION__,
	       loop, ret);

	BUG();
	return ret;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int mp8864_write_reg(struct mp8864_data *mp8864, u8 reg, u8 value)
{
	int ret;
	int loop;

	/*--- pr_debug("[%s] writing reg 0x%x val 0x%08x\n", __FUNCTION__, reg, value); ---*/

	for (loop = 0; loop < 3; loop++) {
		ret = i2c_smbus_write_byte_data(mp8864->client, reg, value);
		if (ret < 0) {
			pr_warn("[%s] writing reg  0x%x return <0: 0x%x -> retry\n",
				__FUNCTION__, reg, ret);
			continue;
		}
		return ret & 0xff;
	}

	pr_err("[%s] I2C write failed after %d retries (ret %d)\n",
	       __FUNCTION__, loop, ret);

	BUG();
	return ret;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int mp8864_reg_is_enabled(struct mp8864_data *mp)
{
	int ret = mp8864_read_reg(mp, MP8864_SYSCNTLREG1);

	ret = (ret & MP8864_REG_SYSCNTLREG1_EN_BIT) >>
	      MP8864_REG_SYSCNTLREG1_EN_OFFSET;

	return ret;
}

/*------------------------------------------------------------------------------------------*\
    ACHTUNG: Nie enable auf NULL setzten, das schaltet die Corespannung komplett ab
 \*------------------------------------------------------------------------------------------*/
static int mp8864_reg_enable(struct mp8864_data *mp,
			     int status __attribute__((unused)))
{
	u8 ret = 0;
	u8 reg = MP8864_SYSCNTLREG1;

	ret = mp8864_read_reg(mp, reg);
	ret |= (1 << MP8864_REG_SYSCNTLREG1_EN_OFFSET);

	mp8864_print("%s: write 0x%x to reg %d\n", __func__, ret, reg);
	mp8864_write_reg(mp, reg, ret);

	return status;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int mp8864_set_vboot(struct mp8864_data *mp, int vboot)
{
	u8 ret = 0;
	u8 reg = MP8864_VSEL;

	ret = mp8864_read_reg(mp, reg);

	if (vboot) {
		ret |= MP8864_REG_VSEL_VBOOT_BIT;
	} else {
		ret &= ~(MP8864_REG_VSEL_VBOOT_BIT);
	}

	/* write to register */
	mp8864_print("%s: write 0x%x to reg %d\n", __func__, ret, reg);
	mp8864_write_reg(mp, reg, ret);

	return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int mp8864_set_pfm_mode(struct mp8864_data *mp, int mode)
{
	u8 ret = 0;
	u8 reg = MP8864_SYSCNTLREG1;

	ret = mp8864_read_reg(mp, reg);
	if (mode) {
		ret |= MP8864_REG_SYSCNTLREG1_MODE_BIT;
	} else {
		ret &= ~(MP8864_REG_SYSCNTLREG1_MODE_BIT);
	}

	/* write to register */
	mp8864_print("%s: write 0x%x to reg %d\n", __func__, ret, reg);
	mp8864_write_reg(mp, reg, ret);
	return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int mp8864_set_slew(struct mp8864_data *mp, int slew)
{
	u8 ret = 0;
	u8 reg = MP8864_SYSCNTLREG1;

	ret = mp8864_read_reg(mp, reg);
	ret &= ~(MP8864_REG_SYSCNTLREG1_SLEWRATE_MASK
		 << MP8864_REG_SYSCNTLREG1_SLEWRATE_OFFSET);
	slew = slew & MP8864_REG_SYSCNTLREG1_SLEWRATE_MASK;
	ret |= (slew << MP8864_REG_SYSCNTLREG1_SLEWRATE_OFFSET);

	/* write to register */
	mp8864_print("%s: write 0x%x to reg %d\n", __func__, ret, reg);
	mp8864_write_reg(mp, reg, ret);

	return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int mp8864_set_switching_freq(struct mp8864_data *mp, int freq)
{
	u8 ret = 0;
	u8 reg = MP8864_SYSCNTLREG1;

	ret = mp8864_read_reg(mp, reg);
	ret &= ~(MP8864_REG_SYSCNTLREG1_SWITCHINGFREQ_MASK
		 << MP8864_REG_SYSCNTLREG1_SWITCHINGFREQ_OFFSET);

	freq = freq & MP8864_REG_SYSCNTLREG1_SWITCHINGFREQ_MASK;
	ret |= (freq << MP8864_REG_SYSCNTLREG1_SWITCHINGFREQ_OFFSET);

	/* write to register */
	mp8864_print("%s: write 0x%x to reg %d\n", __func__, ret, reg);
	mp8864_write_reg(mp, reg, ret);

	return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int mp8864_set_vout(struct mp8864_data *mp, int selector)
{
	u8 ret = 0;
	u8 reg = MP8864_VSEL;

	/*---- for now, don't allow voltages above 1.19 V ----*/
	if (selector > MAX_ALLOWED_VOLTAGE_INDEX) {
		pr_debug("[%s] Error, voltages above 1.19 V are not allowed\n",
			 "MP8864");
		return -1;
	}

	/*--- GO Bit setzen ---*/
	reg = MP8864_SYSCNTLREG1;
	ret = mp8864_read_reg(mp, reg);

	ret |= (1 << MP8864_REG_SYSCNTLREG1_GO_OFFSET);
	ret |= (1
		<< MP8864_REG_SYSCNTLREG1_EN_OFFSET); /* enable muss immer gesetzt sein */

	mp8864_print("%s: write 0x%x to reg %d\n", __func__, ret, reg);
	/*--- pr_debug("%s: write 0x%x to reg %d\n", __func__, ret, reg); ---*/
	mp8864_write_reg(mp, reg, ret);

	/*--- new Voltage setzen ---*/
	reg = MP8864_VSEL;

	/*--- das FB (feedback) Bit NICHT setzten ---*/

	mp8864_print("%s: write 0x%x to reg %d %dµV\n", __func__, selector, reg,
		     VCORE_VSEL_table[selector]);
	/*--- pr_debug("%s: write 0x%x to reg %d %dµV\n", __func__, selector, reg, VCORE_VSEL_table[selector]); ---*/

	if (selector < 0x28) {
		selector = 0x28;
		mp8864_print("%s: WARNING: selector is set to 0x28 = %dµV\n",
			     __func__, VCORE_VSEL_table[selector]);
	}
	//    selector |= (1 << MP8864_REG_VSEL_VBOOT_OFFSET);
	mp8864_write_reg(mp, reg, selector);

	{
		int status = 0;
		int vsel = 0;
		int syscntl = 0;
		status = mp8864_read_reg(mp, MP8864_STATUS);
		vsel = mp8864_read_reg(mp, MP8864_VSEL);
		syscntl = mp8864_read_reg(mp, MP8864_SYSCNTLREG1);
		/*--- pr_debug("[MP8864 Dump] Status is 0x%02x Vsel is 0x%02x Syscntl is 0x%02x\n", status, vsel, syscntl); ---*/
	}

	return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int mp8864_get_voltage_sel(struct regulator_dev *rdev)
{
	struct mp8864_data *mp8864 = rdev_get_drvdata(rdev);
	int selector;

	u8 reg = MP8864_VSEL;
	selector = mp8864_read_reg(mp8864, reg);
	selector &= MP8864_REG_VSEL_OUT_REF_MASK;
	mp8864_print("%s: read 0x%x = %d %dµV\n", __func__, reg, selector,
		     VCORE_VSEL_table[selector]);

	return selector;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int mp8864_set_voltage_sel(struct regulator_dev *rdev, unsigned selector)
{
	struct mp8864_data *mp8864 = rdev_get_drvdata(rdev);

	mp8864_print("[%s] %d\n", __func__, selector);
	mp8864_set_vout(mp8864, selector);

	/* send notify */
	regulator_notifier_call_chain(rdev, REGULATOR_EVENT_VOLTAGE_CHANGE,
				      mp8864);

	return selector;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int mp8864_get_voltage(struct regulator_dev *rdev)
{
	int selector = mp8864_get_voltage_sel(rdev);

	mp8864_print("[%s] \n", __func__);

	if (selector >=
	    sizeof(VCORE_VSEL_table) / sizeof(VCORE_VSEL_table[0])) {
		mp8864_print("[%s] invalid selector %d\n", __func__, selector);
		return 0;
	}

	mp8864_print("[%s] %ld µV\n", __func__, VCORE_VSEL_table[selector]);

	return VCORE_VSEL_table[selector];
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int mp8864_set_voltage(struct regulator_dev *rdev, int min_uV,
			      int max_uV, unsigned int *selector)
{
	int i;

	/*--- pr_debug("[%s] voltage %d - %d\n", __FUNCTION__, min_uV, max_uV); ---*/

	if (max_uV < VCORE_VSEL_table[0]) {
		pr_debug("[%s] max voltage (%d) below table entry (%d)\n",
			 __FUNCTION__, max_uV, VCORE_VSEL_table[0]);
		return -1;
	}

	for (i = 1; i < sizeof(VCORE_VSEL_table) / sizeof(VCORE_VSEL_table[0]);
	     i++) {
		if (min_uV >= VCORE_VSEL_table[i - 1]) {
			if (max_uV < VCORE_VSEL_table[i]) {
				if (selector) {
					*selector = i - 1;
				}
				mp8864_print("[%s] %ld µV\n", __func__, i - 1);

				return mp8864_set_voltage_sel(rdev, i - 1);
			}
		}
	}

	pr_debug("[%s] failed to find table entry.\n", __FUNCTION__);

	return -1;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int mp8864_enable(struct regulator_dev *rdev)
{
	int ret = 0;
	struct mp8864_data *mp = rdev_get_drvdata(rdev);

	mp8864_print("[%s] \n", __func__);

	ret = mp8864_reg_enable(mp, MP_ENABLE);

	return ret;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int mp8864_disable(struct regulator_dev *rdev)
{
	int ret = 0;
	// struct mp8864_data *mp = rdev_get_drvdata(rdev);

	mp8864_print("[%s] ERROR never disable the core voltage !!!!!!\n",
		     __func__);

	//    mp8864_reg_enable(mp, MP_DISABLE);

	return ret;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int mp8864_is_enabled(struct regulator_dev *rdev)
{
	int ret = 0;
	struct mp8864_data *mp = rdev_get_drvdata(rdev);

	mp8864_print("[%s]\n", __func__);

	if (!mp) {
		mp8864_err("%s: Can not get user resource\n", __func__);
		return -1;
	}
	ret = mp8864_reg_is_enabled(mp);

	return ret;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int mp8864_status(struct regulator_dev *rdev)
{
	int status = 0;
	int vsel = 0;
	int syscntl = 0;
	struct mp8864_data *mp = rdev_get_drvdata(rdev);

	mp8864_print("[%s]\n", __func__);
	status = mp8864_read_reg(mp, MP8864_STATUS);
	vsel = mp8864_read_reg(mp, MP8864_VSEL);
	syscntl = mp8864_read_reg(mp, MP8864_SYSCNTLREG1);

	/*--- over current ---*/
	if (status & MP8864_REG_STATUS_OC_BIT)
		return REGULATOR_STATUS_ERROR;
	/*--- over temperature ---*/
	if (status & MP8864_REG_STATUS_OT_BIT)
		return REGULATOR_STATUS_ERROR;
	/* I2C Mode or FB mode ? */
	if (vsel & MP8864_REG_VSEL_VBOOT_BIT)
		return REGULATOR_STATUS_BYPASS;
	/*--- EN Bit? ---*/
	if (syscntl & MP8864_REG_SYSCNTLREG1_EN_BIT) {
		if (status & MP8864_REG_STATUS_PG_BIT)
			return REGULATOR_STATUS_ON;
		else
			return REGULATOR_STATUS_ERROR;
	} else {
		return REGULATOR_STATUS_OFF;
	}
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#ifdef CONFIG_OF
static struct regulator_desc mp8864_regulator_desc[] = {
	{
		.name = "mp8864",
		.id = 0,
		.ops = &mp8864_core_voltage_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{},
};

static struct of_regulator_match mp8864_matches[] = {
	{
		.name = "core",
	},
};

static struct of_device_id mp8864_dt_match[] = {
	{ .compatible = "mps,mp8864" },
	{},
};
MODULE_DEVICE_TABLE(of, mp8864_dt_match);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static struct mp8864_platform_data *mp8864_parse_dt(struct device *dev)
{
	struct mp8864_platform_data *pd;
	struct device_node *np = dev->of_node;
	u32 dt_enable, dt_slew_rate, dt_switching_freq, dt_vout, dt_mode,
		dt_vboot;
	struct regulator_init_data *reg_data;
	struct device_node *regulators;
	int ret;

	pr_debug("[%s] \n", __FUNCTION__);

	pd = devm_kzalloc(dev, sizeof(*pd), GFP_KERNEL);
	if (!pd) {
		dev_err(dev, "Failed to allocate platform data\n");
		return NULL;
	}

	regulators = of_find_node_by_name(np, "regulators");
	if (!regulators) {
		dev_err(dev, "regulator node not found\n");
		return ERR_PTR(-ENODEV);
	}

	ret = of_regulator_match(dev, regulators, mp8864_matches,
				 ARRAY_SIZE(mp8864_matches));
	if (ret < 0) {
		dev_err(dev, "Error parsing regulator init data: %d\n", ret);
		return ERR_PTR(ret);
	} else {
		int idx;

		for (idx = 0; idx < ARRAY_SIZE(mp8864_matches); idx++)
			pr_debug(
				"%s: regulator '%s': of_node='%s'\n", __func__,
				mp8864_matches[idx].name ?: "(no name)",
				mp8864_matches[idx].of_node ?
					mp8864_matches[idx].of_node->full_name :
					"(no name)");
	}

	pd->reg_data = of_get_regulator_init_data(dev, np, &mp8864_regulator_desc[0]);
	if (!pd->reg_data) {
		dev_err(dev, "Failed to parse regulator init data\n");
		goto fail_of;
		return NULL;
	}
	reg_data = pd->reg_data;
	reg_data->constraints.min_uV = MP8864_MIN_VOLTAGE;
	reg_data->constraints.max_uV = MP8864_MAX_VOLTAGE;

	if (of_property_read_u32(np, "vout_enable", &dt_enable)) {
		dev_warn(dev, "vout_enable not specified\n");
		dt_enable = 1;
	}
	pr_debug("[%s] vout_enable %d\n", __FUNCTION__, dt_enable);

	if (of_property_read_u32(np, "vout_vboot", &dt_vboot)) {
		dev_warn(dev, "vout_vboot not specified\n");
		dt_vboot = 1;
	}
	pr_debug("[%s] vout_vboot %d\n", __FUNCTION__, dt_vboot);

	if (of_property_read_u32(np, "pfm_mode", &dt_mode)) {
		dev_warn(dev, "pfm_mode not specified\n");
		dt_mode = 1;
	}
	pr_debug("[%s] pfm_mode%d\n", __FUNCTION__, dt_mode);

	if (of_property_read_u32(np, "voltage_index", &dt_vout)) {
		dev_warn(dev, "voltage_index not specified\n");
		dt_vout = 0;
	} else {
		if (dt_vout >= MAX_ALLOWED_VOLTAGE_INDEX) {
			//switch to FB Mode if specified voltage is set to high
			dt_vboot = 1;
			dt_vout = 0;
		}
	}
	pr_debug("[%s] voltage_index %d\n", __FUNCTION__, dt_vout);

	if (of_property_read_u32(np, "vout_slew_rate", &dt_slew_rate)) {
		dev_warn(dev, "vout_slew_rate not specified\n");
		dt_slew_rate = 4;
	}
	pr_debug("[%s] slew_rate %d\n", __FUNCTION__, dt_slew_rate);

	if (of_property_read_u32(np, "vout_switching_freq",
				 &dt_switching_freq)) {
		dev_warn(dev, "vout_switching_freq not specified\n");
		dt_switching_freq = 0;
	}
	pr_debug("[%s] dt_switching_freq %d\n", __FUNCTION__,
		 dt_switching_freq);

	reg_data->constraints.valid_modes_mask =
		REGULATOR_MODE_NORMAL | REGULATOR_MODE_STANDBY;
	reg_data->constraints.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE;
	if (dt_enable) {
		reg_data->constraints.valid_modes_mask |=
			REGULATOR_CHANGE_STATUS;
		reg_data->constraints.always_on = 1;
	} else {
		reg_data->constraints.always_on = 0;
		reg_data->constraints.valid_ops_mask |= REGULATOR_CHANGE_STATUS;
	}

	pd->slew_rate = dt_slew_rate;
	pd->switching_freq = dt_switching_freq;
	pd->pfm_mode = dt_mode;
	pd->vboot = dt_vboot;
	pd->vout = dt_vout;

	return pd;

fail_of:
	kfree(pd->reg_data);
	kfree(pd);
	return NULL;
}
#else /*--- #ifdef CONFIG_OF ---*/
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static struct mp8864_platform_data *mp8864_parse_dt(struct device *dev)
{
	return NULL;
}
#endif /*--- #else  #ifdef CONFIG_OF ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int mp_8864_i2c_probe(struct i2c_client *client,
			     const struct i2c_device_id *i2c_id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct mp8864_platform_data *pdata = client->dev.platform_data;
	const struct mp_driver_data *drv_data = (void *)i2c_id->driver_data;
	const struct mp_info *info = drv_data->info;
	struct regulator_config config[2];
	struct mp8864_data *mp8864;
	int ret = 0;

	dev_err(&client->dev, "MP8864 probe started.\n");

	if (client->dev.of_node)
		pdata = mp8864_parse_dt(&client->dev);

	if (!pdata) {
		dev_err(&client->dev, "Require the platform data\n");
		return -EINVAL;
	}

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE)) {
		dev_err(&client->dev, "Funktionality test failed\n");
		return -EIO;
	}

	mp8864 = devm_kzalloc(&client->dev, sizeof(struct mp8864_data) * 2,
			      GFP_KERNEL);
	if (!mp8864)
		return -ENOMEM;

	/* save general information */
	mp8864[0].client = client;
	mp8864[0].dev = &client->dev;
	mp8864[0].pdata = pdata;

	/* register regulator to system */
	mp8864_regulator_desc[0].n_voltages = info->table_len;
	mp8864_regulator_desc[0].volt_table = info->table;
	mp8864[0].desc = mp8864_regulator_desc[0];

	config[0] = (struct regulator_config){
		.dev = mp8864[0].dev,
		.init_data = pdata->reg_data,
		.driver_data = &mp8864[0],
		.of_node = mp8864_matches[0].of_node,
	};

	mp8864[0].rdev = regulator_register(&mp8864[0].desc, &config[0]);

	if (IS_ERR(mp8864[0].rdev)) {
		ret = PTR_ERR(mp8864->rdev);
		dev_err(mp8864->dev, "regulator init failed (%d)\n", ret);
		goto fail_no_unregister;
	}

	/* enable I2C control interface, if configured */
	if (pdata->vboot == 0) {
		/* other settings from device tree */
		mp8864_set_pfm_mode(mp8864, pdata->pfm_mode);
		mp8864_set_slew(mp8864, pdata->slew_rate);
		mp8864_set_switching_freq(mp8864, pdata->switching_freq);
		mp8864_set_vout(mp8864, pdata->vout);
		mp8864_set_vboot(mp8864, pdata->vboot);
	} else {
		dev_warn(mp8864->dev,
			 "Regulator is running in FB control loop\n");
	}

	{
		int status = 0;
		int vsel = 0;
		int syscntl = 0;
		status = mp8864_read_reg(mp8864, MP8864_STATUS);
		vsel = mp8864_read_reg(mp8864, MP8864_VSEL);
		syscntl = mp8864_read_reg(mp8864, MP8864_SYSCNTLREG1);
		dev_warn(
			mp8864->dev,
			"[MP8864 Init] Status is 0x%02x Vsel is 0x%02x Syscntl is 0x%02x\n",
			status, vsel, syscntl);
	}

	i2c_set_clientdata(client, mp8864);

#if defined(CONFIG_LTQ_CPUFREQ)
	{
		struct ltq_cpufreq *ltq_cpufreq_obj = ltq_cpufreq_get();

		if (ltq_cpufreq_obj) {
			ltq_cpufreq_obj->regulator =
				regulator_get(config[0].dev, mp8864->desc.name);

			if (IS_ERR_OR_NULL(ltq_cpufreq_obj->regulator)) {
				pr_err("[%s]regulator registration failed\n",
				       __func__);
				BUG();
			}
		}
	}
#endif

	return 0;

fail_no_unregister:

	return ret;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int mp_8864_i2c_exit(struct i2c_client *client)
{
	struct mp8864_data *mp8864 = i2c_get_clientdata(client);
	struct regulator_dev *rdev = mp8864->rdev;

	regulator_unregister(rdev);

	/* free client resource */
	if (mp8864) {
		kfree(mp8864->pdata);
		kfree(mp8864);
	}

	return 0;
}

MODULE_DEVICE_TABLE(i2c, mp8864_ids);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static struct i2c_driver mp_8864_i2c_driver = {
	.probe = mp_8864_i2c_probe,
	.remove = mp_8864_i2c_exit,
	.driver =
		{
			.name = "mp8864",
#ifdef CONFIG_OF
			.of_match_table = of_match_ptr(mp8864_dt_match),
#endif
		},
	.id_table = mp8864_ids,
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int __init mp_8864_init(void)
{
	int ret = i2c_add_driver(&mp_8864_i2c_driver);

	pr_debug("[%s] i2c_add_driver(&mp_8864_i2c_driver) = %d\n",
		 __FUNCTION__, ret);

	return ret;
}
subsys_initcall(mp_8864_init);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void __exit mp_8864_exit(void)
{
	i2c_del_driver(&mp_8864_i2c_driver);
	pr_debug("[%s] i2c_del_driver(&mp_8864_i2c_driver)\n", __FUNCTION__);
}

// module_init(mp_8864_init);
module_exit(mp_8864_exit);

MODULE_DESCRIPTION("MP8864 voltage regulator driver");
MODULE_AUTHOR("AVM");
MODULE_LICENSE("GPL");
