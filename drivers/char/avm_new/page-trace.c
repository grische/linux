// SPDX-License-Identifier: GPL-2.0+

#include <linux/mm.h>

#include <avm/sammel/page-trace.h>

struct _page_extension {
	unsigned long pc;
};

struct _node_extension_table {
	struct pglist_data *pgdat;
	struct _page_extension *page_ext;
};

#define EXTENSION_TABLE_SIZE 2
static struct _node_extension_table node_extension_table[EXTENSION_TABLE_SIZE];

static struct _page_extension *get_page_extension(struct page *page)
{
	struct _page_extension *pg_exttab;
	pg_data_t *pgdat;
	unsigned long pfn;
	unsigned int nid = page_to_nid(page);

	if (unlikely(nid >= ARRAY_SIZE(node_extension_table))) {
		/*--- pr_err("%s: error1 nid=%u pfn=%lu\n", __func__, nid, page_to_pfn(page)); ---*/
		return NULL;
	}
	pgdat = node_extension_table[nid].pgdat;
	if (unlikely(pgdat == NULL)) {
		return NULL;
	}
	pfn = page_to_pfn(page) - pgdat->node_start_pfn;
	if (unlikely(pfn >= pgdat->node_spanned_pages)) {
		/*--- pr_err("%s: error2 nid=%u pfn=%lu\n", __func__, nid, page_to_pfn(page)); ---*/
		return NULL;
	}
	pg_exttab = node_extension_table[nid].page_ext;
	return &pg_exttab[pfn];
}

unsigned long avm_get_page_current_pc(struct page *page)
{
	struct _page_extension *pg_ext;
	struct page *head;

	head = compound_head(page);
	pg_ext = get_page_extension(head);

	if (!pg_ext)
		return 0;

	return pg_ext->pc;
}

void avm_set_page_current_pc(struct page *page, unsigned long pc)
{
	struct _page_extension *pg_ext;

	pg_ext = get_page_extension(page);

	if (!pg_ext)
		return;

	pg_ext->pc = pc;
}

static int __init avm_alloc_page_extension(void)
{
	pg_data_t *pgdat;
	struct _page_extension *pg_ext;

	for (pgdat = first_online_pgdat(); pgdat; pgdat = next_online_pgdat(pgdat)) {
		unsigned int alloc_size;

		if (!pgdat_end_pfn(pgdat))
			continue;

		if (pgdat->node_id >= (int)ARRAY_SIZE(node_extension_table)) {
			pr_err("%s error node_id %u exceed node_extension_table\n",
			       __func__, pgdat->node_id);
			continue;
		}
		if (node_extension_table[pgdat->node_id].page_ext) {
			continue;
		}
		alloc_size = sizeof(struct _page_extension) * pgdat->node_spanned_pages;
		pg_ext = alloc_pages_exact(alloc_size, GFP_KERNEL | __GFP_ZERO);
		if (pg_ext) {
			node_extension_table[pgdat->node_id].page_ext = pg_ext;
			node_extension_table[pgdat->node_id].pgdat    = pgdat;
		}
		pr_err("%s node_extension_table[%u] entries=%lu (size=%u) %s alloced\n",
		       __func__, pgdat->node_id,
		       pgdat->node_spanned_pages, alloc_size,
		       pg_ext ? "" : "not");
	}
	return 0;
}

arch_initcall(avm_alloc_page_extension);
