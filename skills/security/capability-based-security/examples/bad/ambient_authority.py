# BAD: ambient authority — the check trusts the caller's identity, not a
# held capability. Anyone who can claim the role bypasses the object grant.
# This is the classic "process is root so it can do anything" failure.
# intentionally incorrect
class Object:
    def __init__(self, name):
        self.name = name


class Subject:
    def __init__(self, name, is_admin=False):
        self.name = name
        self.is_admin = is_admin


def write_file(subject, obj, content):
    # BAD: ambient check on identity/role, not on a held capability.
    if subject.is_admin:
        return f"ALLOWED: {subject.name} wrote to {obj.name}"
    return "DENIED"


def main():
    admin = Subject("root", is_admin=True)
    user = Subject("user")
    doc = Object("secret-report")

    print("admin write:", write_file(admin, doc, "x"))   # ALLOWED (by role)
    print("user write:", write_file(user, doc, "x"))     # DENIED (no role)

    # BAD: the vulnerability — any subject that becomes admin gains access to
    # every object. There is no per-object capability in the model at all.
    spoofed = Subject("root-impersonator", is_admin=True)
    print("spoofed write:", write_file(spoofed, doc, "x"))  # ALLOWED


if __name__ == "__main__":
    main()
