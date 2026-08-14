#ifndef BAD_OWNERSHIP_H
#define BAD_OWNERSHIP_H

/* C transfers ownership of s once; the Rust side must free it exactly once. */
void take_twice(char *s);

#endif
