//
// virtio device definitions.
// for both the pci interface, and virtio descriptors.
// only tested with qemu.
//
// the virtio spec:
// https://docs.oasis-open.org/virtio/virtio/v1.1/virtio-v1.1.pdf
//

// a virtio pci device's registers are in its memory BARs,
// in regions that its vendor-specific PCI capabilities
// point to. offsets of a capability's fields:
#define VIRTIO_PCI_CAP_ID         0x09 // vendor-specific capability ID
#define VIRTIO_PCI_CAP_CFG_TYPE   3    // 8 bits: which region (below)
#define VIRTIO_PCI_CAP_BAR        4    // 8 bits: BAR it is in
#define VIRTIO_PCI_CAP_OFFSET     8    // 32 bits: offset in that BAR
#define VIRTIO_PCI_CAP_NOTIFY_MUL 16   // 32 bits: notify region only

// cfg_type: the regions.
#define VIRTIO_PCI_CAP_COMMON_CFG 1 // common configuration
#define VIRTIO_PCI_CAP_NOTIFY_CFG 2 // queue notifications
#define VIRTIO_PCI_CAP_ISR_CFG    3 // interrupt status
#define VIRTIO_PCI_CAP_DEVICE_CFG 4 // device-specific configuration

// common configuration registers, as offsets in the common
// configuration region, from qemu virtio_pci.h. each must be
// accessed with its own width; a 64-bit address is written
// as two 32-bit halves, low then high.
// clang-format off
#define VIRTIO_PCI_DEVICE_FEATURE_SEL	0x00 // 32: select which 32 feature bits
#define VIRTIO_PCI_DEVICE_FEATURE	0x04 // 32: device's features, read-only
#define VIRTIO_PCI_DRIVER_FEATURE_SEL	0x08 // 32
#define VIRTIO_PCI_DRIVER_FEATURE	0x0c // 32
#define VIRTIO_PCI_STATUS		0x14 // 8: read/write
#define VIRTIO_PCI_QUEUE_SEL		0x16 // 16: select queue
#define VIRTIO_PCI_QUEUE_SIZE		0x18 // 16: max size when read, then set size
#define VIRTIO_PCI_QUEUE_ENABLE		0x1c // 16: ready bit
#define VIRTIO_PCI_QUEUE_NOTIFY_OFF	0x1e // 16: where to notify, read-only
#define VIRTIO_PCI_QUEUE_DESC_LOW	0x20 // 32: physical address for descriptor table
#define VIRTIO_PCI_QUEUE_DESC_HIGH	0x24
#define VIRTIO_PCI_DRIVER_DESC_LOW	0x28 // 32: physical address for available ring
#define VIRTIO_PCI_DRIVER_DESC_HIGH	0x2c
#define VIRTIO_PCI_DEVICE_DESC_LOW	0x30 // 32: physical address for used ring
#define VIRTIO_PCI_DEVICE_DESC_HIGH	0x34
// clang-format on

// status register bits, from qemu virtio_config.h
#define VIRTIO_CONFIG_S_ACKNOWLEDGE 1
#define VIRTIO_CONFIG_S_DRIVER      2
#define VIRTIO_CONFIG_S_DRIVER_OK   4
#define VIRTIO_CONFIG_S_FEATURES_OK 8

// device feature bits
#define VIRTIO_BLK_F_RO             5  /* Disk is read-only */
#define VIRTIO_BLK_F_SCSI           7  /* Supports scsi command passthru */
#define VIRTIO_BLK_F_FLUSH          9  /* Cache flush command supported */
#define VIRTIO_BLK_F_CONFIG_WCE     11 /* Writeback mode available in config */
#define VIRTIO_BLK_F_MQ             12 /* support more than one vq */
#define VIRTIO_F_ANY_LAYOUT         27
#define VIRTIO_RING_F_INDIRECT_DESC 28
#define VIRTIO_RING_F_EVENT_IDX     29
#define VIRTIO_F_VERSION_1          32 /* not a legacy device */

// this many virtio descriptors.
// must be a power of two.
#define NUM 8

// a single descriptor, from the spec.
struct virtq_desc {
  uint64 addr;
  uint32 len;
  uint16 flags;
  uint16 next;
};
#define VRING_DESC_F_NEXT  1 // chained with another descriptor
#define VRING_DESC_F_WRITE 2 // device writes (vs read)

// the (entire) avail ring, from the spec.
struct virtq_avail {
  uint16 flags;     // always zero
  uint16 idx;       // driver will write ring[idx] next
  uint16 ring[NUM]; // descriptor numbers of chain heads
  uint16 unused;
};

// one entry in the "used" ring, with which the
// device tells the driver about completed requests.
struct virtq_used_elem {
  uint32 id; // index of start of completed descriptor chain
  uint32 len;
};

struct virtq_used {
  uint16 flags; // always zero
  uint16 idx;   // device increments when it adds a ring[] entry
  struct virtq_used_elem ring[NUM];
};

// these are specific to virtio block devices, e.g. disks,
// described in Section 5.2 of the spec.

#define VIRTIO_BLK_T_IN  0 // read the disk
#define VIRTIO_BLK_T_OUT 1 // write the disk

// the format of the first descriptor in a disk request.
// to be followed by two more descriptors containing
// the block, and a one-byte status.
struct virtio_blk_req {
  uint32 type; // VIRTIO_BLK_T_IN or ..._OUT
  uint32 reserved;
  uint64 sector;
};
