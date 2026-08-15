# intentionally incorrect: "root in a user namespace equals host root" is
# false. CAP_SYS_ADMIN gained inside a user namespace does not extend to the
# init user namespace for most operations (no host mount, no host devices).
echo "uid 0 in userns == host root for everything"
