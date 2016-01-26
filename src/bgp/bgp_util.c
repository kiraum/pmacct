/*  
    pmacct (Promiscuous mode IP Accounting package)
    pmacct is Copyright (C) 2003-2016 by Paolo Lucente
*/

/*
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

/* defines */
#define __BGP_UTIL_C

/* includes */
#include "pmacct.h"
#include "bgp.h"

/* BGP Address Famiy Identifier to UNIX Address Family converter. */
int bgp_afi2family (int afi)
{
  if (afi == AFI_IP)
    return AF_INET;
#ifdef ENABLE_IPV6
  else if (afi == AFI_IP6)
    return AF_INET6;
#endif 
  return SUCCESS;
}

int bgp_rd2str(char *str, rd_t *rd)
{
  struct rd_ip  *rdi;
  struct rd_as  *rda;
  struct rd_as4 *rda4;
  struct host_addr a;
  u_char ip_address[INET6_ADDRSTRLEN];

  switch (rd->type) {
  case RD_TYPE_AS:
    rda = (struct rd_as *) rd;
    sprintf(str, "%u:%u:%u", rda->type, rda->as, rda->val); 
    break;
  case RD_TYPE_IP:
    rdi = (struct rd_ip *) rd;
    a.family = AF_INET;
    a.address.ipv4.s_addr = rdi->ip.s_addr;
    addr_to_str(ip_address, &a);
    sprintf(str, "%u:%s:%u", rdi->type, ip_address, rdi->val); 
    break;
  case RD_TYPE_AS4:
    rda4 = (struct rd_as4 *) rd;
    sprintf(str, "%u:%u:%u", rda4->type, rda4->as, rda4->val); 
    break;
  case RD_TYPE_VRFID:
    rda = (struct rd_as *) rd; 
    sprintf(str, "vrfid:%u", rda->val);
    break;
  default:
    sprintf(str, "unknown");
    break; 
  }

  return TRUE;
}

int bgp_str2rd(rd_t *output, char *value)
{
  struct host_addr a;
  char *endptr, *token;
  u_int32_t tmp32;
  u_int16_t tmp16;
  struct rd_ip  *rdi;
  struct rd_as  *rda;
  struct rd_as4 *rda4;
  int idx = 0;
  rd_t rd;

  memset(&a, 0, sizeof(a));
  memset(&rd, 0, sizeof(rd));

  /* type:RD_subfield1:RD_subfield2 */
  while ( (token = extract_token(&value, ':')) && idx < 3) {
    if (idx == 0) {
      tmp32 = strtoul(token, &endptr, 10);
      rd.type = tmp32;
      switch (rd.type) {
      case RD_TYPE_AS:
        rda = (struct rd_as *) &rd;
        break;
      case RD_TYPE_IP:
        rdi = (struct rd_ip *) &rd;
        break;
      case RD_TYPE_AS4:
        rda4 = (struct rd_as4 *) &rd;
        break;
      default:
        printf("ERROR: Invalid RD type specified\n");
        return FALSE;
      }
    }
    if (idx == 1) {
      switch (rd.type) {
      case RD_TYPE_AS:
        tmp32 = strtoul(token, &endptr, 10);
        rda->as = tmp32;
        break;
      case RD_TYPE_IP:
        memset(&a, 0, sizeof(a));
        str_to_addr(token, &a);
        if (a.family == AF_INET) rdi->ip.s_addr = a.address.ipv4.s_addr;
        break;
      case RD_TYPE_AS4:
        tmp32 = strtoul(token, &endptr, 10);
        rda4->as = tmp32;
        break;
      }
    }
    if (idx == 2) {
      switch (rd.type) {
      case RD_TYPE_AS:
        tmp32 = strtoul(token, &endptr, 10);
        rda->val = tmp32;
        break;
      case RD_TYPE_IP:
        tmp32 = strtoul(token, &endptr, 10);
        rdi->val = tmp32;
        break;
      case RD_TYPE_AS4:
        tmp32 = strtoul(token, &endptr, 10);
        rda4->val = tmp32;
        break;
      }
    }

    idx++;
  }

  memcpy(output, &rd, sizeof(rd));

  return TRUE;
}

/* Allocate bgp_info_extra */
struct bgp_info_extra *bgp_info_extra_new(void)
{
  struct bgp_info_extra *new;

  new = malloc(sizeof(struct bgp_info_extra));
  if (!new) {
    Log(LOG_ERR, "ERROR ( %s/core/BGP ): malloc() failed (bgp_info_extra_new). Exiting ..\n", config.name);
    exit_all(1);
  }
  else memset(new, 0, sizeof (struct bgp_info_extra));

  return new;
}

void bgp_info_extra_free(struct bgp_info_extra **extra)
{
  if (extra && *extra) {
    free(*extra);
    *extra = NULL;
  }
}

/* Get bgp_info extra information for the given bgp_info */
struct bgp_info_extra *bgp_info_extra_get(struct bgp_info *ri)
{
  if (!ri->extra)
    ri->extra = bgp_info_extra_new();

  return ri->extra;
}

/* Allocate new bgp info structure. */
struct bgp_info *bgp_info_new()
{
  struct bgp_info *new;

  new = malloc(sizeof(struct bgp_info));
  if (!new) {
    Log(LOG_ERR, "ERROR ( %s/core/BGP ): malloc() failed (bgp_info_new). Exiting ..\n", config.name);
    exit_all(1);
  }
  else memset(new, 0, sizeof (struct bgp_info));
  
  return new;
}

void bgp_info_add(struct bgp_node *rn, struct bgp_info *ri, u_int32_t modulo)
{
  struct bgp_info *top;

  top = rn->info[modulo];

  ri->next = rn->info[modulo];
  ri->prev = NULL;
  if (top)
    top->prev = ri;
  rn->info[modulo] = ri;

  // ri->lock++;
  bgp_lock_node(rn);
  ri->peer->lock++;
}

void bgp_info_delete(struct bgp_structs *inter_domain_routing_db, struct bgp_node *rn, struct bgp_info *ri, u_int32_t modulo)
{
  if (ri->next)
    ri->next->prev = ri->prev;
  if (ri->prev)
    ri->prev->next = ri->next;
  else
    rn->info[modulo] = ri->next;

  bgp_info_free(inter_domain_routing_db, ri);

  bgp_unlock_node(rn);
}

/* Free bgp route information. */
void bgp_info_free(struct bgp_structs *inter_domain_routing_db, struct bgp_info *ri)
{
  if (ri->attr)
    bgp_attr_unintern(inter_domain_routing_db, ri->attr);

  bgp_info_extra_free(&ri->extra);

  ri->peer->lock--;
  free(ri);
}

/* Initialization of attributes */
void bgp_attr_init(struct bgp_structs *inter_domain_routing_db)
{
  aspath_init(&inter_domain_routing_db->ashash);
  attrhash_init(&inter_domain_routing_db->attrhash);
  community_init(&inter_domain_routing_db->comhash);
  ecommunity_init(&inter_domain_routing_db->ecomhash);
}

unsigned int attrhash_key_make(void *p)
{
  struct bgp_attr *attr = (struct bgp_attr *) p;
  unsigned int key = 0;

  key += attr->origin;
  key += attr->nexthop.s_addr;
  key += attr->med;
  key += attr->local_pref;
  if (attr->pathlimit.as) {
    key += attr->pathlimit.ttl;
    key += attr->pathlimit.as;
  }

  if (attr->aspath)
    key += aspath_key_make(attr->aspath);
  if (attr->community)
    key += community_hash_make(attr->community);
  if (attr->ecommunity)
    key += ecommunity_hash_make(attr->ecommunity);

  /* XXX: add mp_nexthop to key */

  return key;
}

int attrhash_cmp(const void *p1, const void *p2)
{
  const struct bgp_attr *attr1 = (const struct bgp_attr *)p1;
  const struct bgp_attr *attr2 = (const struct bgp_attr *)p2;

  if (attr1->flag == attr2->flag
      && attr1->origin == attr2->origin
      && attr1->nexthop.s_addr == attr2->nexthop.s_addr
      && attr1->aspath == attr2->aspath
      && attr1->community == attr2->community
      && attr1->ecommunity == attr2->ecommunity
      && attr1->med == attr2->med
      && attr1->local_pref == attr2->local_pref
      && attr1->pathlimit.ttl == attr2->pathlimit.ttl
      && attr1->pathlimit.as == attr2->pathlimit.as) {
    if (attr1->mp_nexthop.family == attr2->mp_nexthop.family) {
      if (attr1->mp_nexthop.family == AF_INET
	  && attr1->mp_nexthop.address.ipv4.s_addr == attr2->mp_nexthop.address.ipv4.s_addr) 
        return 1;
#if defined ENABLE_IPV6
      else if (attr1->mp_nexthop.family == AF_INET6
	  && !memcmp(&attr1->mp_nexthop.address.ipv6, &attr2->mp_nexthop.address.ipv6, 16))
        return 1;
#endif
      else return 1;
    }
  }

  return SUCCESS;
}

void attrhash_init(struct hash **loc_attrhash)
{
  (*loc_attrhash) = (struct hash *) hash_create(attrhash_key_make, attrhash_cmp);
}

/* Internet argument attribute. */
struct bgp_attr *bgp_attr_intern(struct bgp_structs *inter_domain_routing_db, struct bgp_attr *attr)
{
  struct bgp_attr *find;
 
  /* Intern referenced strucutre. */
  if (attr->aspath) {
    if (! attr->aspath->refcnt)
      attr->aspath = aspath_intern(inter_domain_routing_db, attr->aspath);
  else
    attr->aspath->refcnt++;
  }
  if (attr->community) {
    if (! attr->community->refcnt)
      attr->community = community_intern(inter_domain_routing_db, attr->community);
    else
      attr->community->refcnt++;
  }
  if (attr->ecommunity) {
    if (!attr->ecommunity->refcnt)
      attr->ecommunity = ecommunity_intern(inter_domain_routing_db, attr->ecommunity);
  else
    attr->ecommunity->refcnt++;
  }
 
  find = (struct bgp_attr *) hash_get(inter_domain_routing_db->attrhash, attr, bgp_attr_hash_alloc);
  find->refcnt++;

  return find;
}

/* Free bgp attribute and aspath. */
void bgp_attr_unintern(struct bgp_structs *inter_domain_routing_db, struct bgp_attr *attr)
{
  struct bgp_attr *ret;
  struct aspath *aspath;
  struct community *community;
  struct ecommunity *ecommunity = NULL;
 
  /* Decrement attribute reference. */
  attr->refcnt--;
  aspath = attr->aspath;
  community = attr->community;
  ecommunity = attr->ecommunity;

  /* If reference becomes zero then free attribute object. */
  if (attr->refcnt == 0) {
    ret = (struct bgp_attr *) hash_release(inter_domain_routing_db->attrhash, attr);
    // assert (ret != NULL);
    if (!ret) Log(LOG_INFO, "INFO ( %s/core/BGP ): bgp_attr_unintern() hash lookup failed.\n", config.name);
    free(attr);
  }

  /* aspath refcount shoud be decrement. */
  if (aspath)
    aspath_unintern(inter_domain_routing_db, aspath);
  if (community)
    community_unintern(inter_domain_routing_db, community);
  if (ecommunity)
    ecommunity_unintern(inter_domain_routing_db, ecommunity);
}

void *bgp_attr_hash_alloc(void *p)
{
  struct bgp_attr *val = (struct bgp_attr *) p;
  struct bgp_attr *attr;

  attr = malloc(sizeof (struct bgp_attr));
  if (!attr) {
    Log(LOG_ERR, "ERROR ( %s/core/BGP ): malloc() failed (bgp_attr_hash_alloc). Exiting ..\n", config.name);
    exit_all(1);
  }
  else {
    memset(attr, 0, sizeof (struct bgp_attr));
    memcpy(attr, val, sizeof (struct bgp_attr));
    attr->refcnt = 0;
  }

  return attr;
}

int bgp_peer_init(struct bgp_peer *peer, int type)
{
  int ret = TRUE;
  afi_t afi;
  safi_t safi;

  memset(peer, 0, sizeof(struct bgp_peer));
  peer->type = type;
  peer->status = Idle;
  peer->buf.len = BGP_BUFFER_SIZE;
  peer->buf.base = malloc(peer->buf.len);
  if (!peer->buf.base) {
    Log(LOG_ERR, "ERROR ( %s/core/BGP ): malloc() failed (bgp_peer_init). Exiting ..\n", config.name);
    exit_all(1);
  }
  else {
    memset(peer->buf.base, 0, peer->buf.len);
    ret = FALSE;
  }

  return ret;
}

void bgp_peer_close(struct bgp_peer *peer, int type)
{
  afi_t afi;
  safi_t safi;

  /* pointers to BGP or BMP vars */
  char *msglog_file, *neighbors_file, *msglog_amqp_routing_key;
  char *msglog_kafka_topic;
  char *dump_file = NULL, *dump_amqp_routing_key = NULL;
  char *dump_kafka_topic = NULL;
  int msglog_output;

  if (type == FUNC_TYPE_BGP) {
    msglog_file = config.nfacctd_bgp_msglog_file;
    msglog_amqp_routing_key = config.nfacctd_bgp_msglog_amqp_routing_key;
    msglog_kafka_topic = config.nfacctd_bgp_msglog_kafka_topic;
    msglog_output = config.nfacctd_bgp_msglog_output;
    neighbors_file = config.nfacctd_bgp_neighbors_file; 
  }
  else if (type == FUNC_TYPE_BMP) {
    msglog_file = config.nfacctd_bmp_msglog_file;
    msglog_amqp_routing_key = config.nfacctd_bmp_msglog_amqp_routing_key;
    msglog_kafka_topic = config.nfacctd_bmp_msglog_kafka_topic;
    msglog_output = config.nfacctd_bmp_msglog_output;
    neighbors_file = config.nfacctd_bmp_neighbors_file;
    dump_file = config.bmp_dump_file;
    dump_amqp_routing_key = config.bmp_dump_amqp_routing_key;
    dump_kafka_topic = config.bmp_dump_kafka_topic;
  }
  else return;

  bgp_peer_info_delete(peer);

  if (msglog_file || msglog_amqp_routing_key || msglog_kafka_topic)
    bgp_peer_log_close(peer, msglog_output, type);

  /* BMP case only */
  if (dump_file || dump_amqp_routing_key || dump_kafka_topic)
    bmp_dump_close_peer(peer);

  close(peer->fd);
  peer->fd = 0;
  memset(&peer->id, 0, sizeof(peer->id));
  memset(&peer->addr, 0, sizeof(peer->addr));
  memset(&peer->addr_str, 0, sizeof(peer->addr_str));

  free(peer->buf.base);

  if (neighbors_file)
    write_neighbors_file(neighbors_file);
}

char *bgp_peer_print(struct bgp_peer *peer)
{
  static __thread char buf[INET6_ADDRSTRLEN], dumb_buf[] = "0.0.0.0";
  int ret = 0;

  if (peer) {
    if (peer->id.family) return inet_ntoa(peer->id.address.ipv4);
    else ret = addr_to_str(buf, &peer->addr);
  }

  if (!ret) strcpy(buf, dumb_buf);

  return buf;
}

void bgp_peer_info_delete(struct bgp_peer *peer)
{
  struct bgp_structs *inter_domain_routing_db = bgp_select_routing_db(peer->type);
  struct bgp_table *table;
  struct bgp_node *node;
  afi_t afi;
  safi_t safi;

  for (afi = AFI_IP; afi < AFI_MAX; afi++) {
    for (safi = SAFI_UNICAST; safi < SAFI_MAX; safi++) {
      table = inter_domain_routing_db->rib[afi][safi];
      node = bgp_table_top(table);

      while (node) {
        u_int32_t modulo = bgp_route_info_modulo(peer, NULL);
        u_int32_t peer_buckets;
        struct bgp_info *ri;
        struct bgp_info *ri_next;

        for (peer_buckets = 0; peer_buckets < config.bgp_table_per_peer_buckets; peer_buckets++) {
          for (ri = node->info[modulo+peer_buckets]; ri; ri = ri_next) {
            if (ri->peer == peer) {
	      if (nfacctd_bgp_msglog_backend_methods) {
		char event_type[] = "log";

		if (peer->type == FUNC_TYPE_BGP) 
		  bgp_peer_log_msg(node, ri, safi, event_type, config.nfacctd_bgp_msglog_output, BGP_LOG_TYPE_DELETE);
		else if (peer->type == FUNC_TYPE_BMP) 
		  bgp_peer_log_msg(node, ri, safi, event_type, config.nfacctd_bmp_msglog_output, BGP_LOG_TYPE_DELETE);
	      }

	      ri_next = ri->next; /* let's save pointer to next before free up */
              bgp_info_delete(inter_domain_routing_db, node, ri, modulo+peer_buckets);
            }
	    else ri_next = ri->next;
          }
        }

        node = bgp_route_next(node);
      }
    }
  }
}

int bgp_attr_munge_as4path(struct bgp_peer *peer, struct bgp_attr *attr, struct aspath *as4path)
{
  struct aspath *newpath;

  /* If the BGP peer supports 32bit AS_PATH then we are done */ 
  if (peer->cap_4as) return SUCCESS;

  /* pre-requisite for AS4_PATH is AS_PATH indeed */ 
  // XXX if (as4path && !attr->aspath) return ERR;

  newpath = aspath_reconcile_as4(attr->aspath, as4path);
  aspath_unintern(bgp_select_routing_db(peer->type), attr->aspath);
  attr->aspath = aspath_intern(bgp_select_routing_db(peer->type), newpath);

  return SUCCESS;
}

void load_comm_patterns(char **stdcomm, char **extcomm, char **stdcomm_to_asn)
{
  int idx;
  char *token;

  memset(std_comm_patterns, 0, sizeof(std_comm_patterns));
  memset(ext_comm_patterns, 0, sizeof(ext_comm_patterns));
  memset(std_comm_patterns_to_asn, 0, sizeof(std_comm_patterns_to_asn));

  if (*stdcomm) {
    idx = 0;
    while ( (token = extract_token(stdcomm, ',')) && idx < MAX_BGP_COMM_PATTERNS ) {
      std_comm_patterns[idx] = token;
      trim_spaces(std_comm_patterns[idx]);
      idx++;
    }
  }
 
  if (*extcomm) {
    idx = 0;
    while ( (token = extract_token(extcomm, ',')) && idx < MAX_BGP_COMM_PATTERNS ) {
      ext_comm_patterns[idx] = token;
      trim_spaces(ext_comm_patterns[idx]);
      idx++;
    }
  }

  if (*stdcomm_to_asn) {
    idx = 0;
    while ( (token = extract_token(stdcomm_to_asn, ',')) && idx < MAX_BGP_COMM_PATTERNS ) {
      std_comm_patterns_to_asn[idx] = token;
      trim_spaces(std_comm_patterns_to_asn[idx]);
      idx++;
    }
  }
} 

void evaluate_comm_patterns(char *dst, char *src, char **patterns, int dstlen)
{
  char *ptr, *haystack, *delim_src, *delim_ptn;
  char local_ptr[MAX_BGP_STD_COMMS], *auxptr;
  int idx, i, j, srclen;

  srclen = strlen(src);

  for (idx = 0, j = 0; patterns[idx]; idx++) {
    haystack = src;

    find_again:
    delim_ptn = strchr(patterns[idx], '.');
    if (delim_ptn) *delim_ptn = '\0';
    ptr = strstr(haystack, patterns[idx]);

    if (ptr && delim_ptn) {
      delim_src = strchr(ptr, ' ');
      if (delim_src) {
	memcpy(local_ptr, ptr, delim_src-ptr);
        local_ptr[delim_src-ptr] = '\0';
      }
      else memcpy(local_ptr, ptr, strlen(ptr)+1);
      *delim_ptn = '.';

      if (strlen(local_ptr) != strlen(patterns[idx])) ptr = NULL;
      else {
	for (auxptr = strchr(patterns[idx], '.'); auxptr; auxptr = strchr(auxptr, '.')) {
	  local_ptr[auxptr-patterns[idx]] = '.';
	  auxptr++;
	} 
	if (strncmp(patterns[idx], local_ptr, strlen(patterns[idx]))) ptr = NULL;
      }
    } 
    else if (delim_ptn) *delim_ptn = '.';

    if (ptr) {
      /* If we have already something on the stack, let's insert a space */
      if (j && j < dstlen) {
	dst[j] = ' ';
	j++;
      }

      /* We should be able to trust this string */
      for (i = 0; ptr[i] != ' ' && ptr[i] != '\0'; i++, j++) {
	if (j < dstlen) dst[j] = ptr[i];
	else break;
      } 

      haystack = &ptr[i];
    }

    /* If we don't have space anymore, let's finish it here */
    if (j >= dstlen) {
      dst[dstlen-1] = '+';
      break;
    }

    /* Trick to find multiple occurrences */ 
    if (ptr) goto find_again;
  }
}

as_t evaluate_last_asn(struct aspath *as)
{
  if (!as) return SUCCESS;

  return as->last_as;
}

as_t evaluate_first_asn(char *src)
{
  int idx, is_space = FALSE, len = strlen(src), start, sub_as, iteration;
  char *endptr, *ptr, saved;
  as_t asn, real_first_asn;

  start = 0;
  iteration = 0;
  real_first_asn = 0;

  start_again:

  asn = 0;
  sub_as = FALSE;

  for (idx = start; idx < len && (src[idx] != ' ' && src[idx] != ')'); idx++);

  /* Mangling the AS_PATH string */
  if (src[idx] == ' ' || src[idx] == ')') {
    is_space = TRUE;  
    saved =  src[idx];
    src[idx] = '\0';
  }

  if (src[start] == '(') {
    ptr = &src[start+1];
    sub_as = TRUE;
  }
  else ptr = &src[start];

  asn = strtoul(ptr, &endptr, 10);

  /* Restoring mangled AS_PATH */
  if (is_space) {
    src[idx] = saved; 
    saved = '\0';
    is_space = FALSE;
  }

  if (config.nfacctd_bgp_peer_as_skip_subas && sub_as) {
    while (idx < len && (src[idx] == ' ' || src[idx] == ')')) idx++;

    if (idx != len-1) { 
      start = idx;
      if (iteration == 0) real_first_asn = asn;
      iteration++;
      goto start_again;
    }
  }

  /* skip sub-as kicks-in only when traffic is delivered to a different ASN */
  if (real_first_asn && (!asn || sub_as)) asn = real_first_asn;

  return asn;
}

void write_neighbors_file(char *filename)
{
  FILE *file;
  char neighbor[INET6_ADDRSTRLEN+1];
  int idx, len, ret;
  uid_t owner = -1;
  gid_t group = -1;

  unlink(filename);

  if (config.files_uid) owner = config.files_uid; 
  if (config.files_gid) group = config.files_gid; 

  file = fopen(filename,"w");
  if (file) {
    if ((ret = chown(filename, owner, group)) == -1)
      Log(LOG_WARNING, "WARN: Unable to chown() '%s': %s\n", filename, strerror(errno));

    if (file_lock(fileno(file))) {
      Log(LOG_ALERT, "ALERT: Unable to obtain lock for bgp_neighbors_file '%s'.\n", filename);
      return;
    }
    for (idx = 0; idx < config.nfacctd_bgp_max_peers; idx++) {
      if (peers[idx].fd) {
        if (peers[idx].addr.family == AF_INET) {
          inet_ntop(AF_INET, &peers[idx].addr.address.ipv4, neighbor, INET6_ADDRSTRLEN);
	  len = strlen(neighbor);
	  neighbor[len] = '\n'; len++;
	  neighbor[len] = '\0';
          fwrite(neighbor, len, 1, file);
        }
#if defined ENABLE_IPV6
	else if (peers[idx].addr.family == AF_INET6) {
          inet_ntop(AF_INET6, &peers[idx].addr.address.ipv6, neighbor, INET6_ADDRSTRLEN);
          len = strlen(neighbor);
          neighbor[len] = '\n'; len++;
          neighbor[len] = '\0';
          fwrite(neighbor, len, 1, file);
        }
#endif
      }
    }

    file_unlock(fileno(file));
    fclose(file);
  }
  else {
    Log(LOG_ERR, "ERROR: Unable to open bgp_neighbors_file '%s'\n", filename);
    return;
  }
}

void bgp_config_checks(struct configuration *c)
{
  if (c->what_to_count & (COUNT_STD_COMM|COUNT_EXT_COMM|COUNT_LOCAL_PREF|COUNT_MED|COUNT_AS_PATH|
			  COUNT_PEER_SRC_AS|COUNT_PEER_DST_AS|COUNT_PEER_SRC_IP|COUNT_PEER_DST_IP|
			  COUNT_SRC_STD_COMM|COUNT_SRC_EXT_COMM|COUNT_SRC_AS_PATH|COUNT_SRC_MED|
			  COUNT_SRC_LOCAL_PREF|COUNT_MPLS_VPN_RD)) {
    /* Sanitizing the aggregation method */
    if ( ((c->what_to_count & COUNT_STD_COMM) && (c->what_to_count & COUNT_EXT_COMM)) ||
         ((c->what_to_count & COUNT_SRC_STD_COMM) && (c->what_to_count & COUNT_SRC_EXT_COMM)) ) {
      printf("ERROR: The use of STANDARD and EXTENDED BGP communitities is mutual exclusive.\n");
      exit(1);
    }
    if ( (c->what_to_count & COUNT_SRC_STD_COMM && !c->nfacctd_bgp_src_std_comm_type) ||
	 (c->what_to_count & COUNT_SRC_EXT_COMM && !c->nfacctd_bgp_src_ext_comm_type) ||
	 (c->what_to_count & COUNT_SRC_AS_PATH && !c->nfacctd_bgp_src_as_path_type ) ||
	 (c->what_to_count & COUNT_SRC_LOCAL_PREF && !c->nfacctd_bgp_src_local_pref_type ) ||
	 (c->what_to_count & COUNT_SRC_MED && !c->nfacctd_bgp_src_med_type ) ||
	 (c->what_to_count & COUNT_PEER_SRC_AS && !c->nfacctd_bgp_peer_as_src_type &&
	  (config.acct_type != ACCT_SF && config.acct_type != ACCT_NF)) ) {
      printf("ERROR: At least one of the following primitives is in use but its source type is not specified:\n");
      printf("       peer_src_as     =>  bgp_peer_src_as_type\n");
      printf("       src_as_path     =>  bgp_src_as_path_type\n");
      printf("       src_std_comm    =>  bgp_src_std_comm_type\n");
      printf("       src_ext_comm    =>  bgp_src_ext_comm_type\n");
      printf("       src_local_pref  =>  bgp_src_local_pref_type\n");
      printf("       src_med         =>  bgp_src_med_type\n");
      exit(1);
    }
    c->data_type |= PIPE_TYPE_BGP;
  }
}

void process_bgp_md5_file(int sock, struct bgp_md5_table *bgp_md5)
{
  struct my_tcp_md5sig md5sig;
  struct sockaddr_storage ss_md5sig;
  int rc, keylen, idx = 0, ss_md5sig_len;

  while (idx < bgp_md5->num) {
    memset(&md5sig, 0, sizeof(md5sig));
    memset(&ss_md5sig, 0, sizeof(ss_md5sig));

    ss_md5sig_len = addr_to_sa((struct sockaddr *)&ss_md5sig, &bgp_md5->table[idx].addr, 0);
    memcpy(&md5sig.tcpm_addr, &ss_md5sig, ss_md5sig_len);

    keylen = strlen(bgp_md5->table[idx].key);
    if (keylen) {
      md5sig.tcpm_keylen = keylen;
      memcpy(md5sig.tcpm_key, &bgp_md5->table[idx].key, keylen);
    }

    rc = setsockopt(sock, IPPROTO_TCP, TCP_MD5SIG, &md5sig, sizeof(md5sig));
    if (rc < 0) Log(LOG_ERR, "WARN ( %s/core/BGP ): setsockopt() failed for TCP_MD5SIG (errno: %d).\n", config.name, errno);

    idx++;
  }
}

void bgp_batch_init(struct bgp_peer_batch *bp_batch, int num, int interval)
{
  if (bp_batch) {
    memset(bp_batch, 0, sizeof(struct bgp_peer_batch));

    bp_batch->num = num;
    bp_batch->interval = interval;
  }
}

void bgp_batch_reset(struct bgp_peer_batch *bp_batch, time_t now)
{
  if (bp_batch) {
    bp_batch->num_current = bp_batch->num;
    bp_batch->base_stamp = now;
  }
}

int bgp_batch_is_admitted(struct bgp_peer_batch *bp_batch, time_t now)
{
  if (bp_batch) {
    /* bgp_batch_is_not_empty() maybe replaced by a linear
       distribution of the peers over the time interval */
    if (bgp_batch_is_not_empty(bp_batch) || bgp_batch_is_expired(bp_batch, now)) return TRUE;
    else return FALSE;
  }
  else return ERR;
}

int bgp_batch_is_enabled(struct bgp_peer_batch *bp_batch)
{
  if (bp_batch) {
    if (bp_batch->num) return TRUE;
    else return FALSE;
  }
  else return ERR;
}

int bgp_batch_is_expired(struct bgp_peer_batch *bp_batch, time_t now)
{
  if (bp_batch) {
    if (now > (bp_batch->base_stamp + bp_batch->interval)) return TRUE;
    else return FALSE;
  }
  else return ERR;
}

int bgp_batch_is_not_empty(struct bgp_peer_batch *bp_batch)
{
  if (bp_batch) {
    if (bp_batch->num_current) return TRUE;
    else return FALSE;
  }
  else return ERR;
}

void bgp_batch_increase_counter(struct bgp_peer_batch *bp_batch)
{
  if (bp_batch) bp_batch->num_current++;
}

void bgp_batch_decrease_counter(struct bgp_peer_batch *bp_batch)
{
  if (bp_batch) bp_batch->num_current--;
}

void bgp_batch_rollback(struct bgp_peer_batch *bp_batch)
{
  if (bp_batch && bgp_batch_is_enabled(bp_batch)) {
    bgp_batch_increase_counter(bp_batch);
    if (bp_batch->num_current == bp_batch->num)
      bgp_batch_init(bp_batch, config.nfacctd_bgp_batch, config.nfacctd_bgp_batch_interval); 
  }
}

struct bgp_structs *bgp_select_routing_db(int peer_type)
{
  if (peer_type < FUNC_TYPE_MAX) 
    return &inter_domain_routing_dbs[peer_type];

  return NULL;
}
