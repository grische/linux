#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/plat-ram.h>
#include <linux/mtd/partitions.h>
#if defined(CONFIG_TFFS)
#include <avm/tffs/tffs.h>
#endif
#include <linux/slab.h>
#include <linux/env.h>
#include <linux/moduleparam.h>
#include <linux/mtd/ubi.h>

/*
 * Container-Aufbau:
 * (mtd-nand "ubi" (ubivol "avm_filesys_0" (mtd-gluebi "filesystem"), ... ))
 *
 * Ablauf:
 *
 * (0. mtdram1= ist gesetzt und prepare_ram_resource bereitet
 * avm_mtd_ram_resource vor.)
 * 1. init registriert notification handler und ggf. ein neues platram device
 * 2. mtd_add_handler wird über neue devices in RAM oder NAND informiert.
 *    a) abhängig von mtd->name wird der root= parameter gesetzt, wobei RAM
 *    priorisiert wird
 *    b) "nand-tffs" wird für TFFS3 registriert
 *    c) für "ubi" wird der ubi= parameter gesetz
 * 3. gluebi stellt mtdblock devices auf ubi bereit und triggert
 *    mtd_add_handler, was mtd->name ändert und wieder nach Punkt 2 vorgeht.
 *
 * Wenn UBI_BLOCK vorhanden ist, wird alternativ zu 3. der ubi_add_notifier
 * benutzt.
 */

struct resource avm_mtd_ram_resource[] = { {
	/* for RAM loaded filesystem */
	.start = 0, // set by mtdram_setup()
	.end = 0, // dito
	.flags = IORESOURCE_MEM,
} };

static struct platdata_mtd_ram ram_data = {
	.mapname = "ram-filesystem",
	.bankwidth = 4, /* TODO warum 4? -phu */
	//    .partitions    =
	//    .nr_partitions =
};

static struct platform_device ram_device = {
	.name = "mtd-ram",
	.id = -1,
	.dev = { .platform_data = &ram_data },
	.num_resources = 1,
	//.resource       =
};

struct mtd_entry {
	char *urlader_name;
	char *runtime_name_0;
	char *runtime_name_1;
};

struct mtd_entry mtd_names_ubi[] = {
	{ "avm_filesys_0", "filesystem", "reserved-filesystem" },
	{ "avm_filesys_1", "reserved-filesystem", "filesystem" },
	{ "avm_config", "config", "config" },
	{ "avm_userdata", "nand-filesystem", "nand-filesystem" },
	{ NULL, NULL, NULL },
};

static int prioritize_ram = 0;

extern int __init root_dev_setup(char *line);
extern int __init ubi_mtd_param_parse(const char *val, struct kernel_param *kp);
extern struct mtd_part_parser avm_mtd_parser;

/* check for the partition name set by avmpart parser */
static int mtd_is_ram(struct mtd_info *mtd)
{
	return (mtd && !strcmp(mtd->name, "rootfs_ram"));
}

/* identifies root device candidates by name */
static int mtd_is_rootfs(struct mtd_info *mtd)
{
	const char *rootfs_names[] = { "filesystem", "rootfs_ram" };
	int i;

	for (i = 0; i < (int)ARRAY_SIZE(rootfs_names); i++) {
		pr_debug("[%s] checking name \"%s\"\n", __func__,
			 rootfs_names[i]);
		if (!strcmp(mtd->name, rootfs_names[i])) {
			return true;
		}
	}

	return false;
}

/* mount the root filesystem as if the root= kernel paramater were set */
static void announce_root(struct mtd_info *mtd)
{
	char root_dev[64];
	unsigned long ret;
	ret = snprintf(root_dev, sizeof(root_dev), "/dev/mtdblock%d",
		       mtd->index);

	if (ret >= sizeof(root_dev)) {
		pr_emerg("[%s] Unable to generate root device name!\n",
			 mtd->name);
		return;
	}

	pr_info("[%s] %s (%s) will be used as root device\n", __func__,
		mtd->name, root_dev);

	root_dev_setup(root_dev);
}

/*
 * search a name table for runtime MTD name. Returns original name if no
 * substitution is found
 */
static char *get_name(struct mtd_entry *nametable, const char *name)
{
	unsigned int i;
	unsigned long linux_fs_start;
	char *p, *new_name;
	int res;

	linux_fs_start = 0;
	new_name = (char *)name;

	p = prom_getenv("linux_fs_start");

	if (p != NULL) {
		res = kstrtoul(p, 0, &linux_fs_start);
		if (res == 0) {
			switch (linux_fs_start) {
			case 0:
			case 1:
				break;
			default:
				linux_fs_start = 0;
			}
		}
	}

	i = 0;
	while (nametable[i].urlader_name != NULL) {
		if (!strcmp(name, nametable[i].urlader_name)) {
			new_name = (linux_fs_start == 0) ?
					   nametable[i].runtime_name_0 :
					   nametable[i].runtime_name_1;
			break;
		}
		++i;
	}

	return new_name;
}

void rename_ubi(struct mtd_info *mtd)
{
	char *mtdname, *oldname;

	pr_debug("[%s] entering\n", __func__);
	if (!mtd->name) {
		pr_info("[%s] Called for unnamed mtd", __func__);
		return;
	}

	if (mtd->type != MTD_UBIVOLUME) {
		pr_debug("[%s] skipping mtd %s. Not an UBI volume\n", __func__,
			 mtd->name);
		return;
	}

	mtdname = get_name(&mtd_names_ubi[0], mtd->name);
	if (mtdname != mtd->name) {
		// gluebi allocates name strings dynamiccaly and frees them on
		// volume removal
		oldname = (char *)mtd->name;
		mtd->name = kstrdup(mtdname, GFP_KERNEL);

		if (mtd->name != NULL) {
			pr_debug("[%s] renamed mtd %s -> %s\n", __func__,
				 oldname, mtd->name);
			// FIXME: is it safe to free the string or should we just drop it?
			kfree(oldname);
		} else {
			mtd->name = oldname;
			pr_warning("[%s] Unable to rename mtd %s.\n", __func__,
				   mtd->name);
		}
	}
}

/* called by the mtd driver whenever a new mtd device is being added */
void __init mtd_add_handler(struct mtd_info *mtd)
{
	unsigned char *recover = NULL, *p = prom_getenv("firmware_info");

	pr_debug("[%s] entered\n", __func__);

	/*--------------------------------------------------------------------------------*\
	 * beim recover soll das UBI gelöscht und neu angelegt werden, also wird bei
	 * gesetzten recover nicht das UBI gemountet
	\*--------------------------------------------------------------------------------*/
	if (p) {
		recover = strstr(p, "recovered=2");
	}

	if (mtd->type == MTD_UBIVOLUME)
		rename_ubi(mtd);

	if (mtd_is_rootfs(mtd) && !prioritize_ram) {
		prioritize_ram = mtd_is_ram(mtd);
		announce_root(mtd);
#if defined(CONFIG_TFFS)
	} else if (!strcmp(mtd->name, "nand-tffs")) {
		TFFS3_Register_NAND(mtd);
		pr_debug("[%s] tffs3 on MTD %s\n", __func__, mtd->name);
#endif
	} else if (!strcmp(mtd->name, "ubi")) {
		if (!recover) {
			ubi_mtd_param_parse(mtd->name, NULL);
			pr_debug("[%s] UBI on MTD %s\n", __func__, mtd->name);
		} else {
			pr_debug(KERN_ERR "{%s} box recovered - %s\n", __func__,
				 recover);
		}
	} else {
		pr_debug("[%s] mtd \"%s\" is not handled by me.\n", __func__,
			 mtd->name);
	}
}

void mtd_rm_handler(struct mtd_info *mtd)
{
	pr_debug("[%s] mtd \"%s\" passing by.\n", __func__, mtd->name);
}

static struct mtd_notifier mtd_notifier_ops = { .add = mtd_add_handler,
						.remove = mtd_rm_handler };

static struct mtd_notifier mtd_postinit_notifier_ops = {
	.add = rename_ubi,
	.remove = mtd_rm_handler
};

static int register_ram_device(struct resource *ram_resource)
{
	pr_debug("[%s] registering ram device now\n", __func__);
	ram_device.resource = &ram_resource[0];
	if (platform_device_register(&ram_device)) {
		pr_debug("[%s] failed to register plat_mem device\n", __func__);
	}

	return 0;
}

#if defined(CONFIG_MTD_UBI_BLOCK)
/*------------------------------------------------------------------------------------------*\
 * UBI volume notifier
 * Check volume name against runtime name and if it contains the current
 * root filesysten, register the associated ubiblock-dev as kernel root-dev
\*------------------------------------------------------------------------------------------*/
void __init ubi_add_notifier(struct ubi_device_info *di __attribute__((unused)),
			     struct ubi_volume_info *vi)
{
	unsigned long ret;
	char root_dev[64];
	char *name;

	// skip check if a RAM MTD has already been registered
	if (prioritize_ram) {
		return;
	}

	if (!vi->name) {
		pr_info("[%s] Called for unnamed volume", __func__);
		return;
	}

	// just a sanity check. There should not be a volume called "filesystem"
	if (!strcmp(vi->name, "filesystem")) {
		pr_warn("[%s] found UBI volume named \"filesystem\". Potential "
			"problems ahead!\n",
			__func__);
		return;
	}

	// register ubiblock-dev as kernel root-dev if this volume's runtime
	// name will be "filesystem"
	name = get_name(&mtd_names_ubi[0], vi->name);
	if (!strcmp(name, "filesystem")) {
		ret = snprintf(root_dev, sizeof(root_dev), "/dev/ubiblock%d_%d",
			       vi->ubi_num, vi->vol_id);

		if (ret >= sizeof(root_dev)) {
			pr_emerg(
				"[%s] Unable to generate root device name /dev/ubiblock%d_%d!\n",
				name, vi->ubi_num, vi->vol_id);
			return;
		}

		pr_info("[%s] %s(%s) will be used as root device\n", __func__,
			root_dev, vi->name);

		root_dev_setup(root_dev);
	} else {
		pr_err("skip %s\n", vi->name);
	}
}

static int __init ubi_notify(struct notifier_block *nb __attribute__((unused)),
			     unsigned long l, void *ns_ptr)
{
	struct ubi_notification *nt = ns_ptr;

	switch (l) {
	case UBI_VOLUME_ADDED:
		pr_debug("[%s] new volume %s added\n", __func__, nt->vi.name);
		ubi_add_notifier(&nt->di, &nt->vi);
		break;
	case UBI_VOLUME_REMOVED:
	case UBI_VOLUME_RESIZED:
	case UBI_VOLUME_UPDATED:
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block ubi_notifier __initdata = {
	.notifier_call = ubi_notify,
};
#endif

static int __init avm_mtd_init(void)
{
	pr_debug("[%s] entered\n", __func__);

	register_mtd_parser(&avm_mtd_parser);
	register_mtd_user(&mtd_notifier_ops);
#if defined(CONFIG_MTD_UBI_BLOCK)
	ubi_register_volume_notifier(&ubi_notifier, 1);
#endif

	if (avm_mtd_ram_resource[0].start != avm_mtd_ram_resource[0].end) {
		/* register plat_ram mtd device as per cmdline request */
		register_ram_device(avm_mtd_ram_resource);
	}

	return 0;
}

/* this is called from free_initmem as late_initcall is still too early */
void __init avm_mtd_cleanup(void)
{
	pr_debug("[%s] entered\n", __func__);

	register_mtd_user(&mtd_postinit_notifier_ops);

#if defined(CONFIG_MTD_UBI_BLOCK)
	ubi_unregister_volume_notifier(&ubi_notifier, 1);
#endif
	unregister_mtd_user(&mtd_notifier_ops);
	deregister_mtd_parser(&avm_mtd_parser);
}

subsys_initcall(avm_mtd_init);
