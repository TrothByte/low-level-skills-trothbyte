# intentionally incorrect: reads /proc/vmallocinfo columns backwards
# (caller as size) and claims slabinfo column 1 is "active_objs" — column 1
# is the cache name. Any parse that relies on these column meanings is wrong.
#!/bin/sh
awk '{ print "caller="$1, "size="$2 }' /proc/vmallocinfo | head -3
