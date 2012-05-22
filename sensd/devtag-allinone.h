#ifndef DEVTAG_H
#define DEVTAG_H

#include <sys/types.h>

/*
 * Lookup 'devtag' among the predefined device selections.
 * If a matching device is found return the name of this device in buf.
 *
 * If 'devtag' is prefixed with "/dev/" then the result is also
 * prefixed with "/dev/".
 *
 * Returns number of devices found or a negative value if an error occured.
 */
int devtag_lookup(char *buf, size_t bufsize, const char *devtag);

/* also returns a constant name if supplied in the configuration */
int devtag_lookup2(char *buf, size_t bufsize, char *constbuf, size_t constsize, const char *devtag);

/*
 * Simple lookup.
 * Returns a suitable replacement for devtag.
 * If no replacement can be determined a 'devtag' will be returned as a duplicated string.
 */
char *devtag_get(const char *devtag);

#endif
#ifndef LIBDEVTAG_H
#define LIBDEVTAG_H

#define LIBDEVTAG_VERSION "1.0.5"

struct devname;
struct devname_head {
  struct devname *head;
};

struct devname {
  char *dev;
  char *devname;
  char *type;
  int pos;
  struct devname *next;
};

struct devinfo;
struct devinfo_head {
  struct devinfo *head;
};

struct devinfo {
  const char *name;
  const char *value;
  struct devinfo *next;
};

struct dev;
struct dev_head {
  struct dev *head;
};

struct dev {
  struct devinfo_head info;
  struct devname_head devnames;
  char *class; /* "usb", "pci" etc */
  struct dev *next;
};

/*
 * Find devices matching selections in list 'sel' (of type struct dev_info).
 * Put matching devices in result list 'result'.
 */
int devtag_dev_scan(struct dev_head *result, const struct devinfo_head *sel);

/*
 * Find USB devices matching selections in list 'sel' (of type struct dev_info).
 * Put matching USB devices in result list 'result'.
 */
int devtag_usb_scan(struct dev_head *result, const struct devinfo_head *sel);


#endif
