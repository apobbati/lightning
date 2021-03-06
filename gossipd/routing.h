#ifndef LIGHTNING_LIGHTNINGD_GOSSIP_ROUTING_H
#define LIGHTNING_LIGHTNINGD_GOSSIP_ROUTING_H
#include "config.h"
#include <bitcoin/pubkey.h>
#include <ccan/htable/htable_type.h>
#include <gossipd/broadcast.h>
#include <wire/wire.h>

#define ROUTING_MAX_HOPS 20
#define ROUTING_FLAGS_DISABLED 2

struct node_connection {
	struct node *src, *dst;
	/* millisatoshi. */
	u32 base_fee;
	/* millionths */
	u32 proportional_fee;

	/* Delay for HTLC in blocks.*/
	u32 delay;

	/* Is this connection active? */
	bool active;

	s64 last_timestamp;

	/* Minimum number of msatoshi in an HTLC */
	u32 htlc_minimum_msat;

	/* The channel ID, as determined by the anchor transaction */
	struct short_channel_id short_channel_id;

	/* Flags as specified by the `channel_update`s, among other
	 * things indicated direction wrt the `channel_id` */
	u16 flags;

	/* Cached `channel_announcement` and `channel_update` we might forward to new peers*/
	u8 *channel_announcement;
	u8 *channel_update;
};

struct node {
	struct pubkey id;

	/* -1 means never; other fields undefined */
	s64 last_timestamp;

	/* IP/Hostname and port of this node (may be NULL) */
	struct wireaddr *addresses;

	/* Routes connecting to us, from us. */
	struct node_connection **in, **out;

	/* Temporary data for routefinding. */
	struct {
		/* Total to get to here from target. */
		u64 total;
		/* Total risk premium of this route. */
		u64 risk;
		/* Where that came from. */
		struct node_connection *prev;
	} bfg[ROUTING_MAX_HOPS+1];

	/* UTF-8 encoded alias as tal_arr, not zero terminated */
	u8 *alias;

	/* Color to be used when displaying the name */
	u8 rgb_color[3];

	/* Cached `node_announcement` we might forward to new peers. */
	u8 *node_announcement;
};

const secp256k1_pubkey *node_map_keyof_node(const struct node *n);
size_t node_map_hash_key(const secp256k1_pubkey *key);
bool node_map_node_eq(const struct node *n, const secp256k1_pubkey *key);
HTABLE_DEFINE_TYPE(struct node, node_map_keyof_node, node_map_hash_key, node_map_node_eq, node_map);

struct routing_state {
	/* All known nodes. */
	struct node_map *nodes;

	struct broadcast_state *broadcasts;

	struct sha256_double chain_hash;

	/* Our own ID so we can identify local channels */
	struct pubkey local_id;
};

struct route_hop {
	struct short_channel_id channel_id;
	struct pubkey nodeid;
	u32 amount;
	u32 delay;
};

struct routing_state *new_routing_state(const tal_t *ctx,
					const struct sha256_double *chain_hash,
					const struct pubkey *local_id);

/* Given a short_channel_id, retrieve the matching connection, or NULL if it is
 * unknown. */
struct node_connection *get_connection_by_scid(const struct routing_state *rstate,
					       const struct short_channel_id *schanid,
					      const u8 direction);

/* Handlers for incoming messages */

/**
 * handle_channel_announcement -- Add channel announcement to state
 *
 * Returns true if the channel was fully signed and is local. This
 * means that if we haven't sent a node_announcement just yet, now
 * would be a good time.
 */
bool handle_channel_announcement(struct routing_state *rstate, const u8 *announce);
void handle_channel_update(struct routing_state *rstate, const u8 *update);
void handle_node_announcement(struct routing_state *rstate, const u8 *node);

/* Compute a route to a destination, for a given amount and riskfactor. */
struct route_hop *get_route(tal_t *ctx, struct routing_state *rstate,
			    const struct pubkey *source,
			    const struct pubkey *destination,
			    const u32 msatoshi, double riskfactor,
			    u32 final_cltv);

/* Utility function that, given a source and a destination, gives us
 * the direction bit the matching channel should get */
#define get_channel_direction(from, to) (pubkey_cmp(from, to) > 0)

#endif /* LIGHTNING_LIGHTNINGD_GOSSIP_ROUTING_H */
