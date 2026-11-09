/*
  GPL LICENSE SUMMARY

  Copyright(c) 2016 Intel Corporation.

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
  The full GNU General Public License is included in this distribution
  in the file called LICENSE.GPL.

  Contact Information:
    Intel Corporation
    2200 Mission College Blvd.
    Santa Clara, CA  97052
*/

#define pr_fmt(fmt) "L2NAT:%s:%d " fmt, __func__, __LINE__

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kref.h>
#include <linux/netdevice.h>
#include <linux/if_pppox.h>
#include "l2nat.h"

/* search for an ip-MAC mapping entry, should be called with rcu_read_lock() */
static struct hash_entry *l2nat_entry_get_rcu(struct l2nat_dev *l2nat,
					      __be32 ip)
{
	struct hash_entry *ent;

	hash_for_each_possible_rcu (l2nat->hash, ent, hlist, ntohl(ip)) {
		if (ent->ip == ip)
			return ent;
	}

	return NULL;
}

/* search for an UNIQ-MAC mapping entry,
 * should be called with rcu_read_lock()
 */
static struct pppoe_uniq_hash_entry
	*
	l2nat_pppoe_uniq_entry_get_rcu(
		struct l2nat_dev *l2nat,
		u64 uniq)
{
	struct pppoe_uniq_hash_entry *ent;

	hash_for_each_possible_rcu (l2nat->pppoe_uniq_hash, ent, hlist, uniq) {
		if (ent->host_uniq == uniq)
			return ent;
	}

	return NULL;
}

/* search for an SessionID-MAC mapping entry,
 * should be called with rcu_read_lock()
 */
static struct pppoe_sid_hash_entry
	*
	l2nat_pppoe_ses_entry_get_rcu(
		struct l2nat_dev *l2nat,
		u16 sid,
		unsigned char *serv_mac)
{
	struct pppoe_sid_hash_entry *ent;

	hash_for_each_possible_rcu (l2nat->pppoe_sid_hash, ent, hlist, sid) {
		if ((ent->sid == sid) && !memcmp(serv_mac,
						 ent->serv_mac,
						 ETH_ALEN))
			return ent;
	}

	return NULL;
}

/* search for an SID-MAC mapping entry, should be called with rcu_read_lock() */
struct pppoe_sid_hash_entry
	*
	l2nat_pppoe_ses_entry_get(
		struct l2nat_dev *l2nat,
		unsigned char *serv_mac,
		u16 sid)
{
	struct pppoe_sid_hash_entry *ent;

	rcu_read_lock();
	ent = l2nat_pppoe_ses_entry_get_rcu(l2nat, sid, serv_mac);
	if (ent)
		l2nat_pppoe_ses_entry_hold(ent);
	rcu_read_unlock();

	return ent;
}

/* search for an ip-MAC mapping entry in the hash table */
struct hash_entry *l2nat_entry_get(struct l2nat_dev *l2nat, __be32 ip)
{
	struct hash_entry *ent;

	rcu_read_lock();
	ent = l2nat_entry_get_rcu(l2nat, ip);
	if (ent)
		l2nat_entry_hold(ent);
	rcu_read_unlock();

	return ent;
}

/* search for an ip-MAC mapping entry in the hash table */
struct pppoe_uniq_hash_entry *l2nat_pppoe_uniq_entry_get(
	struct l2nat_dev *l2nat,
	u64 uniq)
{
	struct pppoe_uniq_hash_entry *ent;

	rcu_read_lock();
	ent = l2nat_pppoe_uniq_entry_get_rcu(l2nat, uniq);
	if (ent)
		l2nat_pppoe_uniq_entry_hold(ent);
	rcu_read_unlock();

	return ent;
}

/* free an ip-MAC mapping entry */
void l2nat_entry_release(struct kref *kref)
{
	struct hash_entry *ent = container_of(kref, struct hash_entry, kref);
	struct l2nat_dev *l2nat = ent->l2nat;

	del_timer(&ent->timer);
	l2n_dbg_hash("%s: freeing entry %pI4 %pM\n", l2nat->dev->name,
		     &ent->ip, &ent->mac);

	l2nat->stats.entries_del++;

	kfree(ent);
	atomic_dec(&l2nat->ent_count);

	/* wake up anyone waiting for all entries to release */
	wake_up_interruptible(&l2nat->wq);
	l2nat_dev_put(l2nat);
}

/* free an ip-MAC mapping entry */
void l2nat_pppoe_uniq_entry_release(struct kref *kref)
{
	struct pppoe_uniq_hash_entry *ent = container_of(kref,
							 struct pppoe_uniq_hash_entry,
							 kref);
	struct l2nat_dev *l2nat = ent->l2nat;

	del_timer(&ent->timer);
	l2n_dbg_hash("%s: freeing entry uniq:%llx\n", l2nat->dev->name,
		     ent->host_uniq);

	l2nat->stats.pppoe_disc_entries_del++;

	kfree(ent);
	atomic_dec(&l2nat->pppoe_ent_count);

	/* wake up anyone waiting for all entries to release */
	wake_up_interruptible(&l2nat->wq);
	l2nat_dev_put(l2nat);
}

/* free an SID-MAC mapping entry */
void l2nat_pppoe_ses_entry_release(struct kref *kref)
{
	struct pppoe_sid_hash_entry *ent = container_of(kref,
							struct pppoe_sid_hash_entry,
							kref);
	struct l2nat_dev *l2nat = ent->l2nat;

	del_timer(&ent->timer);
	l2n_dbg_hash("%s: freeing entry SID:0x%04x\n", l2nat->dev->name,
		     ent->sid);

	l2nat->stats.pppoe_ses_entries_del++;

	kfree(ent);
	atomic_dec(&l2nat->pppoe_ses_ent_count);

	/* wake up anyone waiting for all entries to release */
	wake_up_interruptible(&l2nat->wq);
	l2nat_dev_put(l2nat);
}

/* put an ip-MAC mapping entry for the hash, called from call_rcu */
static void l2nat_entry_free_rcu(struct rcu_head *rcu)
{
	struct hash_entry *ent = container_of(rcu, struct hash_entry, rcu);

	l2nat_entry_put(ent);
}

/* put an ip-MAC mapping entry for the hash, called from call_rcu */
static void l2nat_pppoe_uniq_entry_free_rcu(struct rcu_head *rcu)
{
	struct pppoe_uniq_hash_entry *ent = container_of(rcu,
							 struct pppoe_uniq_hash_entry,
							 rcu);

	l2nat_pppoe_uniq_entry_put(ent);
}

/* put an ip-MAC mapping entry for the hash, called from call_rcu */
static void l2nat_pppoe_ses_entry_free_rcu(struct rcu_head *rcu)
{
	struct pppoe_sid_hash_entry *ent = container_of(rcu,
							struct pppoe_sid_hash_entry,
							rcu);

	l2nat_pppoe_ses_entry_put(ent);
}

/* delete ip-MAC mapping entry, should be locked by l2nat->lock */
static void __l2nat_pppoe_uniq_entry_del(struct pppoe_uniq_hash_entry *ent)
{
	l2n_dbg_hash("%s: freeing entry uniq:%llx\n", ent->l2nat->dev->name,
		     ent->host_uniq);

	hash_del_rcu(&ent->hlist);
	call_rcu(&ent->rcu, l2nat_pppoe_uniq_entry_free_rcu);

	/* entry has a pending timer, force it to expire to relase the
	 * counter reference.
	 * note - a running timer is not pending
	 */
	if (timer_pending(&ent->timer))
		mod_timer(&ent->timer, jiffies);
}

/* delete ip-MAC mapping entry, should be locked by l2nat->lock */
static void __l2nat_pppoe_ses_entry_del(struct pppoe_sid_hash_entry *ent)
{
	l2n_dbg_hash("%s: freeing entry SID:0x%04x\n", ent->l2nat->dev->name,
		     ent->sid);

	hash_del_rcu(&ent->hlist);
	call_rcu(&ent->rcu, l2nat_pppoe_ses_entry_free_rcu);

	/* entry has a pending timer, force it to expire to relase the
	 * counter reference.
	 * note - a running timer is not pending
	 */
	if (timer_pending(&ent->timer))
		mod_timer(&ent->timer, jiffies);
}

/* delete ip-MAC mapping entry, should be locked by l2nat->lock */
static void __l2nat_entry_del(struct hash_entry *ent)
{
	l2n_dbg_hash("%s: deleting entry %pI4 %pM\n", ent->l2nat->dev->name,
		     &ent->ip, &ent->mac);

	hash_del_rcu(&ent->hlist);
	call_rcu(&ent->rcu, l2nat_entry_free_rcu);

	/* entry has a pending timer, force it to expire to relase the
	 * counter reference.
	 * note - a running timer is not pending
	 */
	if (timer_pending(&ent->timer))
		mod_timer(&ent->timer, jiffies);
}

/* flush all ip-MAC mapping entries */
void l2nat_hash_flush(struct l2nat_dev *l2nat)
{
	int i;
	struct hash_entry *ent;
	struct pppoe_uniq_hash_entry *ppp_uniq_ent;
	struct pppoe_sid_hash_entry *ppp_ses_ent;
	struct hlist_node *tmp;

	l2n_dbg_hash("flushing entries for %s\n", l2nat->dev->name);

	spin_lock_bh(&l2nat->lock);
	hash_for_each_safe (l2nat->hash, i, tmp, ent, hlist)
		__l2nat_entry_del(ent);
	hash_for_each_safe (l2nat->pppoe_uniq_hash,
			    i, tmp, ppp_uniq_ent, hlist)
		__l2nat_pppoe_uniq_entry_del(ppp_uniq_ent);
	hash_for_each_safe (l2nat->pppoe_sid_hash,
			    i, tmp, ppp_ses_ent, hlist)
		__l2nat_pppoe_ses_entry_del(ppp_ses_ent);
	spin_unlock_bh(&l2nat->lock);
}

/* flush all ip-MAC mapping entries, and wait for them to be freed */
void l2nat_hash_flush_sync(struct l2nat_dev *l2nat)
{
	l2nat_hash_flush(l2nat);

	l2n_dbg_hash("waiting for entries to be freed\n");
	wait_event_interruptible(l2nat->wq,
				 atomic_read(&l2nat->ent_count) == 0);
	wait_event_interruptible(l2nat->wq,
				 atomic_read(&l2nat->pppoe_ent_count) == 0);
}

/* initialize a new ip-MAC mapping entry */
static inline void l2nat_entry_init(struct l2nat_dev *l2nat,
				    struct hash_entry *ent,
				    __be32 ip, u8 *mac)
{
	ent->ip = ip;
	memcpy(&ent->mac, mac, ETH_ALEN);
	atomic_long_set(&ent->last_pkt_timestamp, jiffies);
	ent->first_pkt_timestamp = jiffies;
	ent->tx_packets++;
	ent->l2nat = l2nat;
	l2nat_dev_hold(l2nat);
	kref_init(&ent->kref);
	init_timer(&ent->timer);
	ent->timer.function = entry_aging_timer_fn;
	ent->timer.data = (unsigned long)ent;
	INIT_HLIST_NODE(&ent->hlist);
	atomic_inc(&l2nat->ent_count);

	l2n_dbg_hash("%s: initialized entry %pI4 %pM\n", l2nat->dev->name, &ip,
		     mac);
}

/* initialize a new PPPoE Host UNIQ mapping entry */
static inline void
l2nat_pppoe_uniq_entry_init(struct l2nat_dev *l2nat,
			    struct pppoe_uniq_hash_entry *ent,
			    u64 uniq, u8 *cli_mac)
{
	ent->host_uniq = uniq;
	memcpy(&ent->cli_mac, cli_mac, ETH_ALEN);
	atomic_long_set(&ent->last_pkt_timestamp, jiffies);
	ent->first_pkt_timestamp = jiffies;
	ent->tx_packets++;
	ent->l2nat = l2nat;
	l2nat_dev_hold(l2nat);
	kref_init(&ent->kref);
	init_timer(&ent->timer);
	ent->timer.function = pppoe_uniq_entry_aging_timer_fn;
	ent->timer.data = (unsigned long)ent;
	INIT_HLIST_NODE(&ent->hlist);
	atomic_inc(&l2nat->pppoe_ent_count);
}

/* initialize a new PPPoE session ID mapping entry */
static inline void l2nat_pppoe_ses_entry_init(struct l2nat_dev *l2nat,
					      struct pppoe_sid_hash_entry *ent,
					      u8 *serv_mac,
					      u8 *cli_mac,
					      u16 sid)
{
	ent->sid = sid;
	memcpy(&ent->cli_mac, cli_mac, ETH_ALEN);
	memcpy(&ent->serv_mac, serv_mac, ETH_ALEN);
	atomic_long_set(&ent->last_pkt_timestamp, jiffies);
	ent->first_pkt_timestamp = jiffies;
	ent->tx_packets++;
	ent->l2nat = l2nat;
	l2nat_dev_hold(l2nat);
	kref_init(&ent->kref);
	init_timer(&ent->timer);
	ent->timer.function = pppoe_ses_entry_aging_timer_fn;
	ent->timer.data = (unsigned long)ent;
	INIT_HLIST_NODE(&ent->hlist);
	atomic_inc(&l2nat->pppoe_ses_ent_count);

	l2n_dbg_hash("%s: initialized entry sid:0x%04x cli:%pM serv:%pM\n",
		     l2nat->dev->name, sid, cli_mac, serv_mac);
}

int l2nat_pppoe_ses_entry_check_add(struct l2nat_dev *l2nat,
				    unsigned char *serv_mac,
				    unsigned char *cli_mac,
				    u16 sid)
{
	int ret = 0;
	struct pppoe_sid_hash_entry *old_e, *new_e;
	unsigned long interval;

	if (unlikely(!serv_mac) || unlikely(!cli_mac))
		return 0;

	if (unlikely(!l2nat->hash_en))
		return 0;

	old_e = l2nat_pppoe_ses_entry_get(l2nat, serv_mac, sid);

	/* entry exists and mac did not change */
	if (likely(old_e && !memcmp(old_e->serv_mac, serv_mac, ETH_ALEN)))
		goto update_entry;

	new_e = kzalloc(sizeof(*new_e), GFP_ATOMIC);
	if (!new_e) {
		l2n_err("%s: failed to allocate entry! UNIQ:0x%04x serv:%pM, cli:%pM\n",
			l2nat->dev->name, sid, serv_mac, cli_mac);
		return -1;
	}

	l2nat_pppoe_ses_entry_init(l2nat, new_e, serv_mac, cli_mac, sid);

	spin_lock_bh(&l2nat->lock);

	hash_add_rcu(l2nat->pppoe_sid_hash, &new_e->hlist, sid);

	/* mac address changed, replace old entry with new one */
	if (old_e && hash_hashed(&old_e->hlist))
		__l2nat_pppoe_ses_entry_del(old_e);

	spin_unlock_bh(&l2nat->lock);

	l2n_dbg_hash("%s: added entry SID:0x%04x serv:%pM cli:%pM\n",
		     l2nat->dev->name, sid, serv_mac, cli_mac);

	if (l2nat->aging_timeout) {
		/* take extra ref for timer */
		l2nat_pppoe_ses_entry_hold(new_e);
		interval = l2nat->aging_timeout;
		mod_timer(&new_e->timer, jiffies + interval);
	}

	/* release the entry we just took */
	if (old_e)
		l2nat_pppoe_ses_entry_put(old_e);

	l2nat->stats.pppoe_ses_entries_add++;

	return ret;

update_entry:
	atomic_long_set(&old_e->last_pkt_timestamp, jiffies);
	old_e->tx_packets++;
	l2nat_pppoe_ses_entry_put(old_e);
	return ret;
}

/* check for an UNIQ-MAC mapping entry,
 * if it is new - add it to the hash table
 * if it exists - update timestamp
 * if it exists but mac address changed - replace with new entry
 */
int l2nat_pppoe_uniq_entry_check_add(
	struct l2nat_dev *l2nat,
	unsigned char *mac,
	u64 uniq)
{
	int ret = 0;
	struct pppoe_uniq_hash_entry *old_e, *new_e;
	unsigned long interval;

	if (unlikely(!mac))
		return 0;

	if (unlikely(!l2nat->hash_en))
		return 0;

	old_e = l2nat_pppoe_uniq_entry_get(l2nat, uniq);

	/* entry exists and mac did not change */
	if (likely(old_e && !memcmp(old_e->cli_mac, mac, ETH_ALEN)))
		goto update_entry;

	new_e = kzalloc(sizeof(*new_e), GFP_ATOMIC);
	if (!new_e) {
		l2n_err("%s: failed to allocate entry! %pM\n",
			l2nat->dev->name, &mac);
		return -1;
	}

	l2nat_pppoe_uniq_entry_init(l2nat, new_e, uniq, mac);

	spin_lock_bh(&l2nat->lock);

	hash_add_rcu(l2nat->pppoe_uniq_hash, &new_e->hlist, uniq);

	/* mac address changed, replace old entry with new one */
	if (old_e && hash_hashed(&old_e->hlist))
		__l2nat_pppoe_uniq_entry_del(old_e);

	spin_unlock_bh(&l2nat->lock);

	if (l2nat->aging_timeout) {
		/* take extra ref for timer */
		l2nat_pppoe_uniq_entry_hold(new_e);
		interval = l2nat->aging_timeout;
		mod_timer(&new_e->timer, jiffies + interval);
	}

	/* release the entry we just took */
	if (old_e)
		l2nat_pppoe_uniq_entry_put(old_e);

	l2nat->stats.pppoe_disc_entries_add++;

	return ret;

update_entry:
	atomic_long_set(&old_e->last_pkt_timestamp, jiffies);
	old_e->tx_packets++;
	l2nat_pppoe_uniq_entry_put(old_e);
	return ret;
}

/* check for an ip-MAC mapping entry,
 * if it is new - add it to the hash table
 * if it exists - update timestamp
 * if it exists but mac address changed - replace with new entry
 */
int l2nat_entry_check_add(struct l2nat_dev *l2nat, __be32 ip,
			  unsigned char *mac)
{
	int ret = 0;
	struct hash_entry *old_e, *new_e;
	unsigned long interval;

	if (unlikely(!ip))
		return 0;

	if (unlikely(!l2nat->hash_en))
		return 0;

	old_e = l2nat_entry_get(l2nat, ip);

	/* entry exists and mac did not change */
	if (likely(old_e && !memcmp(old_e->mac, mac, ETH_ALEN)))
		goto update_entry;

	new_e = kzalloc(sizeof(*new_e), GFP_ATOMIC);
	if (!new_e) {
		l2n_err("%s: failed to allocate entry! %pI4 %pM\n",
			l2nat->dev->name, &ip, &mac);
		return -1;
	}

	l2nat_entry_init(l2nat, new_e, ip, mac);

	spin_lock_bh(&l2nat->lock);

	hash_add_rcu(l2nat->hash, &new_e->hlist, ntohl(ip));

	/* mac address changed, replace old entry with new one */
	if (old_e && hash_hashed(&old_e->hlist))
		__l2nat_entry_del(old_e);

	spin_unlock_bh(&l2nat->lock);

	l2n_dbg_hash("%s: added entry %pI4 %pM\n", l2nat->dev->name, &ip, mac);

	if (l2nat->aging_timeout) {
		/* take extra ref for timer */
		l2nat_entry_hold(new_e);
		interval = l2nat->aging_timeout -
			   HZ * (L2NAT_AGING_WAIT_INFO + L2NAT_AGING_WAIT_ARP);
		mod_timer(&new_e->timer, jiffies + interval);
	}

	/* release the entry we just took */
	if (old_e)
		l2nat_entry_put(old_e);

	l2nat->stats.entries_add++;

	return ret;

update_entry:
	atomic_long_set(&old_e->last_pkt_timestamp, jiffies);
	old_e->tx_packets++;
	l2nat_entry_put(old_e);
	return ret;
}

/* ip-MAC mapping entry timer timeout handling */
void entry_aging_timer_fn(unsigned long data)
{
	struct hash_entry *ent = (struct hash_entry *)data;
	unsigned long delta, next_time_offset, send_arp_timeout;
	unsigned long get_info_timeout;
	struct l2nat_dev *l2nat = ent->l2nat;

	spin_lock_bh(&l2nat->lock);

	/* entry was removed by someone, release timer reference */
	if (!hash_hashed(&ent->hlist)) {
		l2nat_entry_put(ent);
		goto done;
	}

	delta = last_packet_delta(ent);
	send_arp_timeout = l2nat->aging_timeout - L2NAT_AGING_WAIT_ARP * HZ;
	get_info_timeout = send_arp_timeout - L2NAT_AGING_WAIT_INFO * HZ;

	/* delete entry if its inactive longer than allowed */
	if (delta >= l2nat->aging_timeout) {
		l2n_dbg_tmr("timer deleting entry %pI4 inactive %u sec\n",
			    &ent->ip,
			    jiffies_to_msecs(last_packet_delta(ent)) / 1000);

		__l2nat_entry_del(ent);
		l2nat_entry_put(ent);
		goto done;
	}

	if (delta < get_info_timeout) {
		/* no need for any actions, reschedule on the moment
		 * of next arp attempt, clear waiting flags if any.
		 */
		next_time_offset = get_info_timeout - delta;
		l2n_dbg_tmr("timer entry %pI4 active, offset %lu, delta %lu\n",
			    &ent->ip, next_time_offset, delta);

		goto modify_out;
	}

	if (delta < send_arp_timeout) {
		atomic_set(&l2nat->arp_info_needed, 1);
		next_time_offset = L2NAT_AGING_WAIT_INFO * HZ;
		l2n_dbg_tmr("timer req arp to %pI4, offset %lu, delta %lu\n",
			    &ent->ip, next_time_offset, delta);

		goto modify_out;
	}

	next_time_offset = L2NAT_AGING_WAIT_ARP * HZ;

	/* only send arp if info is not stale */
	if (jiffies - l2nat->last_arp_info_ts <= L2NAT_AGING_WAIT_INFO * HZ) {
		__gen_fake_arp_req(l2nat, l2nat->ip_for_arp, l2nat->mac_for_arp,
				   ent->ip);

		l2n_dbg_tmr("timer sent arp to %pI4 offset %lu\n", &ent->ip,
			    next_time_offset);
	}

modify_out:
	mod_timer(&ent->timer, jiffies + next_time_offset);
done:
	spin_unlock_bh(&l2nat->lock);
}

/* UNIQ-MAC mapping entry timer timeout handling */
void pppoe_uniq_entry_aging_timer_fn(unsigned long d)
{
	struct pppoe_uniq_hash_entry *ent = (struct pppoe_uniq_hash_entry *)d;
	unsigned long delta;
	struct l2nat_dev *l2nat = ent->l2nat;

	spin_lock_bh(&l2nat->lock);

	/* entry was removed by someone, release timer reference */
	if (!hash_hashed(&ent->hlist)) {
		l2nat_pppoe_uniq_entry_put(ent);
		goto done;
	}

	delta = last_pppoe_uniq_packet_delta(ent);

	/* delete entry if its inactive longer than allowed */
	if (delta >= l2nat->aging_timeout) {
		__l2nat_pppoe_uniq_entry_del(ent);
		l2nat_pppoe_uniq_entry_put(ent);
		goto done;
	}
	mod_timer(&ent->timer, jiffies + l2nat->aging_timeout);
done:
	spin_unlock_bh(&l2nat->lock);
}

/* SID-MAC mapping entry timer timeout handling */
void pppoe_ses_entry_aging_timer_fn(unsigned long data)
{
	struct pppoe_sid_hash_entry *ent = (struct pppoe_sid_hash_entry *)data;
	unsigned long delta;
	struct l2nat_dev *l2nat = ent->l2nat;

	spin_lock_bh(&l2nat->lock);

	/* entry was removed by someone, release timer reference */
	if (!hash_hashed(&ent->hlist)) {
		l2nat_pppoe_ses_entry_put(ent);
		goto done;
	}

	delta = last_pppoe_ses_packet_delta(ent);

	/* delete entry if its inactive longer than allowed */
	if (delta >= l2nat->aging_timeout) {
		l2n_dbg_tmr("timer deleting ent SID:0x%04x inactive %u sec\n",
			    ent->sid,
			    jiffies_to_msecs(last_pppoe_ses_packet_delta(ent)) / 1000);

		__l2nat_pppoe_ses_entry_del(ent);
		l2nat_pppoe_ses_entry_put(ent);
		goto done;
	}

	mod_timer(&ent->timer, jiffies + l2nat->aging_timeout);
done:
	spin_unlock_bh(&l2nat->lock);
}
