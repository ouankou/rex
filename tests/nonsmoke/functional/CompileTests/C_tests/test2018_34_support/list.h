/* SPDX-License-Identifier: ISC */
#ifndef TEST2018_34_SUPPORT_LIST_H
#define TEST2018_34_SUPPORT_LIST_H

#define TQ_DECLARE(name)                                                       \
  struct name##_list {                                                         \
    struct name *first;                                                        \
    struct name *last;                                                         \
  }

#define tq_empty(head) ((head)->first == NULL)

void *tq_pop(void *head);
void tq_append(void *head, void *entry);
void tq_remove(void *head, void *entry);
void list_append(void *head, void *entry);
void list2tq(void *head, void *entry);

#endif
