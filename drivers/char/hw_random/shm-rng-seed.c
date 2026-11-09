// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2021 AVM GmbH <fritzbox_info@avm.de> */

#include <linux/crc32.h>
#include <linux/dma-mapping.h>
#include <linux/hw_random.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/random.h>
#include <linux/shm-rng-seed.h>
#include <linux/slab.h>

#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>

#define INIT_MAGIC	0x462cf6bd
#define UNINIT_MAGIC	0x9dfe551d
#define EARLY_MAGIC	0x1fcf236b

struct region {
	phys_addr_t base;
	phys_addr_t size;
};

/* Priting functions for shared code between early (no struct device
 * dev exists) and late (should use dev_printk) init.
 */

struct pr_ctx {
	enum {
		PR_CTX_TYPE_DEV,
		PR_CTX_TYPE_MEM,
	} type;

	union {
		struct device *dev;
		struct reserved_mem *mem;
	};
};

static inline struct pr_ctx pr_ctx_from_dev(struct device *dev)
{
	return (struct pr_ctx) {
		.type = PR_CTX_TYPE_DEV,
		.dev = dev,
	};
}

static inline struct pr_ctx pr_ctx_from_mem(struct reserved_mem *mem)
{
	return (struct pr_ctx) {
		.type = PR_CTX_TYPE_MEM,
		.mem = mem,
	};
}

#define pr_ctx_printk(pr_ctx, level, fmt, ...) ( \
	(pr_ctx)->type == PR_CTX_TYPE_DEV \
		? dev_printk(level, (pr_ctx)->dev, fmt, ##__VA_ARGS__) \
		: printk(level "%s: " fmt, (pr_ctx)->mem->name, ##__VA_ARGS__) \
)

#define pr_ctx_err(pr_ctx, ...) pr_ctx_printk(pr_ctx, KERN_ERR, __VA_ARGS__)
#define pr_ctx_info(pr_ctx, ...) pr_ctx_printk(pr_ctx, KERN_INFO, __VA_ARGS__)

struct dma_ctx {
	struct device *dev;
	dma_addr_t dma;
};

struct shm_rng_header {
	__be32 magic;
	__be32 csum_or_len;
	char buf[];
};

struct credit_entropy_data {
	struct delayed_work work;
	struct device *dev;
	size_t entropy_bytes;
};

static void shm_rng_seed_credit_entropy_work(struct work_struct *work)
{
	struct credit_entropy_data *data =
		container_of(work, struct credit_entropy_data, work.work);

	add_hwgenerator_randomness(NULL, 0, data->entropy_bytes * 8);
	dev_info(data->dev,
		 "Credited %zd bytes of entropy in the system pool\n",
		 data->entropy_bytes);

	kfree(data);
}

static int shm_rng_seed_read_common(struct pr_ctx *dev, struct shm_rng_header *mem,
				    struct dma_ctx *dma, struct region *region, bool early)
{
	struct shm_rng_header header;
	u32 csum = -1;
	char *cur;
	const char *end;
	char buf[512];
	size_t mem_buf_len = region->size - sizeof(*mem);
	size_t chunk, entropy_bytes;
	struct credit_entropy_data *credit_entropy_data;

	if (dma)
		dma_sync_single_for_cpu(dma->dev, dma->dma,
					sizeof(*mem), DMA_BIDIRECTIONAL);
	header = *mem;
	mem->magic = htonl(UNINIT_MAGIC);
	mem->csum_or_len = 0xdeadbeef;
	if (dma)
		dma_sync_single_for_device(dma->dev, dma->dma,
					   sizeof(*mem), DMA_BIDIRECTIONAL);

	switch (ntohl(header.magic)) {
	case UNINIT_MAGIC:
		pr_ctx_err(dev, "The memory region is set as uninitialized\n");
		return -EINVAL;
	case INIT_MAGIC:
		if (dma)
			dma_sync_single_for_cpu(dma->dev, dma->dma + sizeof(*mem),
						mem_buf_len, DMA_BIDIRECTIONAL);

		end = mem->buf + mem_buf_len;
		for (cur = mem->buf; cur < end; cur += chunk) {
			chunk = min(sizeof(buf), (size_t)(end - cur));

			memcpy(buf, cur, chunk);
			memset(cur, 0, chunk);

			add_device_randomness(buf, chunk);
			csum = crc32(csum, buf, chunk);
		}

		if (header.csum_or_len != csum) {
			pr_ctx_err(dev,
				   "Found a checksum mismatch: expected %08x, got %08x\n",
				   csum, header.csum_or_len);
			return -EINVAL;
		}

		entropy_bytes = end - mem->buf;
		pr_ctx_info(dev,
			    "Added %zd random bytes to the system pool\n",
			    entropy_bytes);

		if (dma)
			dma_sync_single_for_device(dma->dev, dma->dma + sizeof(*mem),
						   mem_buf_len, DMA_BIDIRECTIONAL);

		if (early) {
			if (entropy_bytes > U32_MAX)
				entropy_bytes = U32_MAX;

			if (dma)
				dma_sync_single_for_cpu(dma->dev, dma->dma,
							sizeof(*mem), DMA_TO_DEVICE);

			mem->magic = htonl(EARLY_MAGIC);
			mem->csum_or_len = htonl(entropy_bytes);

			if (dma)
				dma_sync_single_for_device(dma->dev, dma->dma,
							   sizeof(*mem), DMA_TO_DEVICE);

			return 0;
		}

		break;
	case EARLY_MAGIC:
		if (early) {
			/* Early reserved memory init after a manual call
			 * to early_init_shm_rng_seed. Keep the original
			 * header intact for the late init.
			 */
			*mem = header;
			return 0;
		}
		entropy_bytes = header.csum_or_len;
		break;
	default:
		pr_ctx_err(dev,
			   "The memory region starts with an unexpected value: %08x\n",
			   ntohl(header.magic));
		return -EINVAL;
	}

	credit_entropy_data = kzalloc(sizeof(*credit_entropy_data),
				      GFP_KERNEL);
	if (!credit_entropy_data)
		return -ENOMEM;

	INIT_DELAYED_WORK(&credit_entropy_data->work,
			  &shm_rng_seed_credit_entropy_work);

	if (entropy_bytes > SIZE_MAX / 8)
		entropy_bytes = SIZE_MAX / 8;
	credit_entropy_data->entropy_bytes = entropy_bytes;
	credit_entropy_data->dev = WARN_ON(dev->type != PR_CTX_TYPE_DEV) ? NULL : dev->dev;
	schedule_delayed_work(&credit_entropy_data->work, 0);

	return 0;
}

#if IS_BUILTIN(CONFIG_SHM_RNG_SEED)
#define EARLY_COMPATIBLE "avm,shm-rng-seed-early"

static int __init __early_init_shm_rng_seed_read(struct reserved_mem *reserved)
{
	struct pr_ctx pr_ctx = pr_ctx_from_mem(reserved);
	void *mem;
	struct region region;
	ssize_t ret;

	mem = phys_to_virt(reserved->base);
	region.base = reserved->base;
	region.size = reserved->size;
	ret = shm_rng_seed_read_common(&pr_ctx, mem, NULL, &region, true);
	if (ret < 0)
		pr_ctx_info(&pr_ctx, "Failed to add random bytes to the system pool: %zd\n", ret);
	return ret;
}

static __init int __early_init_shm_rng_seed_fdt_iter(unsigned long node,
						     const char *uname,
						     int depth __maybe_unused,
						     void *data __maybe_unused)
{
	struct reserved_mem rmem = { 0 };
	int len;
	const __be32 *reg;

	if (!of_flat_dt_is_compatible(node, EARLY_COMPATIBLE))
		return 0;

	reg = of_get_flat_dt_prop(node, "reg", &len);
	if (!reg || len != (dt_root_addr_cells + dt_root_size_cells) * sizeof(*reg)) {
		pr_err("shm-rng-seed: failed to read the reg property on %s\n", uname);
		return -ENOENT;
	}

	rmem.name = uname;
	rmem.base = dt_mem_next_cell(dt_root_addr_cells, &reg);
	rmem.size = dt_mem_next_cell(dt_root_size_cells, &reg);

	return __early_init_shm_rng_seed_read(&rmem);
}

int __init early_init_shm_rng_seed(void)
{
	return of_scan_flat_dt(__early_init_shm_rng_seed_fdt_iter, NULL);
}

RESERVEDMEM_OF_DECLARE(shm_rng_seed_read, EARLY_COMPATIBLE, __early_init_shm_rng_seed_read);
#endif

static int late_shm_rng_seed_read(struct device *dev, struct region *region)
{
	struct pr_ctx pr_ctx = pr_ctx_from_dev(dev);
	void *mem;
	struct dma_ctx dma = { .dev = dev };
	int ret;

	mem = memremap(region->base, region->size, MEMREMAP_WB);
	if (!mem)
		return -ENOMEM;

	dma.dma = dma_map_single(dev, mem, region->size, DMA_BIDIRECTIONAL);
	if (dma_mapping_error(dev, dma.dma)) {
		memunmap(mem);
		return -ENOMEM;
	}

	ret = shm_rng_seed_read_common(&pr_ctx, mem, &dma, region, false);

	dma_unmap_single(dev, dma.dma, region->size, DMA_BIDIRECTIONAL);
	memunmap(mem);

	if (ret < 0)
		dev_info(dev, "Failed to add random bytes to the system pool: %zd\n", ret);
	return ret;
}

struct cb_data {
	struct device *dev;
	struct region region;
	struct notifier_block ready_nb;
	struct delayed_work ready_work;
};

static int shm_rng_seed_write(struct device *dev, struct region *region)
{
	u32 csum = -1;
	dma_addr_t dma;
	struct shm_rng_header *mem;
	char *beg, *cur;
	const char *end;
	char buf[512];
	size_t mem_buf_len = region->size - sizeof(*mem);
	size_t chunk;

	mem = memremap(region->base, region->size, MEMREMAP_WB);
	if (!mem) {
		dev_err(dev,
			"Failed to map the shared memory region\n");
		return -ENOMEM;
	}

	dma = dma_map_single(dev, mem, region->size, DMA_TO_DEVICE);
	if (dma_mapping_error(dev, dma)) {
		dev_err(dev,
			"Failed to create a DMA mapping for the shared memory region\n");
		memunmap(mem);
		return -ENOMEM;
	}

	dma_sync_single_for_cpu(dev, dma, sizeof(*mem), DMA_TO_DEVICE);
	mem->magic = htonl(UNINIT_MAGIC);
	mem->csum_or_len = 0xdeadbeef;
	dma_sync_single_for_device(dev, dma, sizeof(*mem), DMA_TO_DEVICE);

	dma_sync_single_for_cpu(dev, dma + sizeof(*mem),
				mem_buf_len, DMA_TO_DEVICE);

	beg = mem->buf;
	end = mem->buf + mem_buf_len;
	for (cur = beg; cur < end; cur += chunk) {
		chunk = min(sizeof(buf), (size_t)(end - cur));

		get_random_bytes(buf, chunk);
		memcpy(cur, buf, chunk);

		csum = crc32(csum, buf, chunk);
	}

	dma_sync_single_for_device(dev, dma + sizeof(*mem),
				   mem_buf_len, DMA_TO_DEVICE);

	dma_sync_single_for_cpu(dev, dma, sizeof(*mem), DMA_TO_DEVICE);
	mem->magic = htonl(INIT_MAGIC);
	mem->csum_or_len = htonl(csum);
	dma_sync_single_for_device(dev, dma, sizeof(*mem), DMA_TO_DEVICE);

	dma_unmap_single(dev, dma, region->size, DMA_TO_DEVICE);
	memunmap(mem);

	dev_info(dev, "Added %zu random bytes to the shared memory area\n",
		 (size_t)(end - beg));
	return 0;
}

static DECLARE_WAIT_QUEUE_HEAD(write_wq);
static atomic_t write_ctr;

static void shm_rng_seed_ready_work(struct work_struct *work)
{
	struct cb_data *data = container_of(work, struct cb_data, ready_work.work);

	if (shm_rng_seed_write(data->dev, &data->region) < 0)
		dev_err(data->dev, "Failed to write random data to the SHM region.\n");
	if (atomic_dec_and_test(&write_ctr))
		wake_up_all(&write_wq);
	kfree(data);
}

static int shm_rng_seed_ready_cb(struct notifier_block *nb,
				  unsigned long action, void *data)
{
	struct cb_data *container_data = container_of(nb, struct cb_data, ready_nb);

	/* Get out of a potential IRQ context. */
	schedule_delayed_work(&container_data->ready_work, 0);

	return 0;
}

static int shm_rng_seed_random_wait_and_write(struct device *dev, struct region *region)
{
	int ret;
	struct cb_data *cb_data;

	cb_data = kzalloc(sizeof(*cb_data), GFP_KERNEL);
	if (!cb_data)
		return -ENOMEM;

	cb_data->dev = dev;
	cb_data->region = *region;
	cb_data->ready_nb.notifier_call = shm_rng_seed_ready_cb;
	INIT_DELAYED_WORK(&cb_data->ready_work, &shm_rng_seed_ready_work);

	atomic_inc(&write_ctr);
	ret = register_random_ready_notifier(&cb_data->ready_nb);
	if (ret == -EALREADY) {
		shm_rng_seed_ready_work(&cb_data->ready_work.work);
		ret = 0;
	}

	return ret;
}

int shm_rng_seed_wait_all(void)
{
	return wait_event_interruptible(write_wq, atomic_read(&write_ctr) == 0);
}
EXPORT_SYMBOL_GPL(shm_rng_seed_wait_all);

static int shm_rng_seed_probe(struct platform_device *pdev)
{
	struct region region;
	struct device_node *mem_node;
	struct resource res;
	bool is_write;
	int err;

	is_write = device_property_read_bool(&pdev->dev, "avm,write");
	mem_node = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	if (!mem_node) {
		dev_err(&pdev->dev,
			"Failed to locate the matching DT memory resource\n");
		return -EINVAL;
	}
	err = of_address_to_resource(mem_node, 0, &res);
	of_node_put(mem_node);
	if (err) {
		dev_err(&pdev->dev,
			"Failed to parse the matching DT memory resource\n");
		return err;
	}

	if (resource_size(&res) < sizeof(struct shm_rng_header) + 1) {
		dev_err(&pdev->dev,
			"Did not reserve enough memory for proper operation\n");
		return -EINVAL;
	}

	region.size = resource_size(&res);
	region.base = res.start;

	if (is_write)
		return shm_rng_seed_random_wait_and_write(&pdev->dev, &region);
	else
		return late_shm_rng_seed_read(&pdev->dev, &region);
}

static const struct of_device_id shm_rng_dt_ids[] = {
	{ .compatible = "avm,shm-rng-seed" },
	{ },
};
MODULE_DEVICE_TABLE(of, shm_rng_dt_ids);

static struct platform_driver shm_rng_seed_driver = {
	.probe = shm_rng_seed_probe,
	.driver = {
		.name = "shm-rng-seed",
		.of_match_table = shm_rng_dt_ids,
	},
};
module_platform_driver(shm_rng_seed_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Johannes Nixdorf <j.nixdorf@avm.de>");
MODULE_DESCRIPTION("one-time shared memory based rng seed communication");
