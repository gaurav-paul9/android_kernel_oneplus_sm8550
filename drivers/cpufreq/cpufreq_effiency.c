/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 2021-2022 Oplus. All rights reserved.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/cpufreq_effiency.h>
#include <linux/kernel.h>
#include <linux/kernel_stat.h>
#include <linux/cpufreq.h>
#include <linux/sched.h>
#include <linux/cpu.h>
#include <linux/types.h>
#include <linux/sysfs.h>
#include <linux/string.h>
#include <linux/topology.h>
#include <linux/slab.h>
#include <linux/cpumask.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/pm_opp.h>
#include <linux/platform_device.h>

#define ABSENT_SOC_ID    0
#define SM8350_SOC_ID    415
#define SM8450_SOC_ID    457
#define SM7450_SOC_ID    506
#define SM8550_SOC_ID    519
#define PLATFORM_SM8350  "lahaina"
#define PLATFORM_SM8450  "waipio"
#define PLATFORM_SM7450  "diwali"
#define PLATFORM_SM8550  "kalama"

#define MAX_CLUSTER      3
#define SLIVER_CLUSTER   0
#define GOLDEN_CLUSTER   1
#define GOPLUS_CLUSTER   2

static unsigned int opp_number[MAX_CLUSTER];

#define MAX_CLUSTER_PARAMETERS 5
#define AFFECT_FREQ_VALUE1     0
#define AFFECT_THRES_SIZE1     1
#define AFFECT_FREQ_VALUE2     2
#define AFFECT_THRES_SIZE2     3
#define MASK_FREQ_VALUE        4

static int debug_mode;
module_param(debug_mode, int, 0664);

static int affect_mode;
module_param(affect_mode, int, 0664);

static int cluster0_effiency[MAX_CLUSTER_PARAMETERS];
module_param_array(cluster0_effiency, int, NULL, 0664);

static int cluster1_effiency[MAX_CLUSTER_PARAMETERS];
module_param_array(cluster1_effiency, int, NULL, 0664);

static int cluster2_effiency[MAX_CLUSTER_PARAMETERS];
module_param_array(cluster2_effiency, int, NULL, 0664);

static unsigned int platform_soc_id;

static DEFINE_RAW_SPINLOCK(power_effiency_lock);

static unsigned int sm8550_cluster_pd[MAX_CLUSTER] = {16, 20, 21};
static unsigned int sm8550_pd_sliver[16] = {
	0, 0, 1, 2, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 8
};
static unsigned int sm8550_pd_golden[20] = {
	0, 1, 2, 2, 2, 3, 3, 4, 4, 5, 5, 6, 7, 7, 8, 8, 8, 8, 9, 10
};
static unsigned int sm8550_pd_goplus[21] = {
	0, 1, 2, 2, 2, 3, 3, 4, 4, 4, 5, 5, 6, 7, 7, 8, 8, 8, 8, 9, 10
};

static unsigned int sm8350_cluster_pd[MAX_CLUSTER] = {16, 16, 19};
static unsigned int sm8350_pd_sliver[16] = {
	0, 0, 0, 0, 0, 1, 1, 2, 2, 2, 3, 3, 3, 4, 5, 5
};
static unsigned int sm8350_pd_golden[16] = {
	0, 1, 1, 2, 2, 3, 3, 4, 4, 4, 5, 6, 6, 7, 7, 8
};
static unsigned int sm8350_pd_goplus[19] = {
	0, 1, 1, 1, 2, 2, 3, 3, 4, 4, 4, 5, 6, 7, 7, 7, 7, 8, 8
};

static unsigned int sm8450_cluster_pd[MAX_CLUSTER] = {15, 18, 21};
static unsigned int sm8450_pd_sliver[15] = {
	0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 3, 4, 5, 6, 6
};
static unsigned int sm8450_pd_golden[18] = {
	0, 1, 1, 2, 2, 3, 3, 4, 4, 4, 5, 6, 6, 7, 7, 7, 8, 9
};
static unsigned int sm8450_pd_goplus[21] = {
	0, 1, 1, 1, 2, 2, 3, 3, 3, 4, 4, 4, 5, 6, 6, 7, 7, 7, 7, 8, 9
};

static unsigned int sm7450_cluster_pd[MAX_CLUSTER] = {9, 10, 10};
static unsigned int sm7450_pd_sliver[9] = {
	0, 1, 1, 2, 3, 3, 4, 5, 6
};
static unsigned int sm7450_pd_golden[10] = {
	0, 1, 2, 3, 4, 4, 5, 6, 7, 7
};
static unsigned int sm7450_pd_goplus[10] = {
	0, 1, 2, 3, 4, 4, 5, 6, 7, 7
};

static int get_cluster_num(struct cpufreq_policy *policy)
{
	int first_cpu, cluster_num;
	struct device *cpu_dev;

	first_cpu = cpumask_first(policy->related_cpus);
	cpu_dev = get_cpu_device(first_cpu);
	if (cpu_dev == NULL) {
		pr_err("failed to get cpu device\n");
		return -1;
	}

	cluster_num = topology_physical_package_id(cpu_dev->id);
	if (cluster_num >= MAX_CLUSTER)
		return -1;

	if (platform_soc_id == SM8350_SOC_ID) {
		if (opp_number[cluster_num] != sm8350_cluster_pd[cluster_num])
			return -1;
	} else if (platform_soc_id == SM8450_SOC_ID) {
		if (opp_number[cluster_num] != sm8450_cluster_pd[cluster_num])
			return -1;
	} else if (platform_soc_id == SM7450_SOC_ID) {
		if (opp_number[cluster_num] != sm7450_cluster_pd[cluster_num])
			return -1;
	} else if (platform_soc_id == SM8550_SOC_ID) {
		if (opp_number[cluster_num] != sm8550_cluster_pd[cluster_num])
			return -1;
	}

	return cluster_num;
}

static unsigned int *get_cluster_pd(struct cpufreq_policy *policy)
{
	int cluster_id;

	cluster_id = get_cluster_num(policy);
	if (platform_soc_id == SM8350_SOC_ID) {
		switch (cluster_id) {
		case SLIVER_CLUSTER:
			return sm8350_pd_sliver;
		case GOLDEN_CLUSTER:
			return sm8350_pd_golden;
		case GOPLUS_CLUSTER:
			return sm8350_pd_goplus;
		default:
			return NULL;
		}
	} else if (platform_soc_id == SM8450_SOC_ID) {
		switch (cluster_id) {
		case SLIVER_CLUSTER:
			return sm8450_pd_sliver;
		case GOLDEN_CLUSTER:
			return sm8450_pd_golden;
		case GOPLUS_CLUSTER:
			return sm8450_pd_goplus;
		default:
			return NULL;
		}
	} else if (platform_soc_id == SM7450_SOC_ID) {
		switch (cluster_id) {
		case SLIVER_CLUSTER:
			return sm7450_pd_sliver;
		case GOLDEN_CLUSTER:
			return sm7450_pd_golden;
		case GOPLUS_CLUSTER:
			return sm7450_pd_goplus;
		default:
			return NULL;
		}
	} else if (platform_soc_id == SM8550_SOC_ID) {
		switch (cluster_id) {
		case SLIVER_CLUSTER:
			return sm8550_pd_sliver;
		case GOLDEN_CLUSTER:
			return sm8550_pd_golden;
		case GOPLUS_CLUSTER:
			return sm8550_pd_goplus;
		default:
			return NULL;
		}
	} else {
		return NULL;
	}
}

static bool was_diff_powerdomain(struct cpufreq_policy *policy, unsigned int freq)
{
	int index, index_pre;
	unsigned int *cluster_pd;

	index = cpufreq_frequency_table_target(policy, freq, CPUFREQ_RELATION_L);
	index_pre = cpufreq_frequency_table_target(policy, freq - 1, CPUFREQ_RELATION_H);

	if (index == index_pre)
		return false;

	cluster_pd = get_cluster_pd(policy);
	if (cluster_pd) {
		if (cluster_pd[index] == cluster_pd[index_pre])
			return false;
		else
			return true;
	}

	return false;
}

static bool was_mask_freq(struct cpufreq_policy *policy, unsigned int freq)
{
	int cluster_id;

	cluster_id = get_cluster_num(policy);
	switch (cluster_id) {
	case SLIVER_CLUSTER:
		return (freq == cluster0_effiency[MASK_FREQ_VALUE]);
	case GOLDEN_CLUSTER:
		return (freq == cluster1_effiency[MASK_FREQ_VALUE]);
	case GOPLUS_CLUSTER:
		return (freq == cluster2_effiency[MASK_FREQ_VALUE]);
	default:
		return false;
	}
}

static unsigned int select_effiency_freq(struct cpufreq_policy *policy,
		unsigned int freq, unsigned int loadadj_freq)
{
	unsigned int freq_temp, index_temp, affect_thres;
	int cluster_id;

	index_temp = cpufreq_frequency_table_target(policy, freq - 1, CPUFREQ_RELATION_H);
	freq_temp = policy->freq_table[index_temp].frequency;

	if ((loadadj_freq > freq) || (loadadj_freq < freq_temp))
		return freq;

	cluster_id = get_cluster_num(policy);
	switch (cluster_id) {
	case SLIVER_CLUSTER:
		if ((cluster0_effiency[AFFECT_FREQ_VALUE2] > 0) &&
		    (freq >= cluster0_effiency[AFFECT_FREQ_VALUE2]))
			affect_thres = cluster0_effiency[AFFECT_THRES_SIZE2];
		else if ((cluster0_effiency[AFFECT_FREQ_VALUE1] > 0) &&
			 (freq >= cluster0_effiency[AFFECT_FREQ_VALUE1]))
			affect_thres = cluster0_effiency[AFFECT_THRES_SIZE1];
		else
			affect_thres = 0;
		break;
	case GOLDEN_CLUSTER:
		if ((cluster1_effiency[AFFECT_FREQ_VALUE2] > 0) &&
		    (freq >= cluster1_effiency[AFFECT_FREQ_VALUE2]))
			affect_thres = cluster1_effiency[AFFECT_THRES_SIZE2];
		else if ((cluster1_effiency[AFFECT_FREQ_VALUE1] > 0) &&
			 (freq >= cluster1_effiency[AFFECT_FREQ_VALUE1]))
			affect_thres = cluster1_effiency[AFFECT_THRES_SIZE1];
		else
			affect_thres = 0;
		break;
	case GOPLUS_CLUSTER:
		if ((cluster2_effiency[AFFECT_FREQ_VALUE2] > 0) &&
		    (freq >= cluster2_effiency[AFFECT_FREQ_VALUE2]))
			affect_thres = cluster2_effiency[AFFECT_THRES_SIZE2];
		else if ((cluster2_effiency[AFFECT_FREQ_VALUE1] > 0) &&
			 (freq >= cluster2_effiency[AFFECT_FREQ_VALUE1]))
			affect_thres = cluster2_effiency[AFFECT_THRES_SIZE1];
		else
			affect_thres = 0;
		break;
	default:
		affect_thres = 0;
		break;
	}

	if (abs((int)(loadadj_freq - freq_temp)) < affect_thres) {
		if (debug_mode)
			pr_info("cluster_id = %d loadadj_freq = %d freq_temp = %d affect_thres = %d\n",
				cluster_id, loadadj_freq, freq_temp, affect_thres);
		return freq_temp;
	}

	return freq;
}

unsigned int update_power_effiency_lock(struct cpufreq_policy *policy,
		unsigned int freq, unsigned int loadadj_freq)
{
	unsigned int temp_index;
	unsigned long flags;

	if (!policy || !affect_mode || !freq)
		return freq;

	raw_spin_lock_irqsave(&power_effiency_lock, flags);
	if (was_mask_freq(policy, freq)) {
		temp_index = cpufreq_frequency_table_target(policy, freq - 1,
							   CPUFREQ_RELATION_H);
		freq = policy->freq_table[temp_index].frequency;
	} else if (was_diff_powerdomain(policy, freq)) {
		freq = select_effiency_freq(policy, freq, loadadj_freq);
	}
	raw_spin_unlock_irqrestore(&power_effiency_lock, flags);

	return freq;
}
EXPORT_SYMBOL(update_power_effiency_lock);

static int cpufreq_pd_init(void)
{
	const char *prop_str;

	if (!of_root) {
		pr_info("of_root is null\n");
		return -1;
	}

	of_node_get(of_root);
	prop_str = of_get_property(of_root, "compatible", NULL);
	if (!prop_str) {
		pr_info("of_root's compatible is null\n");
		of_node_put(of_root);
		return -1;
	}

	if (strstr(prop_str, PLATFORM_SM8350))
		platform_soc_id = SM8350_SOC_ID;
	else if (strstr(prop_str, PLATFORM_SM8450))
		platform_soc_id = SM8450_SOC_ID;
	else if (strstr(prop_str, PLATFORM_SM7450))
		platform_soc_id = SM7450_SOC_ID;
	else if (strstr(prop_str, PLATFORM_SM8550))
		platform_soc_id = SM8550_SOC_ID;
	else
		platform_soc_id = ABSENT_SOC_ID;

	of_node_put(of_root);

	return 0;
}

static unsigned long is_init;

void frequence_opp_init(struct cpufreq_policy *policy)
{
	int first_cpu, cluster_id, opp_num;
	struct device *cpu_dev;

	if (!policy) {
		pr_err("policy was NULL\n");
		return;
	}

	first_cpu = cpumask_first(policy->related_cpus);

	if (test_bit(first_cpu, &is_init))
		return;

	cpu_dev = get_cpu_device(first_cpu);
	if (!cpu_dev) {
		pr_err("failed to get cpu device\n");
		return;
	}

	cluster_id = topology_physical_package_id(cpu_dev->id);
	if (cluster_id >= MAX_CLUSTER) {
		pr_err("error cluster id\n");
		return;
	}

	opp_num = dev_pm_opp_get_opp_count(cpu_dev);
	if (opp_number[cluster_id] != opp_num)
		opp_number[cluster_id] = opp_num;

	set_bit(first_cpu, &is_init);

	pr_info("opp_number: %d cluster_id: %d is_init: %ld\n",
		opp_number[cluster_id], cluster_id, is_init);
}
EXPORT_SYMBOL(frequence_opp_init);

static int __init cpufreq_effiency_init(void)
{
	cpufreq_pd_init();

	pr_info("cpufreq_effiency_init load\n");

	return 0;
}

static void __exit cpufreq_effiency_exit(void)
{
	pr_info("cpufreq effiency exit\n");
}
module_init(cpufreq_effiency_init);
module_exit(cpufreq_effiency_exit);
MODULE_DESCRIPTION("cpufreq_effiency");
MODULE_LICENSE("GPL v2");
