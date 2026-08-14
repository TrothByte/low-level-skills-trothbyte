/* BAD: return value of copy_from_user is ignored (CWE-252). */
#include "../stubs.h"
#include <stdio.h>
#include <string.h>

struct request {
	unsigned int op;
	unsigned int len;
	char data[32];
};

/* BAD: copy failure is invisible; req is stale or partially copied */
static int handle_cmd(struct request *req, const struct request __user *u)
{
	copy_from_user_emu(req, u, sizeof *req);
	return req->op == 1 ? 0 : -EINVAL;
}

int main(void)
{
	struct request kreq;

	memset(&kreq, 0, sizeof kreq);
	kreq.op = 1;
	/* u is outside the user region, so copy_from_user returns sizeof(*req)
	 * and leaves kreq untouched — yet the driver proceeds with kreq.op. */
	if (handle_cmd(&kreq, (const struct request __user *)&kreq) == 0)
		printf("BUG reproduced: used data copy_from_user failed to deliver\n");
	return 0;
}
