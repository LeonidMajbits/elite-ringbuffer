/** @file elite_topology.h
 * Read-only OS-visible topology, not a physical-hardware attestation or a
 * scheduler. Discovery allocates only at setup; no ring data path depends on it.
 * Missing/malformed observations retain errno/status. Explicit limits bound
 * hostile sysfs/fixture inputs; a limit makes the result PARTIAL, never complete.
 */
#ifndef ELITE_TOPOLOGY_H
#define ELITE_TOPOLOGY_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#define ELITE_TOPO_MAX_CPUS 1024u
#define ELITE_TOPO_MAX_CACHES 16u
#define ELITE_TOPO_MAX_NODES 256u
#define ELITE_TOPO_MAX_LEVELS 16u
#define ELITE_TOPO_MAX_CGROUPS 64u
#define TOPOLOGY_CLUSTER_UNKNOWN 0u
#define TOPOLOGY_CLUSTER_PERF 1u
#define TOPOLOGY_CLUSTER_EFFICIENCY 2u
#define TOPOLOGY_CLUSTER_MIXED 3u
struct elite_topo_value { uint64_t value; int error; bool available; };
struct elite_topo_set { unsigned char bits[ELITE_TOPO_MAX_CPUS]; size_t count; int error; };
struct elite_topo_cache {
    unsigned index;
    char type[24];
    struct elite_topo_value level, bytes, line_bytes, id;
    struct elite_topo_set shared_cpus;
};
struct elite_topo_cpu {
    unsigned cpu_id;
    int numa_node;
    struct elite_topo_value package_id, die_id, core_id, cluster_id;
    struct elite_topo_set smt_siblings;
    size_t cache_count;
    struct elite_topo_cache caches[ELITE_TOPO_MAX_CACHES];
};
struct elite_topo_node { unsigned node_id; struct elite_topo_set cpus; };
struct elite_topo_perflevel {
    unsigned index, cluster_class;
    char name[64]; int name_error;
    struct elite_topo_value physical, logical, l1d_bytes, l2_bytes, l3_bytes, cpus_per_l2;
};
struct elite_topo_cgroup {
    char path[1024];
    struct elite_topo_value quota_us, period_us, memory_max, memory_current;
    struct elite_topo_set cpus_effective, mems_effective;
    bool quota_unlimited, memory_unlimited;
};
struct elite_hardware_topology {
    char platform[64], architecture[64], kernel[256], model[256];
    char source_kind[32], environment[48], cgroup_status[64];
    struct elite_topo_value page_bytes, physical_memory_bytes, physical_cpus, logical_cpus;
    struct elite_topo_value memory_frequency_hz, memory_channels;
    struct elite_topo_value system_l3_bytes, cache_line_bytes;
    struct elite_topo_set online_cpus, allowed_cpus;
    bool container_marker, hypervisor_flag, translated, partial;
    int translation_error;
    uint32_t host_cluster_class;
    unsigned observation_errors;
    size_t cpu_count, node_count, perflevel_count, cgroup_count;
    struct elite_topo_cpu *cpus;
    struct elite_topo_node *nodes;
    struct elite_topo_perflevel perflevels[ELITE_TOPO_MAX_LEVELS];
    struct elite_topo_cgroup *cgroups;
};
/* A numeric CPU list is sparse, sorted by serialization, never assumed 0..N-1. */
int elite_topology_parse_cpulist(const char *text,struct elite_topo_set *set);
int elite_topology_parse_size(const char *text,uint64_t *bytes);
int elite_topology_discover(struct elite_hardware_topology **out);
/* Tests only: rooted proc/sys fixture, no OS reads or synthetic performance. */
int elite_topology_fixture(const char *root,struct elite_hardware_topology **out);
int elite_topology_json(FILE *file,const struct elite_hardware_topology *topology);
void elite_topology_free(struct elite_hardware_topology *topology);
const char *elite_topology_cluster_name(unsigned value);
#endif
