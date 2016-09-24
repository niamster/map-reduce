#ifndef _DLLIST_H
#define _DLLIST_H

#include <stdbool.h>

#define container_of(ptr, type, member) \
    (type *)((char *)ptr - offsetof(type, member))

typedef struct dllist {
    struct dllist *next;
    struct dllist *prev;
} dllist_t;

static inline void
dllist_init(dllist_t *head) {
    head->next = head->prev = head;
}

static inline bool
dllist_empty(dllist_t *head) {
    return head->next == head && head->prev == head;
}

static inline dllist_t *
dllist_first(dllist_t *head) {
    return head->next;
}

static inline dllist_t *
dllist_last(dllist_t *head) {
    return head->prev;
}

#define dllist_for_each(head, e)                \
    for ((e)=(head)->next;                      \
         (e)!=(head);                           \
         (e)=(e)->next)

#define dllist_for_each_safe(head, e, t)        \
    for ((t)=(head)->next->next, (e)=(t)->prev; \
         (e)!=(head);                           \
         (t)=(t)->next, (e)=(t)->prev)

static inline void
dllist_detach(dllist_t *e) {
    e->prev->next = e->next;
    e->next->prev = e->prev;
}

static inline void
dllist_add_head(dllist_t *head, dllist_t *e) {
    head->next->prev = e;
    e->next = head->next;
    e->prev = head;
    head->next = e;
}

static inline void
dllist_add_tail(dllist_t *head, dllist_t *e) {
    head->prev->next = e;
    e->prev = head->prev;
    head->prev = e;
    e->next = head;
}

#endif
