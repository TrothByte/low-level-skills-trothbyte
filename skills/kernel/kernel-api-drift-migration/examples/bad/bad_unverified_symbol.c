/*
 * BAD: // intentionally incorrect — unverified GPL-only symbol on a 6.9+ target.
 * Assumes drm_fb_helper-like symbol availability without checking the export
 * state for the pinned kernel version. The module compiles, but if the symbol
 * is GPL-only and the module license is not GPL-compatible, or the symbol was
 * unexported, the load fails or the feature silently disappears.
 */
#include <linux/module.h>
#include <linux/kernel.h>

extern int fbdev_setup_api(struct device *dev);

static int __init bad_fb_init(void)
{
	return fbdev_setup_api(NULL);
}

static void __exit bad_fb_exit(void)
{
}

module_init(bad_fb_init);
module_exit(bad_fb_exit);
MODULE_LICENSE("Proprietary");
