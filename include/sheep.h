/*
 * Copyright (C) 2009-2011 Nippon Telegraph and Telephone Corporation.
 * Copyright (C) 2012-2013 Taobao Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __SHEEP_H__
#define __SHEEP_H__

#include <stdint.h>
#include "internal_proto.h"
#include "util.h"
#include "bitops.h"
#include "list.h"
#include "net.h"
#include "rbtree.h"

struct sd_vnode {
	struct rb_node rb;
	const struct sd_node *node;
	uint64_t hash;
};

struct vnode_info {
	struct rb_root vroot;
	struct rb_root nroot;
	int nr_nodes;
	int nr_zones;
	refcnt_t refcnt;
};

/* added for pvce */
const static uint8_t master_ip[4]={172,17,2,109};
struct sd_vnode ** sd_master_vnode;
struct sd_node ** sd_master_node;

static inline void sd_init_req(struct sd_req *req, uint8_t opcode)
{
	memset(req, 0, sizeof(*req));
	req->opcode = opcode;
	req->proto_ver = opcode < 0x80 ? SD_PROTO_VER : SD_SHEEP_PROTO_VER;
}

static inline int same_zone(const struct sd_vnode *v1,
			    const struct sd_vnode *v2)
{
	return v1->node->zone == v2->node->zone;
}

static inline int vnode_cmp(const struct sd_vnode *node1,
			    const struct sd_vnode *node2)
{
	return intcmp(node1->hash, node2->hash);
}

/* If v1_hash < oid_hash <= v2_hash, then oid is resident on v2 */
static inline struct sd_vnode *
oid_to_first_vnode(uint64_t oid, struct rb_root *root)
{
	struct sd_vnode dummy = {
		.hash = sd_hash_oid(oid),
	};
	return rb_nsearch(root, &dummy, rb, vnode_cmp);
}

/* Replica are placed along the ring one by one with different zones */
static inline void oid_to_vnodes(uint64_t oid, struct rb_root *root,
				 int nr_copies,
				 const struct sd_vnode **vnodes)
{
	vnodes[0] = *sd_master_vnode;
		
	if (nr_copies > 1) {
		const struct sd_vnode *next = oid_to_first_vnode(oid, root);
		vnodes[1] = next;
		for (int i = 2; i < nr_copies; i++) {
next:
			next = rb_entry(rb_next(&next->rb), struct sd_vnode, rb);
			if (!next) /* Wrap around */
				next = rb_entry(rb_first(root), struct sd_vnode, rb);
			if (unlikely(next == vnodes[1]))
				panic("can't find a valid vnode");
			for (int j = 0; j < i; j++)
				if (same_zone(vnodes[j], next))
					goto next;
			vnodes[i] = next;
		}
	}
}

static inline const struct sd_vnode *
oid_to_vnode(uint64_t oid, struct rb_root *root, int copy_idx)
{
	const struct sd_vnode *vnodes[SD_MAX_COPIES];

	oid_to_vnodes(oid, root, copy_idx + 1, vnodes);

	return vnodes[copy_idx];
}

static inline const struct sd_node *
oid_to_node(uint64_t oid, struct rb_root *root, int copy_idx)
{
	const struct sd_vnode *vnode;

	vnode = oid_to_vnode(oid, root, copy_idx);

	return vnode->node;
}

static inline void oid_to_nodes(uint64_t oid, struct rb_root *root,
				int nr_copies,
				const struct sd_node **nodes)
{
	const struct sd_vnode *vnodes[SD_MAX_COPIES];

	oid_to_vnodes(oid, root, nr_copies, vnodes);
	for (int i = 0; i < nr_copies; i++)
		nodes[i] = vnodes[i]->node;
}

static inline const char *sd_strerror(int err)
{
	static const char *descs[256] = {
		/* from sheepdog_proto.h */
		[SD_RES_SUCCESS] = "Success",
		[SD_RES_UNKNOWN] = "Unknown error",
		[SD_RES_NO_OBJ] = "No object found",
		[SD_RES_EIO] = "I/O error",
		[SD_RES_VDI_EXIST] = "VDI exists already",
		[SD_RES_INVALID_PARMS] = "Invalid parameters",
		[SD_RES_SYSTEM_ERROR] = "System error",
		[SD_RES_VDI_LOCKED] = "VDI is already locked",
		[SD_RES_NO_VDI] = "No VDI found",
		[SD_RES_NO_BASE_VDI] = "No base VDI found",
		[SD_RES_VDI_READ] = "Failed to read from requested VDI",
		[SD_RES_VDI_WRITE] = "Failed to write to requested VDI",
		[SD_RES_BASE_VDI_READ] = "Failed to read from base VDI",
		[SD_RES_BASE_VDI_WRITE] = "Failed to write to base VDI",
		[SD_RES_NO_TAG] = "Failed to find requested tag",
		[SD_RES_STARTUP] = "System is still booting",
		[SD_RES_VDI_NOT_LOCKED] = "VDI is not locked",
		[SD_RES_SHUTDOWN] = "System is shutting down",
		[SD_RES_NO_MEM] = "Out of memory on server",
		[SD_RES_FULL_VDI] = "Maximum number of VDIs reached",
		[SD_RES_VER_MISMATCH] = "Protocol version mismatch",
		[SD_RES_NO_SPACE] = "Server has no space for new objects",
		[SD_RES_WAIT_FOR_FORMAT] = "Waiting for cluster to be formatted",
		[SD_RES_WAIT_FOR_JOIN] = "Waiting for other nodes to join cluster",
		[SD_RES_JOIN_FAILED] = "Node has failed to join cluster",
		[SD_RES_HALT] =
			"IO has halted as there are not enough living nodes",
		[SD_RES_READONLY] = "Object is read-only",

		/* from internal_proto.h */
		[SD_RES_OLD_NODE_VER] = "Request has an old epoch",
		[SD_RES_NEW_NODE_VER] = "Request has a new epoch",
		[SD_RES_NOT_FORMATTED] = "Cluster has not been formatted",
		[SD_RES_INVALID_CTIME] = "Creation times differ",
		[SD_RES_INVALID_EPOCH] = "Invalid epoch",
		[SD_RES_NETWORK_ERROR] = "Network error between sheep",
		[SD_RES_NO_CACHE] = "No cache object found",
		[SD_RES_BUFFER_SMALL] = "The buffer is too small",
		[SD_RES_FORCE_RECOVER] = "Cluster is running/halted and cannot be force recovered",
		[SD_RES_NO_STORE] = "Targeted backend store is not found",
		[SD_RES_NO_SUPPORT] = "Operation is not supported",
		[SD_RES_NODE_IN_RECOVERY] = "Targeted node is in recovery",
		[SD_RES_KILLED] = "Node is killed",
		[SD_RES_OID_EXIST] = "Object ID exists already",
		[SD_RES_AGAIN] = "Ask to try again",
		[SD_RES_STALE_OBJ] = "Object may be stale",
		[SD_RES_CLUSTER_ERROR] = "Cluster driver error",
	};

	if (!(0 <= err && err < ARRAY_SIZE(descs)) || descs[err] == NULL) {
		static __thread char msg[32];
		snprintf(msg, sizeof(msg), "Invalid error code %x", err);
		return msg;
	}

	return descs[err];
}

static inline int oid_cmp(const uint64_t *oid1, const uint64_t *oid2)
{
	return intcmp(*oid1, *oid2);
}

static inline int node_id_cmp(const struct node_id *node1,
			      const struct node_id *node2)
{
	int cmp = memcmp(node1->addr, node2->addr, sizeof(node1->addr));
	if (cmp != 0)
		return cmp;

	return intcmp(node1->port, node2->port);
}

static inline int node_cmp(const struct sd_node *node1,
			   const struct sd_node *node2)
{
	return node_id_cmp(&node1->nid, &node2->nid);
}

static inline bool node_eq(const struct sd_node *a, const struct sd_node *b)
{
	return node_cmp(a, b) == 0;
}


static inline bool node_ismaster(const struct sd_node *a)
{
	int ip_offset = 12;
	for (int i = 0; i < 4; i++) {
		if ((&a->nid)->addr[i + ip_offset] != master_ip[i]) 
			return false;
	}
	return true;
}
static struct rb_node void_rb_node;

static inline void
node_to_vnodes(struct sd_node *n, struct rb_root *vroot)
{
	if (node_ismaster(n)) {
		n->nr_vnodes = 1;
		sd_master_node = &n;
		struct sd_vnode *v = xmalloc(sizeof(*v));
		v->hash = 0;
		v->node = n;
		v->rb = void_rb_node;
		if (unlikely(rb_insert(vroot, v, rb, vnode_cmp)))
			panic("vdisk hash collison");
		sd_master_vnode = &v;
		return;
	}

	uint64_t hval = sd_hash(&n->nid, offsetof(typeof(n->nid),
						  io_addr));

	for (int i = 0; i < n->nr_vnodes; i++) {
		struct sd_vnode *v = xmalloc(sizeof(*v));

		hval = sd_hash_next(hval);
		v->hash = hval;
		v->node = n;
		if (unlikely(rb_insert(vroot, v, rb, vnode_cmp)))
			panic("vdisk hash collison");
	}
}

static inline void
nodes_to_vnodes(struct rb_root *nroot, struct rb_root *vroot)
{
	struct sd_node *n;

	rb_for_each_entry(n, nroot, rb)
		node_to_vnodes(n, vroot);
}

static inline void nodes_to_buffer(struct rb_root *nroot, void *buffer)
{
	struct sd_node *n, *buf = buffer;

	rb_for_each_entry(n, nroot, rb) {
		memcpy(buf++, n, sizeof(*n));
	}
}

#define MAX_NODE_STR_LEN 256

static inline const char *node_to_str(const struct sd_node *id)
{
	static __thread char str[MAX_NODE_STR_LEN];
	int af = AF_INET6;
	const uint8_t *addr = id->nid.addr;

	/* Find address family type */
	if (addr[12]) {
		int  oct_no = 0;
		while (!addr[oct_no] && oct_no++ < 12)
			;
		if (oct_no == 12)
			af = AF_INET;
	}

	snprintf(str, sizeof(str), "%s ip:%s port:%d",
		(af == AF_INET) ? "IPv4" : "IPv6",
		addr_to_str(id->nid.addr, 0), id->nid.port);

	return str;
}

static inline struct sd_node *str_to_node(const char *str, struct sd_node *id)
{
	int port;
	char v[8], ip[MAX_NODE_STR_LEN];

	sscanf(str, "%s ip:%s port:%d", v, ip, &port);
	id->nid.port = port;
	if (!str_to_addr(ip, id->nid.addr))
		return NULL;

	return id;
}

#endif
