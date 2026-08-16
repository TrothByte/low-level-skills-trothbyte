# GOOD: object-capability model — unforgeable, confined, revocable.
# A capability is a token in the subject's possession; the check uses the
# held capability, never the caller's identity. Run: python3 capability_model.py
class Capability:
    """Unforgeable token: only the kernel factory can create one."""

    def __init__(self, owner, obj, rights):
        self._owner = owner          # subject that holds this token
        self._obj = obj              # object it authorizes
        self._rights = frozenset(rights)

    @property
    def owner(self):
        return self._owner

    @property
    def obj(self):
        return self._obj

    def permits(self, right):
        return right in self._rights


class Object:
    def __init__(self, name):
        self.name = name
        self.data = []


class Kernel:
    """The only entity that can mint or revoke capabilities."""

    def __init__(self):
        self._active = set()

    def grant(self, subject, obj, rights):
        cap = Capability(subject, obj, rights)
        self._active.add(id(cap))
        return cap

    def revoke(self, cap):
        # GOOD: revoke removes the authority for THIS token. Derived copies
        # must also be revoked (seL4 Revoke semantics) — here we invalidate
        # by token identity and document the derived-copy caveat.
        self._active.discard(id(cap))


class Subject:
    def __init__(self, name, kernel):
        self.name = name
        self.kernel = kernel

    def invoke(self, cap, right, obj):
        # GOOD: authority is the held capability, not the subject identity.
        if id(cap) not in self.kernel._active:
            return "DENIED (revoked)"
        if not cap.permits(right) or cap.obj is not obj or cap.owner is not self:
            return "DENIED (no right on object)"
        return "ALLOWED"


def main():
    k = Kernel()
    alice = Subject("alice", k)
    bob = Subject("bob", k)
    doc = Object("report.pdf")

    cap = k.grant(alice, doc, {"read"})

    print("alice read:", alice.invoke(cap, "read", doc))    # ALLOWED
    print("alice write:", alice.invoke(cap, "write", doc))  # DENIED (no right)
    print("bob read with alice's cap:", bob.invoke(cap, "read", doc))  # DENIED

    k.revoke(cap)
    print("alice read after revoke:", alice.invoke(cap, "read", doc))  # DENIED


if __name__ == "__main__":
    main()
