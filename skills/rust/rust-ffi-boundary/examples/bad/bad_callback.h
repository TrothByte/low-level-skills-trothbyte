#ifndef BAD_CALLBACK_H
#define BAD_CALLBACK_H

/* C expects a plain function pointer; a capturing closure cannot satisfy this. */
void register_cb(void (*cb)(int));

#endif
