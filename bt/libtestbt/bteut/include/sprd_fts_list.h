#ifndef _SPRD_FTS_LIST_H_
#define _SPRD_FTS_LIST_H_

#ifdef __cplusplus
extern "C" {
#endif

#define _offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#define container_of(ptr, type, member) ( { \
const typeof( ((type *)0)->member ) *__mptr = (ptr); \
(type *)( (char *)__mptr - _offsetof(type,member) ); } )

static inline void prefetch(const void *x) {;}
static inline void prefetchw(const void *x) {;}

#define LIST_POISON1  ((void *) 0x00100100)
#define LIST_POISON2  ((void *) 0x00200200)

typedef struct list_head
{
  struct list_head *next;
  struct list_head *prev;
} list_t;

#define INIT_LIST_HEAD(ptr) \
  (ptr)->next = (ptr)->prev = (ptr)

#define LIST_HEAD(name) \
  list_t name = { &(name), &(name) }

#define INIT_LIST_HEAD(ptr) do { \
(ptr)->next = (ptr); (ptr)->prev = (ptr); \
} while (0)

static inline void
list_add (list_t *newp, list_t *prev, list_t *next)
{
  next->prev = newp;
  newp->next = next;
  newp->prev = prev;
  prev->next = newp;
}

static inline void
list_add_tail (list_t *newp, list_t *head)
{
  list_add(newp, head->prev, head);
}

static inline void
list_del (list_t *elem)
{
  elem->next->prev = elem->prev;
  elem->prev->next = elem->next;
}

static inline void
list_splice (list_t *add, list_t *head)
{
  if (add != add->next)
    {
      add->next->prev = head;
      add->prev->next = head->next;
      head->next->prev = add->prev;
      head->next = add->next;
    }
}

#define list_entry(ptr, type, member) \
  ((type *) ((char *) (ptr) - (unsigned long) (&((type *) 0)->member)))

#define list_for_each(pos, head) \
  for (pos = (head)->next; pos != (head); pos = pos->next)

#define list_for_each_prev(pos, head) \
  for (pos = (head)->prev; pos != (head); pos = pos->prev)

#define list_for_each_prev_safe(pos, p, head) \
  for (pos = (head)->prev, p = pos->prev; \
       pos != (head); \
       pos = p, p = pos->prev)

#ifdef __cplusplus
}
#endif

#endif
