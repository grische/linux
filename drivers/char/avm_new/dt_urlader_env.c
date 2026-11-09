// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (C) 2021 AVM GmbH <fritzbox_info@avm.de> */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/overflow.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

static struct {
	struct platform_device *pdev;
	struct ctl_table_header *ctl_table;

	char *env_string;
} ctx;

#define ENVIRONMENT_DEFAULT NULL
#define ENVIRONMENT_SLOT 0

#define FIRMWARE_VERSION_DEFAULT "avm"
#define FIRMWARE_VERSION_SLOT 1

#define ANNEX_DEFAULT "Ohne"
#define ANNEX_SLOT 2

static struct ctl_table sysctl_urlader[] = {
	[ENVIRONMENT_SLOT] = {
		.procname = "environment",
		.data = ENVIRONMENT_DEFAULT,
		.maxlen = 0,
		.mode = 0444,
		.proc_handler = proc_dostring,
	},
	[FIRMWARE_VERSION_SLOT] = {
		.procname = "firmware_version",
		.data = FIRMWARE_VERSION_DEFAULT,
		.maxlen = sizeof(FIRMWARE_VERSION_DEFAULT) - 1,
		.mode = 0444,
		.proc_handler = proc_dostring,
	},
	[ANNEX_SLOT] = {
		.procname = "annex",
		.data = ANNEX_DEFAULT,
		.maxlen = sizeof(ANNEX_DEFAULT) - 1,
		.mode = 0444,
		.proc_handler = proc_dostring,
	},
	{ },
};

static struct ctl_table sysctl_root[] = {
	{
		.procname = "urlader",
		.data = NULL,
		.maxlen = 0,
		.mode = 0555,
		.child = sysctl_urlader,
	},
	{ },
};

static int probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device_node *of_chosen;
	int nenv, i, ret;
	char *cur;
	const char *end;
	size_t len;

	if (ctx.pdev)
		return -EBUSY;

	ctx.pdev = NULL;
	ctx.ctl_table = NULL;
	ctx.env_string = NULL;

	of_chosen = of_find_node_by_path("/chosen");
	if (!of_chosen)
		return -EINVAL;

	nenv = of_property_count_strings(np, "avm,urlader-env-names");
	if (nenv < 0)
		return -EINVAL;

	len = 0;
	for (i = 0; i < nenv; ++i) {
		const char *name, *value;

		ret = of_property_read_string_index(np,
						    "avm,urlader-env-names",
						    i,
						    &name);
		if (ret)
			return ret;

		ret = of_property_read_string(of_chosen, name, &value);
		if (ret == -EINVAL)
			continue;
		else if (ret)
			return ret;

		/* + 1 for the separating tab */
		if (check_add_overflow(len, strlen(name), &len) ||
		    /* +1 for the separating \t */
		    check_add_overflow(len, (size_t)1, &len) ||
		    check_add_overflow(len, strlen(value), &len) ||
		    /* +1 for the terminating \n / \0 */
		    check_add_overflow(len, (size_t)1, &len))
			return -EOVERFLOW;
	}
	/* the terminating \0 with an empty environment */
	if (!len)
		len = 1;

	ctx.env_string = kzalloc(len, GFP_KERNEL);
	if (!ctx.env_string)
		return -ENOMEM;

	cur = ctx.env_string;
	end = cur + len;
	for (i = 0; i < nenv; ++i) {
		const char *name, *value;
		ssize_t vlen;

		ret = of_property_read_string_index(np,
						    "avm,urlader-env-names",
						    i,
						    &name);
		if (ret)
			goto fail_free;

		ret = of_property_read_string(of_chosen, name, &value);
		if (ret == -EINVAL)
			continue;
		else if (ret)
			goto fail_free;

		if (!strcmp(name, "firmware_version")) {
			sysctl_urlader[FIRMWARE_VERSION_SLOT].data = (char *)value;
			sysctl_urlader[FIRMWARE_VERSION_SLOT].maxlen = strlen(value);
		} else if (!strcmp(name, "annex")) {
			sysctl_urlader[ANNEX_SLOT].data = (char *)value;
			sysctl_urlader[ANNEX_SLOT].maxlen = strlen(value);
		}

		vlen = strscpy(cur, name, end - cur);
		if (WARN_ON(vlen < 0 || cur + vlen == end)) {
			ret = -E2BIG;
			goto fail_free;
		}
		cur += vlen;
		*(cur++) = '\t';

		vlen = strscpy(cur, value, end - cur);
		if (WARN_ON(vlen < 0 || cur + vlen == end)) {
			ret = -E2BIG;
			goto fail_free;
		}
		cur += vlen;
		*(cur++) = '\n';
	}
	/* remove the last newline */
	if (cur != ctx.env_string)
		cur--;
	*(cur++) = '\0';

	sysctl_urlader[ENVIRONMENT_SLOT].data = ctx.env_string;
	sysctl_urlader[ENVIRONMENT_SLOT].maxlen = cur - ctx.env_string;

	ctx.ctl_table = register_sysctl_table(sysctl_root);
	if (!ctx.ctl_table) {
		pr_err("failed to register \"/sys/urlader\"");
		ret = -ENODEV;
		goto fail_free;
	}
	ctx.pdev = pdev;
	of_node_put(of_chosen);
	return 0;

fail_free:
	if (ctx.ctl_table)
		unregister_sysctl_table(ctx.ctl_table);
	kfree(ctx.env_string);
	ctx.pdev = NULL;
	ctx.ctl_table = NULL;
	ctx.env_string = NULL;
	of_node_put(of_chosen);
	return ret;
}

static int remove(struct platform_device *pdev)
{
	if (ctx.pdev != pdev)
		return 0;
	unregister_sysctl_table(ctx.ctl_table);
	kfree(ctx.env_string);
	ctx.pdev = NULL;
	return 0;
}

static const struct of_device_id of_match_tbl[] = {
	{ .compatible = "avm,dt-urlader-env" },
	{ },
};
MODULE_DEVICE_TABLE(of, of_match_tbl);

static struct platform_driver plat_driver = {
	.probe = probe,
	.remove = remove,
	.driver = {
		.name = "dt-urlader-env",
		.of_match_table = of_match_tbl,
	},
};
module_platform_driver(plat_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Johannes Nixdorf <j.nixdorf@avm.de>");
MODULE_DESCRIPTION("Device Tree based access to the Urlader environment");
