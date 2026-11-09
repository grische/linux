/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef INTERNAL_AVM_KPI_H
#define INTERNAL_AVM_KPI_H

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <avm/sammel/kpi.h>
#include <linux/kobject.h>

struct kpi_node *create_kpi_node(const char *name, struct kobject *parent);
void destroy_kpi_node(struct kpi_node *node);

#endif /* INTERNAL_AVM_KPI_H */
