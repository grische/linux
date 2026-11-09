/*
 *
 *   Copyright (C) 2017 AVM GmbH <fritzbox_info@avm.de>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef __KERNEL__
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#define ARRAY_SIZE(a)  (sizeof(a) / sizeof((a)[0]))
#define min_t(t, a, b) ((a) > (b) ? (b) : (a))
#define printk(args...)      printf(args)
#define pr_err(args...)      printf(args)
#define no_printk(args...)
#define KERN_ERR             ""
#define kzalloc(size, param) calloc(size, 1)
#define kfree(p)             free(p)
#define kstrndup(a, b, c)    strndup(a, b)
#define atomic_t             unsigned int
#define atomic_t             unsigned int
#define atomic_set(a, b)     (*(a)  = (b))
#define atomic_add(a, b)     (*(b) += (a))
#define atomic_read(a)           (*(a))
#define atomic_dec_and_test(a)   ((--*(a)) == 0)

/**
 */
enum _cond_type  {
	cond_type_unknown  = 0,
	cond_type_absolute = 1, /*--- 0x123   ---*/
	cond_type_addr     = 2, /*--- *0x123 ---*/
	cond_type_register = 3, /*--- $0, $1 ... ---*/
};
#else/*--- #ifndef __KERNEL__ ---*/
#include <linux/kernel.h>
#include <linux/ctype.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <asm/avm_enh/avm_enh.h>

#endif
/*--- #define DEBUG_CONDITION ---*/

#if defined(DEBUG_CONDITION)
#define DBG_TRC_PARSE(level, args...) { int a = level; while (a--) printk("\t"); printk(args); }

#define RLEVEL_INC() (ppriv_cond->bracket_count++)
#define RLEVEL_DEC() (ppriv_cond->bracket_count--)
#define DBG_TRC_CALC(level, args...) {int a = level; while (a--) printk("\t"); printk(args); }
/*--- #define DBG_TRC_CALC(level, args...) ---*/
#else /*--- #if defined(DEBUG_CONDITION) ---*/

#define DBG_TRC_CALC(level, args...)	no_printk(args)
#define DBG_TRC_PARSE(level, args...)	no_printk(args)
#define RLEVEL_INC()
#define RLEVEL_DEC()

#endif/*--- #if defined(DEBUG_CONDITION) ---*/

/**
 */
enum _cond_pre_operator {
	cond_pre_operator_nop         = 0,
	cond_pre_operator_minus       = 1,        /*--- - ---*/
	cond_pre_operator_neg         = 2,        /*--- ~ ---*/
	cond_pre_operator_logneg      = 3,        /*--- ! ---*/
	cond_pre_operator_lognegneg   = 4,        /*--- !! ---*/
};

/**
 * inklusive Prio!
 */
enum _cond_operator {
	cond_operator_nop            = 15,
	cond_operator_equal          = 9  | 0x10,
	cond_operator_not_equal      = 9  | 0x20,
	cond_operator_equalgreater   = 10 | 0x40,
	cond_operator_equalless      = 10 | 0x30,
	cond_operator_greater        = 10 | 0x20,
	cond_operator_less           = 10 | 0x10,
	cond_operator_leftshift      = 11 | 0x10,
	cond_operator_rightshift     = 11 | 0x20,
	cond_operator_plus           = 12 | 0x10,
	cond_operator_minus          = 12 | 0x20,
	cond_operator_mul            = 13 | 0x10,
	cond_operator_div            = 13 | 0x20,
	cond_operator_mod            = 13 | 0x30,
	cond_operator_or             = 6,
	cond_operator_and            = 8,
	cond_operator_xor            = 7,
	cond_operator_logical_or     = 4,
	cond_operator_logical_and    = 5,
};
#define get_operator_prio(operator) ((operator) & 0xF)
/**
 */
struct _condition_list {
	enum _cond_type type;
	enum _cond_pre_operator pre_operator;
	enum _cond_operator operator;
	unsigned long value;
	struct _condition_list *next;
	struct _condition_list *bracket;
};
#define MAGIC_CONDITIONHANDLE    0x434F4E44
#define VALID_CONDITIONSTRUCT(a) ((a) && (a)->magic == MAGIC_CONDITIONHANDLE)
/**
 */
struct _priv_parse_condition {
	unsigned long magic;
	atomic_t link;
	struct _condition_list *pcond_start;
	unsigned int pos;
	unsigned int max_conditions;
	int bracket_count;
	int calculate_error;
	void *ref;
	int (*get_value_by_type)(void *typeref, enum _cond_type type, unsigned long *value);
	int (*get_value_by_symbol)(void *symbolref, enum _cond_type type, char *txt, unsigned long *value);
};
/**
 */
static int get_value(struct _priv_parse_condition *ppriv_cond,
			const struct _condition_list *pcond, unsigned long *value)
{
	int ret = 0;

	switch (pcond->type) {
	case cond_type_addr:
	case cond_type_register:
		if (ppriv_cond->get_value_by_type) {
			return ppriv_cond->get_value_by_type(ppriv_cond->ref, pcond->type, value);
		}
		ret = -EINVAL;
		break;
	case cond_type_absolute:
		*value = pcond->value;
		break;
	case cond_type_unknown:
		*value = 0;
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}

/**
 */
static const char *print_preprocess_operator(enum _cond_pre_operator pre_operator)
{
	switch (pre_operator) {
	case cond_pre_operator_nop:
		return "";
	case cond_pre_operator_minus:
		return "-";
	case cond_pre_operator_neg:
		return "~";
	case cond_pre_operator_logneg:
		return "!";
	case cond_pre_operator_lognegneg:
		return "!!";
	}
	return "?pre?";
}

/**
 */
static unsigned long preprocess_value(const struct _condition_list *pcond, unsigned long value)
{
	switch (pcond->pre_operator) {
	case cond_pre_operator_nop:
		break;
	case cond_pre_operator_minus:
		value = -value;
		break;
	case cond_pre_operator_neg:
		value = ~value;
		break;
	case cond_pre_operator_logneg:
		value = !value;
		break;
	case cond_pre_operator_lognegneg:
		value = !!value;
		break;
	}
	return value;
}

/**
 */
static const char *print_operator(enum _cond_operator operator);

/**
 */
static unsigned long operate_values(enum _cond_operator operator, unsigned long value, unsigned long next_value)
{
	unsigned long ret = value;

	if (operator == cond_operator_nop)
		ret = next_value;
	if (operator == cond_operator_equal)
		ret = (value == next_value);
	if (operator == cond_operator_not_equal)
		ret = (value != next_value);
	if (operator == cond_operator_equalgreater)
		ret = (value >= next_value);
	if (operator == cond_operator_equalless)
		ret = (value <= next_value);
	if (operator == cond_operator_greater)
		ret = (value > next_value);
	if (operator == cond_operator_less)
		ret = (value < next_value);
	if (operator == cond_operator_leftshift)
		ret = (value << next_value);
	if (operator == cond_operator_rightshift)
		ret = (value >> next_value);
	if (operator == cond_operator_plus)
		ret = (value +  next_value);
	if (operator == cond_operator_minus)
		ret = (value -  next_value);
	if (operator == cond_operator_mul)
		ret = (value *  next_value);
	if (operator == cond_operator_div)
		ret = (value /  next_value);
	if (operator == cond_operator_mod)
		ret = (value %  next_value);
	if (operator == cond_operator_or)
		ret = (value |  next_value);
	if (operator == cond_operator_and)
		ret = (value &  next_value);
	if (operator == cond_operator_xor)
		ret = (value ^  next_value);
	if (operator == cond_operator_logical_or)
		ret = (value || next_value);
	if (operator == cond_operator_logical_and)
		ret = (value && next_value);
	return ret;
}
#define snprintf_add(ptxt, txtlen, args...)	do {					\
	if (ptxt == NULL) {								\
		pr_err(args);								\
	} else {									\
		int local_add_len;							\
											\
		local_add_len = snprintf(ptxt, txtlen, args);				\
		if (local_add_len > 0) {						\
			int tail = min_t(int, txtlen, local_add_len);			\
											\
			(ptxt) += tail, (txtlen) -= tail;				\
		}									\
	}										\
	} while (0)

/**
 */
static void _print_condition(struct _condition_list *pcond, char **txt, unsigned int *txt_len)
{
	char *ptxt = txt ? *txt : NULL;

	snprintf_add(ptxt, *txt_len, "%s%s", print_preprocess_operator(pcond->pre_operator),
					pcond->type == cond_type_register ? "$" :
					pcond->type == cond_type_addr     ? "*" :
					pcond->type == cond_type_absolute ? ""  : "?");
	if (pcond->bracket) {
		snprintf_add(ptxt, *txt_len, "(");
		if (txt)
			*txt = ptxt;
		_print_condition(pcond->bracket, txt, txt_len);
		ptxt = txt ? *txt : NULL;
		snprintf_add(ptxt, *txt_len, ")");
	} else {
		snprintf_add(ptxt, *txt_len, "0x%lx", pcond->value);
	}
	if (pcond->operator != cond_operator_nop) {
		snprintf_add(ptxt, *txt_len, " %s ", print_operator(pcond->operator));
	}
	if (pcond->next) {
		if (txt)
			*txt = ptxt;
		_print_condition(pcond->next, txt, txt_len);
		ptxt = txt ? *txt : NULL;
	}
	if (txt)
		*txt = ptxt;
}

/**
 */
static unsigned long calculate_condition_recursive(struct _priv_parse_condition *ppriv_cond,
						const struct _condition_list *pcond, unsigned long value,
						enum _cond_operator op)
{
	unsigned long erg;
	unsigned long next_value;
	int ret;

	RLEVEL_INC();
	if (pcond) {
		DBG_TRC_CALC(ppriv_cond->bracket_count, "%s: ----- [%p] value=%lx %s '%s' (next:'%s')\n", __func__,
								pcond,
								value,
								pcond->bracket ? "macro" : "",
								print_operator(op),
								print_operator(pcond->operator)
								);
	}
	while (pcond && (ppriv_cond->calculate_error == 0)) {
		if (pcond->bracket) {
			DBG_TRC_CALC(ppriv_cond->bracket_count, "-->%s:[%p]bracket enter\n", __func__, pcond->bracket);
			next_value = calculate_condition_recursive(ppriv_cond, pcond->bracket, 0, cond_operator_nop);
			next_value = preprocess_value(pcond, next_value);
			DBG_TRC_CALC(ppriv_cond->bracket_count, "-->%s:[%p]bracket leave: next_value=0x%lx type=%x\n", __func__,
						pcond, next_value, pcond->type);
			if (pcond->type != cond_type_absolute) {
				ret = get_value(ppriv_cond, pcond, &next_value);
				if (ret) {
					ppriv_cond->calculate_error = ret;
					continue;
				}
			}
		} else if (get_operator_prio(op) < get_operator_prio(pcond->operator)) {
			/*--- prio beachten! ---*/
			next_value = pcond->value;
			ret = get_value(ppriv_cond, pcond, &next_value);
			if (ret) {
				ppriv_cond->calculate_error = ret;
				continue;
			}
			next_value = preprocess_value(pcond, next_value);
			DBG_TRC_CALC(ppriv_cond->bracket_count, "-->%s:[%p] prio! value=0x%lx\n", __func__, pcond, next_value);
			next_value = calculate_condition_recursive(ppriv_cond, pcond->next, next_value, pcond->operator);
			DBG_TRC_CALC(ppriv_cond->bracket_count, "-->%s:[%p] after prio: value=0x%lx\n", __func__, pcond, next_value);
			pcond = NULL;
		} else {
			next_value = pcond->value;
			ret = get_value(ppriv_cond, pcond, &next_value);
			if (ret) {
				ppriv_cond->calculate_error = ret;
				continue;
			}
			next_value = preprocess_value(pcond, next_value);
		}
		erg = operate_values(op, value, next_value);
		DBG_TRC_CALC(ppriv_cond->bracket_count, "***** %s: %lx %s %lx -> erg=%lx\n", __func__,
						value, print_operator(op),
						next_value, erg);
		value    = erg;
		if (pcond) {
			op       = pcond->operator;
			pcond    = pcond->next;
		}
	}
	RLEVEL_DEC();
	return value;
}
#define SKIP_SPACES(txt) { while (*txt && (((*txt == ' ') || (*txt == '\t')))) txt++; }
#define SKIP_UNTIL_SEP(txt) { while (*txt && (isalnum(*txt) || (*txt == '_'))) txt++; }

/**
 */
static enum _cond_type parse_type(char **string)
{
	char *p = *string;

	SKIP_SPACES(p);
	if (*p == '$') {
		*string = p + 1;
		return cond_type_register;
	} else if (*p == '*') {
		*string = p + 1;
		return cond_type_addr;
	}
	return cond_type_absolute;
}

static const struct _cond_table2op  {
	const char *op_str;
	enum _cond_operator op;
} cond_table2op[] = {
	{ .op_str = "==", .op = cond_operator_equal		},
	{ .op_str = "!=", .op = cond_operator_not_equal		},
	{ .op_str = ">=", .op = cond_operator_equalgreater	},
	{ .op_str = "<=", .op = cond_operator_equalless		},
	{ .op_str = "||", .op = cond_operator_logical_or	},
	{ .op_str = "&&", .op = cond_operator_logical_and	},
	{ .op_str = "<<", .op = cond_operator_leftshift		},
	{ .op_str = ">>", .op = cond_operator_rightshift	},
};

static const struct _cond_table1op  {
	const char *op_str;
	enum _cond_operator op;
} cond_table1op[] = {
	{ .op_str = ">", .op = cond_operator_greater	},
	{ .op_str = "<", .op = cond_operator_less	},
	{ .op_str = "|", .op = cond_operator_or		},
	{ .op_str = "+", .op = cond_operator_plus	},
	{ .op_str = "-", .op = cond_operator_minus	},
	{ .op_str = "*", .op = cond_operator_mul	},
	{ .op_str = "/", .op = cond_operator_div	},
	{ .op_str = "%", .op = cond_operator_mod	},
	{ .op_str = "&", .op = cond_operator_and	},
	{ .op_str = "^", .op = cond_operator_xor	},
};

/**
 */
static const char *print_operator(enum _cond_operator operator)
{
	unsigned int i;

	if (operator == cond_operator_nop)
		return "";
	for (i = 0; i < ARRAY_SIZE(cond_table2op); i++) {
		const struct _cond_table2op *pop = &cond_table2op[i];

		if (pop->op == operator) {
			return pop->op_str;
		}
	}
	for (i = 0; i < ARRAY_SIZE(cond_table1op); i++) {
		const struct _cond_table1op *pop = &cond_table1op[i];

		if (pop->op == operator) {
			return pop->op_str;
		}
	}
	return "?op?";
}

/**
 */
static enum _cond_operator parse_operator(char **string)
{
	unsigned int i;
	char *p = *string;

	SKIP_SPACES(p);
	for (i = 0; i < ARRAY_SIZE(cond_table2op); i++) {
		const struct _cond_table2op *pop = &cond_table2op[i];

		if (*p == *pop->op_str && *(p+1) == *(pop->op_str + 1)) {
			*string = p + 2;
			return pop->op;
		}
	}
	for (i = 0; i < ARRAY_SIZE(cond_table1op); i++) {
		const struct _cond_table1op *pop = &cond_table1op[i];

		if (*p == *pop->op_str) {
			*string = p + 1;
			return pop->op;
		}
	}
	return cond_operator_nop;
}

/**
 */
static enum _cond_pre_operator parse_preoperator(char **string)
{
	char *p = *string;

	SKIP_SPACES(p);
	if (*p == '+') {
		*string = p + 1;
		return cond_pre_operator_nop;
	} else if (*p == '-') {
		*string = p + 1;
		return cond_pre_operator_minus;
	} else if (*p == '~') {
		*string = p + 1;
		return cond_pre_operator_neg;
	} else if (*p == '!') {
		p++;
		*string = p;
		if (*p == '!') {
			*string = p + 1;
			return cond_pre_operator_lognegneg;
		}
		return cond_pre_operator_logneg;
	}
	*string = p;
	return cond_pre_operator_nop;
}

/**
 */
static int parse_sign(char **string, int sign)
{
	int ret = 0;
	char *p = *string;

	SKIP_SPACES(p);
	if (*p == sign) {
		p++;
		ret = 1;
	}
	*string = p;
	return ret;
}

/**
 */
static int parse_value(struct _priv_parse_condition *ppriv_cond, char **string, enum _cond_type type,
			unsigned long *value)
{
	char symbol[256];
	char *p = *string;

	SKIP_SPACES(p);
	if (*p == '0' && *(p+1) == 'x') {
		int nmb = 0;

		sscanf(p, "0x%lx", value);
		p += 2;
		while (isxdigit(*p))
			p++, nmb++;
		if (nmb > 8)
			return -1;
	} else if (isdigit(*p)) {
		sscanf(p, "%lu", value);
		while (isdigit(*p))
			p++;
	} else if (ppriv_cond->get_value_by_symbol) {
		int len;
		char *pe = p;

		SKIP_UNTIL_SEP(pe);
		len = pe - p;
		if (len && (len < sizeof(symbol))) {
			memcpy(symbol, p, len);
			symbol[len] = 0;
		}
		if (ppriv_cond->get_value_by_symbol(ppriv_cond->ref, type, symbol, value)) {
			return -1;
		}
		type = cond_type_unknown;
		p = pe;
	} else {
		return -1;
	}
	*string = p;
	if ((type == cond_type_register) && ppriv_cond->get_value_by_symbol) {
		/*--- checke range des values ---*/
		return ppriv_cond->get_value_by_symbol(ppriv_cond->ref, type, NULL, value);
	}
	return 0;
}

/**
 * parse string und lege cond_list an
 * ret 0: ok
 */
static int parse_condition_recursive(char **string, struct _condition_list *pcond,
					struct _priv_parse_condition *ppriv_cond)
{
	int ret;
	struct _condition_list *pcond_bracket, *pcond_next;

	DBG_TRC_PARSE(ppriv_cond->bracket_count, "**** %s:[%p] '%s' %u avail=%u\n", __func__,
				pcond, *string, ppriv_cond->bracket_count,
				ppriv_cond->max_conditions - ppriv_cond->pos);
	if (ppriv_cond->pos >= ppriv_cond->max_conditions) {
		return -ENOMEM;
	}
	pcond->pre_operator = parse_preoperator(string);
	pcond->type         = parse_type(string);
	if (parse_sign(string, '(')) {
		ppriv_cond->bracket_count++;
		pcond->operator      = cond_pre_operator_nop;
		ppriv_cond->pos++;
		DBG_TRC_PARSE(ppriv_cond->bracket_count, "%s:[%p] enter bracket '%s' pos=%u brackets=%u\n", __func__,
					pcond, *string,
					ppriv_cond->pos,
					ppriv_cond->bracket_count);
		pcond_bracket = ppriv_cond->pcond_start + ppriv_cond->pos;
		ret = parse_condition_recursive(string, pcond_bracket, ppriv_cond);
		if (ret == 0) {
			DBG_TRC_PARSE(ppriv_cond->bracket_count, "%s:[%p] leave bracket '%s' pos=%u brackets=%u\n", __func__,
						pcond_bracket, *string,
						ppriv_cond->pos, ppriv_cond->bracket_count);
			pcond->bracket = pcond_bracket;
		} else {
			DBG_TRC_PARSE(ppriv_cond->bracket_count, "%s: parse-error after enter-bracket\n", __func__);
			return ret;
		}
	} else {
		if (parse_value(ppriv_cond, string, pcond->type, &pcond->value)) {
			DBG_TRC_PARSE(ppriv_cond->bracket_count, "%s: value- parse-error\n", __func__);
			return -ERANGE;
		}
		DBG_TRC_PARSE(ppriv_cond->bracket_count, "%s:[%p] : '%s' set: value=0x%lx type=%u\n", __func__,
								pcond, *string, pcond->value, pcond->type);
	}
	if (parse_sign(string, ')')) {
		ppriv_cond->bracket_count--;
		pcond->operator      = cond_operator_nop;
		return 0;
	}
	DBG_TRC_PARSE(ppriv_cond->bracket_count, "%s:[%p] '%s' %s(value=0x%lx) bracket_count=%u\n", __func__,
							pcond, *string,
							pcond->bracket ?  "erg from bracket" : "",
							pcond->value,
								ppriv_cond->bracket_count);
	pcond->operator = parse_operator(string);
	if (pcond->operator != cond_operator_nop) {
		ppriv_cond->pos++;
		DBG_TRC_PARSE(ppriv_cond->bracket_count, "%s: [%p] next: '%s' pos=%u\n", __func__,
							pcond, *string, ppriv_cond->pos);
		pcond_next = ppriv_cond->pcond_start + ppriv_cond->pos;
		ret = parse_condition_recursive(string, pcond_next, ppriv_cond);
		if (ret == 0) {
			pcond->next = pcond_next;
			DBG_TRC_PARSE(ppriv_cond->bracket_count, "%s: [%p] next: '%p'\n", __func__, pcond, pcond->next);
		}
		return ret;
	}
	return 0;
}

/**
 * Daten nachbereiten -> brackets hinzufuegen je nach prio
 * ret 0: ok
 */
static int prior_condition_recursive(struct _condition_list *pcond, struct _priv_parse_condition *ppriv_cond)
{
	int ret;
	struct _condition_list *pcond_new, *pcond_bracket = NULL, *pcond_next;
	enum _cond_operator op, last_op = 0;

	ppriv_cond->bracket_count++;
	while (pcond) {
		if (pcond->bracket) {
			ret = prior_condition_recursive(pcond->bracket, ppriv_cond);
			if (ret) {
				ppriv_cond->bracket_count--;
				return ret;
			}
		}
		op = pcond->operator;
		DBG_TRC_PARSE(ppriv_cond->bracket_count, "%s:[%p][%p] %s versus %s\n", __func__,
					pcond, pcond->next, print_operator(last_op),
					print_operator(op));
		pcond_next = NULL;
		if (get_operator_prio(op) < get_operator_prio(last_op)) {
			if (pcond_bracket) {
				DBG_TRC_PARSE(ppriv_cond->bracket_count,
						"%s:[%p] value=0x%lx %s versus %s new bracket until %p value=%0lx\n", __func__,
						pcond_bracket,
						pcond_bracket->value,
						print_operator(last_op), print_operator(op),
						pcond,
						pcond->value);
				ppriv_cond->pos++;
				if (ppriv_cond->pos >= ppriv_cond->max_conditions) {
					ppriv_cond->bracket_count--;
					return -ENOMEM;
				}
				pcond_new              = ppriv_cond->pcond_start + ppriv_cond->pos;
				memcpy(pcond_new, pcond_bracket, sizeof(*pcond_new));

				pcond_bracket->operator = pcond->operator;
				pcond_bracket->type     = pcond->type;
				pcond_bracket->value    = (unsigned int)-1;
				pcond_bracket->bracket  = pcond_new;
				pcond_bracket->next     = pcond->next;
				pcond->operator         = cond_operator_nop;
				pcond_next              = pcond->next;
				pcond->next             = NULL;
				pcond_bracket           = NULL;
			}
		} else if (pcond_bracket == NULL) {
			pcond_bracket = pcond;
			last_op       = op;
		}
		if (pcond_next) {
			pcond = pcond_next;
		} else {
			pcond = pcond->next;
		}
	}
	ppriv_cond->bracket_count--;
	return 0;
}

/**
 * alle brackets die bereits absolut sind schon mal zusammenrechnen
 */
static void optimize_conditions(struct _condition_list *pcond, struct _priv_parse_condition *ppriv_cond)
{
	struct _condition_list *pcond_bracket = NULL;
	unsigned int value;
	int (*local_get_value_by_type)(void *ref, enum _cond_type type, unsigned long *value);

	local_get_value_by_type = ppriv_cond->get_value_by_type;
	ppriv_cond->get_value_by_type = NULL;    /*--- get_value_by_type-Funktion austragen ---*/
	while (pcond) {
		if (pcond->bracket) {
			pcond_bracket = pcond->bracket;
			ppriv_cond->calculate_error = 0;
			value = calculate_condition_recursive(ppriv_cond, pcond->bracket, 0, cond_operator_nop);
			if (ppriv_cond->calculate_error == 0) {
				pcond_bracket->bracket       = NULL;
				pcond_bracket->next          = NULL;
				pcond_bracket->value         = value;
				pcond_bracket->operator      = cond_operator_nop;
				pcond_bracket->pre_operator  = cond_pre_operator_nop;
				pcond_bracket->type          = cond_type_absolute;
			}
		}
		pcond = pcond->next;
	}
	ppriv_cond->get_value_by_type = local_get_value_by_type;
}

/**
 * \\brief: Handle anlegen fuer prepare_condition()
 * Export-Funktion
 * max_conditions: Anzahl der Conditions (0: default =100)
 * (*get_value_by_type): Callback fuer Aufloesung ret = 0: ok sonst Fehler
 * (*get_value_by_symbol): Callback fuer Symbol oder Registername (wenn type = cond_type_register)
 * falls cond_type_register: get_value_by_symbol() wird auch mit sym=NULL aufgerufen um register-idx abzuchecken!
 * ret = 0: ok sonst Fehler
 */
void *condition_alloc(int (*get_value_by_type)(void *symbolref, enum _cond_type type, unsigned long *value),
			int (*get_value_by_symbol)(void *typeref, enum _cond_type type, char *sym, unsigned long *value),
			unsigned int max_conditions)
{
	struct _priv_parse_condition *ppriv_cond;
	unsigned int size;

	if (max_conditions == 0) {
		max_conditions = 100;
	}
	size = sizeof(struct _priv_parse_condition) + sizeof(struct _condition_list) * max_conditions;
	ppriv_cond = kzalloc(size, GFP_KERNEL);
	if (ppriv_cond) {
		ppriv_cond->max_conditions      = max_conditions;
		ppriv_cond->pcond_start         = (struct _condition_list *)(ppriv_cond + 1);
		ppriv_cond->get_value_by_type   = get_value_by_type;
		ppriv_cond->get_value_by_symbol = get_value_by_symbol;
		ppriv_cond->magic               = MAGIC_CONDITIONHANDLE;
		atomic_set(&ppriv_cond->link, 1);
	}
	return ppriv_cond;
}

/**
 * Export-Funktion
 * link erzeugen
 */
void condition_get(void *handle)
{
	struct _priv_parse_condition *ppriv_cond = (struct _priv_parse_condition *)handle;

	if (!VALID_CONDITIONSTRUCT(ppriv_cond)) {
		return;
	}
	atomic_add(1, &ppriv_cond->link);
	/*--- pr_err("%s %p link=%u\n", __func__, ppriv_cond, atomic_read(&ppriv_cond->link)); ---*/
}

/**
 * link freigeben
 */
void condition_put(void *handle)
{
	struct _priv_parse_condition *ppriv_cond = (struct _priv_parse_condition *)handle;

	if (!VALID_CONDITIONSTRUCT(ppriv_cond)) {
		return;
	}
	if (atomic_read(&ppriv_cond->link) == 0) {
		/*--- printk("%s link already zero\n", __func__); ---*/
		return;
	}
	if (atomic_dec_and_test(&ppriv_cond->link)) {
		ppriv_cond->magic = 0;
		/*--- printk("%s free %p\n", __func__, ppriv_cond); ---*/
		kfree(ppriv_cond);
		return;
	}
	/*--- pr_info("%s %p link=%u\n", __func__, ppriv_cond, atomic_read(&ppriv_cond->link)); ---*/
}

/**
 * Export-Funktion
 */
void condition_free(void *handle)
{
	condition_put(handle);
}
/**
 * Export-Funktion
 * falls txt = NULL -> stdout
 */
char *condition_print(void *handle, char *txt, unsigned int txt_len)
{
	struct _priv_parse_condition *ppriv_cond = (struct _priv_parse_condition *)handle;
	char *start_txt  = txt;

	if (txt && txt_len)
		txt[0] = 0;
	if (!VALID_CONDITIONSTRUCT(ppriv_cond)) {
		return "";
	}
	if (ppriv_cond->pcond_start) {
		_print_condition(ppriv_cond->pcond_start, txt ? &txt : NULL, &txt_len);
	}
	return (start_txt && txt_len) ? start_txt : "";
}
/**
 * Export-Funktion
 * Expression parsen
 * Ruft (falls noetig) Callback get_value_by_symbol() auf
 * stringlen: 0 string ist nullterminiert
 * ret 0: ok
 */
int condition_parse(void *handle, void *symbolref, char *string, unsigned int stringlen)
{
	char *pstring = NULL;
	int ret;
	struct _priv_parse_condition *ppriv_cond = (struct _priv_parse_condition *)handle;

	if (!VALID_CONDITIONSTRUCT(ppriv_cond)) {
		return -EINVAL;
	}
	if (string == NULL) {
		return -EINVAL;
	}
	if (stringlen != 0) {
		pstring = kstrndup(string, stringlen, GFP_KERNEL);
		if (pstring == NULL) {
			return -ENOMEM;
		}
		string = pstring;
	}
	DBG_TRC_PARSE(0, "%s: orig='%s'\n", __func__, string);
	ppriv_cond->ref           = symbolref;
	ppriv_cond->pos           = 0;
	ppriv_cond->bracket_count = 0;
	memset(ppriv_cond->pcond_start, 0, sizeof(struct _condition_list) * ppriv_cond->max_conditions);

	ret = parse_condition_recursive(&string, ppriv_cond->pcond_start, ppriv_cond);
	kfree(pstring);
	if (ret) {
		DBG_TRC_PARSE(0, "parse-error ret=%d\n", ret);
		return ret;
	}
	if (ppriv_cond->bracket_count) {
		DBG_TRC_PARSE(0, "braket-error %d\n", ppriv_cond->bracket_count);
		return -EINVAL;
	}
	ret  = prior_condition_recursive(ppriv_cond->pcond_start, ppriv_cond);
	if (ret) {
		DBG_TRC_PARSE(0, "prio-list error %d\n", ret);
	}
	/*--- print_condition(ppriv_cond, NULL, 0); ---*/
	optimize_conditions(ppriv_cond->pcond_start, ppriv_cond);
	return ret;
}
/**
 * Export-Funktion
 * Expression berechnen
 * Ruft (falls noetig) Callback get_value_by_type() auf
 */
int condition_calculate(void *handle, void *typeref, unsigned long *value)
{
	struct _priv_parse_condition *ppriv_cond = (struct _priv_parse_condition *)handle;
	struct _priv_parse_condition tmp;

	if (!VALID_CONDITIONSTRUCT(ppriv_cond)) {
		return -EINVAL;
	}
	memcpy(&tmp, ppriv_cond, sizeof(struct _priv_parse_condition));
	tmp.ref             = typeref;
	tmp.calculate_error   = 0;
	tmp.get_value_by_type = ppriv_cond->get_value_by_type;
#if defined(DEBUG_CONDITION)
	tmp.bracket_count     = 0;
#endif/*--- #if defined(DEBUG_CONDITION) ---*/
	*value = calculate_condition_recursive(&tmp, ppriv_cond->pcond_start, 0, cond_operator_nop);
	return tmp.calculate_error;
}

/**
 */
#define p_to_idx(cond) (int)((cond) ? (((cond) - (start))) : -1)

/**
 */
static void print_cond_list(struct _condition_list cond[], int entries)
{
	struct _condition_list *start = cond;
	unsigned int i;

	pr_err("\n-------------------------\n");
	for (i = 0; i < entries; i++) {
		struct _condition_list *pcond = &cond[i];

		if (pcond->type == cond_type_unknown) {
			break;
		}
		pr_err("[%02u][%p]type=%-6s pre-op='%-5s' op='%-5s' value=0x%08lx next=%3d bracket=%3d (%p)/(%p)\n",
						i, pcond,
						pcond->type == cond_type_register ? "$" :
						pcond->type == cond_type_addr     ? "*" :
						pcond->type == cond_type_absolute ? " " : "?",
						print_preprocess_operator(pcond->pre_operator),
						print_operator(pcond->operator), pcond->value,
						p_to_idx(pcond->next), p_to_idx(pcond->bracket),
						pcond->next, pcond->bracket);

	}
}

/**
 * Export-Funktion
 */
void condition_debuglist(void *handle)
{
	struct _priv_parse_condition *ppriv_cond = (struct _priv_parse_condition *)handle;

	if (!VALID_CONDITIONSTRUCT(ppriv_cond)) {
		pr_err("%s invalid handle %p\n", __func__, handle);
		return;
	}
	print_cond_list(ppriv_cond->pcond_start, ppriv_cond->max_conditions);
}
#if !defined(__KERNEL__)
#define CALUCLATE(a)  do { erg = (a); \
			printf("Teststring: '%s'\n", #a);\
			condition_parse(handle, NULL, #a, 0);\
			condition_print(handle, NULL, 0); \
			printf("\n"); \
			condition_debuglist(handle); \
			if (condition_calculate(handle, NULL, &erg2)) { \
				printf("calculate_error\n"); \
				exit(-1); \
			} \
			if (erg != erg2)  {        \
				printf("erg(%x) != erg-calc(%x)\n", erg, erg2); \
				exit(-1); \
			} else {  \
				printf("pass ok: erg(%x)\n", erg2); \
			}						\
			} while (0)

/**
 */
static int get_value_by_type(void *ref, enum _cond_type type, unsigned long *value)
{
	unsigned long *addr = (unsigned long *)(*value);

	switch (type) {
	case cond_type_register:
		/*--- *value  ; ---*/
		break;
	case cond_type_addr:
		printf("%s: %s(%d) addr=%p\n", __func__, type == cond_type_register ? "register" : "addr", type, addr);
		*value = *addr;
		break;
	default:
		printf("%s: type=%u addr=%p failed\n", __func__, type, addr);
		return -1;
	}
	printf("%s: %s(%d) addr=%p value=0x%lx\n", __func__, type == cond_type_register ? "register" : "addr", type, addr, *value);
	return 0;
}

/**
 */
static int get_symbol_by_type(void *ref, enum _cond_type type, char *symbol, unsigned long *value)
{
	int ret = -1;

	switch (type) {
	case cond_type_register:
		if (symbol == NULL) {
			if (*value < 32) {
				ret = 0;
			}
		}
		break;
	default:
		if (strcmp(symbol, "testvar") == 0) {
			*value = (unsigned long)ref;
			ret    = 0;
		} else {
			*value = 0;
		}
	}
	if (ret < 0) {
		printf("%s: type=%d '%s' value=0x%lx failed\n", __func__, type, symbol, *value);
	} else {
		printf("%s: %s(%d) '%s' value=0x%lx\n", __func__, type == cond_type_register ? "register" : "addr", type, symbol, *value);
	}
	return ret < 0 ? ret : 0;
}
/**
 * gcc expression.c -Wall -g -o cond
 */
int main(int argc, char *argv[0])
{
	char txt[512];
	void *handle;
	unsigned long erg, erg2, testvar = 12;

	txt[0] = 0;
	handle = condition_alloc(get_value_by_type, get_symbol_by_type, 0);

	CALUCLATE(0x24 == 0x24 - 0x0 && 0x1);
	CALUCLATE(!35);
	CALUCLATE(35 > 12);
	CALUCLATE(35 >= 12);
	CALUCLATE(35 != 12);
	CALUCLATE(35 <= 12);
	CALUCLATE(35 <  12);
	CALUCLATE(!!35);
	CALUCLATE(~1234);
	CALUCLATE(-1234);
	CALUCLATE(0x1 + 0x2 == 0x7-4);
	CALUCLATE(!!0x1 + ~(~0x2));
	CALUCLATE((0x1) + 0x2 + 33 == (0x7-3)*9 && 12 == 6 + 6);
	CALUCLATE(0x1 + 0x2 + 33 == (0x7-3)*9 - 0 && 1);
	CALUCLATE((0x1+0x2));
	CALUCLATE(0x1 + 0x2 + 33 == (0x7-3)*((9 - 6) * 3) && 1);
	CALUCLATE(0x1  + (2 << (10 | 1)));
	CALUCLATE(0x1050 *  (2 << (10 | 1)));
	printf("congratulation - always pass ok\n");

	condition_parse(handle, NULL, "0x1 + $2 + 33 == (0x7-3)*9 - 0 && $1", 0);
	condition_parse(handle, NULL, "0x1 + $1 + 33 == (0x7-3)*9 - 0 && $a0", 0);
	condition_parse(handle, NULL, "0x1 + $1 + 33 == (0x7-3)*9 - 0 && $a0", 0);
	condition_parse(handle, &testvar, "(*(testvar) == 12)", 0);
	condition_parse(handle, NULL, "0x12 + 0x10 == 0x20 + 0x11 -1", 0);
	/*--- condition_parse(handle, NULL,"$0 == 0", 0); ---*/
	printf("%s\n", condition_print(handle, txt, sizeof(txt)));
	condition_debuglist(handle);
	printf("\n");
	condition_calculate(handle, NULL, &erg);
	printf("\n");
	condition_free(handle);
	return 0;
}
#endif/*--- #if !defined(__KERNEL__) ---*/
