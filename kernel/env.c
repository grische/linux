#include <linux/init.h>
#include <linux/kconfig.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/env.h>

#include <asm/prom.h>
#define EMIF_ADDRESS
#define phys_to_virt /*--- nur beim ARM notwendig ---*/

#if defined(DEBUG) || defined(DEBUG_PROM_INIT)
static const int debug_prom_init = 1;
#else
static const int debug_prom_init;
#endif

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/

static char env_buffer[2048];
static char *_local_envp[64 * 2];
unsigned long fritz_box_hw_revision;
static int *_prom_envp;

static void _env_debug_print_env(char **myenv)
{
	int i;

	if (!debug_prom_init)
		return;

	pr_err("myenvp=%p\n", myenv);

	for (i = 0; myenv[i] && myenv[i + 1]; i += 2)
		pr_err("(%p)%s : (%p)%s\n", myenv[i], myenv[i], myenv[i + 1],
		       myenv[i + 1]);
}

static void _env_debug_print_cmdline(int argc, char **argv)
{
	int i;

	if (!debug_prom_init)
		return;

	pr_err("argc=%d argv=%p\n", argc, argv);

	for (i = 0; i < argc; i++)
		pr_err("[%d]: %s\n", i, argv[i]);
}

static void _env_debug_env_location(const char *prefix,
				    enum _env_location env_location, int *envp,
				    int index)
{
	if (!debug_prom_init)
		return;

	switch (env_location) {
	case ENV_LOCATION_FLASH:
		pr_err("[%s] envp[%u]= \"%s\"=\"%s\"\n", prefix, index,
		       (char *)phys_to_virt(envp[index]),
		       (char *)EMIF_ADDRESS(envp[index + 1]));
		break;

	case ENV_LOCATION_PHY_RAM:
		pr_err("[%s] envp[%u]= \"%s\"=\"%s\"\n", prefix, index,
		       (char *)phys_to_virt(envp[index]),
		       (char *)phys_to_virt(envp[index + 1]));
		break;

	default:
		pr_err("[%s] envp[%u]= \"%s\"=\"%s\"\n", prefix, index,
		       (char *)envp[index], (char *)envp[index + 1]);
	}
}

extern unsigned long fw_arg0, fw_arg1;

void __init env_init(int *fw_arg2, enum _env_location env_location)
{
	unsigned int i;
	char *p;
	static unsigned int once;
	int err;

	if (once)
		return;

	once = 1;

	_prom_envp = fw_arg2;

	_env_debug_print_env((char **)_prom_envp);
	_env_debug_print_cmdline(fw_arg0, (char **)fw_arg1);

	pr_info("[%s] 0x%08x[0]\n", __func__, _prom_envp[0]);
	pr_info("[%s] 0x%08x[1]\n", __func__, _prom_envp[1]);
	pr_info("[%s] 0x%08x[2]\n", __func__, _prom_envp[2]);

	if (((_prom_envp[1] & 0xC0000000) == 0x80000000) &&
	    ((_prom_envp[2] & 0xC0000000) == 0x80000000)) {
		pr_info("[%s] switch to ram location\n", __func__);
		env_location = ENV_LOCATION_PHY_RAM;
	} else {
		pr_info("[%s] switch to flash location\n", __func__);
		env_location = ENV_LOCATION_FLASH;
	}

	/*------------------------------------------------------------------*\
	\*------------------------------------------------------------------*/
	env_buffer[0] = '\0';
	p = env_buffer;

	/*------------------------------------------------------------------*\
	 * copy envp values from urlader memory to (non init) kernel memory *
	\*------------------------------------------------------------------*/
	for (i = 0; _prom_envp && _prom_envp[i] && _prom_envp[i + 1]; i += 2) {
		_env_debug_env_location(__func__, env_location, _prom_envp, i);

		_local_envp[i] = p;
		strcat(p, (char *)phys_to_virt(_prom_envp[i]));
		p += strlen((char *)phys_to_virt(_prom_envp[i])) + 1;

		while ((unsigned int)p & 0x3) /* align auf naechstes wort */
			*p++ = '\0';

		_local_envp[i + 1] = p;
		switch (env_location) {
		case ENV_LOCATION_FLASH:
			strcat(p, (char *)EMIF_ADDRESS(_prom_envp[i + 1]));
			p += strlen((char *)EMIF_ADDRESS(_prom_envp[i + 1])) +
			     1;
			break;
		case ENV_LOCATION_PHY_RAM:
			strcat(p, (char *)phys_to_virt(_prom_envp[i + 1]));
			p += strlen((char *)phys_to_virt(_prom_envp[i + 1])) +
			     1;
			break;
		case ENV_LOCATION_VIRT_RAM:
			strcat(p, (char *)(_prom_envp[i + 1]));
			p += strlen((char *)(_prom_envp[i + 1])) + 1;
			break;
		}
		while ((unsigned int)p & 0x3) /* align auf naechstes wort */
			*p++ = '\0';
	}

	_local_envp[i] = NULL;
	_local_envp[i + 1] = NULL;
	_prom_envp = (int *)_local_envp;

	err = kstrtoul(prom_getenv("HWRevision"), 10, &fritz_box_hw_revision);
	if (err)
		pr_err("%s: Unable to get hardware revision (HWRevision) from "
		       "PROM (error: %d)\n",
		       __func__, err);
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#if IS_ENABLED(CONFIG_OF_AVM_DT_ENV)
#include <linux/of_fdt.h>
#include <linux/of.h>
#include <linux/of_platform.h>
/*
 *  Retrieves the AMV Environment Variables from the Device Tree
 */
char *__must_check prom_getenv_device_tree(const char *envname)
{
	//Check if we have a device tree
	if (of_have_populated_dt()) {
		struct device_node *node;
		char *string = 0;
		//Go to the chosen node where the environment is stored
		node = of_find_node_by_path("/chosen");
		if (!node) {
			pr_err("Chosen node not found\n");
			return 0;
		}
		//check if the is a property with the name specified by envname
		if (of_property_read_string(node, envname,
					    (const char **)&string) == 0) {
			pr_debug("Found env %s\n", string);
			return string;
		} else {
			pr_err("Could not find Env '%s' in the device tree \n",
			       envname);
			return 0;
		}
	} else {
		pr_err("Device Tree not populated\n");
		return NULL;
	}
}
#endif

char *__must_check prom_getenv(const char *envname)
{
	/*
	 * Return a pointer to the given environment variable.
	 * In 64-bit mode: we're using 64-bit pointers, but all pointers
	 * in the PROM structures are only 32-bit, so we need some
	 * workarounds, if we are running in 64-bit mode.
	 */

#if IS_ENABLED(CONFIG_OF_AVM_DT_ENV)
	pr_debug("Retrieving %s from device tree\n", envname);
	return prom_getenv_device_tree(envname);
#else
	int i;
	int index = 0;

	if (!_prom_envp)
		return NULL;

	i = strlen(envname);

	for (i = strlen(envname); _prom_envp[index]; index += 2) {
		char *env_str = (char *)_prom_envp[index];

		if (strncmp(envname, env_str, i) == 0)
			return (char *)_prom_envp[index + 1];
	}

	return NULL;
#endif
}
EXPORT_SYMBOL(prom_getenv);

/* vim: set noexpandtab sw=8 ts=8 sts=0: */
