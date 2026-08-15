# intentionally incorrect: an overlay mount with only lowerdir works for
# reads but the claim that "whiteout is a regular empty file" is wrong:
# whiteouts are char devices 0/0. Also, without upperdir/workdir there is no
# copy-up, so writes fail on a read-only lowerdir.
mount -t overlay overlay -o lowerdir=/tmp/lower /mnt/merged
