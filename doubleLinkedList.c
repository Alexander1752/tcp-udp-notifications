#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "util.h"
#include "doubleLinkedList.h"

/* Allocates memory for the list and initializes its fields */
void init(TDoubleLinkedList **list)
{
    *list = malloc(sizeof(TDoubleLinkedList));
    DIE(*list == NULL, "malloc");

    (*list)->len = 0;
    node *sentinel = (node*)malloc(sizeof(node));
    DIE(sentinel == NULL, "malloc");

    sentinel->prev = sentinel->next = sentinel;

    (*list)->sentinel = sentinel;
    (*list)->sentinel->data = NULL;
}

/* Adds node at the end of the list */
int addLast(TDoubleLinkedList *list, void* data)
{
    return addNode(list, list->len, data);
}

/* 
 * Adds node on position n, returns 1 on succes or -1 otherwise
 * (n is negative or greater than the size of the list)
 */
int addNode(TDoubleLinkedList *list,int n,void* data)
{
    int i;
    node *p, *u;

    if (n < 0 || n > list->len)
        return -1;

    if (n == 0) {
        p = list->sentinel->next;
        list->sentinel->next = malloc(sizeof(node));
        DIE(list->sentinel->next == NULL, "malloc");

        list->sentinel->next->data = data;
        list->sentinel->next->next = p;
        list->sentinel->next->prev = list->sentinel;

        if (list->sentinel->prev == list->sentinel)
            list->sentinel->prev = list->sentinel->next;
        else p->prev = list->sentinel->next;

        list->len++;
        return 1;
    }

    if (n == list->len) {
        p = list->sentinel->prev;
        list->sentinel->prev = malloc(sizeof(node));
        DIE(list->sentinel->prev == NULL, "malloc");

        list->sentinel->prev->data = data;
        list->sentinel->prev->prev = p;
        list->sentinel->prev->next = list->sentinel;

        p->next = list->sentinel->prev;
        list->len++;
        return 1;
    }

    u = malloc(sizeof(node));
    DIE(u == NULL, "malloc");
    u->data = data;

    if (n < list->len / 2) {
        for (p = list->sentinel->next, i = 1; i < n; ++i)
            p = p->next;
        u->next = p->next;
        u->prev = p;
        if (u->next)
            u->next->prev = u;
        p->next = u;
    } else {
        for (p = list->sentinel->prev, i = list->len - 2; i >=  n; --i)
            p = p->prev;
        u->next = p;
        u->prev = p->prev;
        if (u->prev)
            u->prev->next = u;
        p->prev = u;
    }

    list->len++;
    return 1;
}

/* Removes given node and returns the one preceding it */
node* removeNode(TDoubleLinkedList *list, node *n)
{
    node *p = n->prev, *u = n->next;
    p->next = u;
    p->next->prev = p;
    list->len--;
    return p;
}

/* Removes last node*/
node* removeLast(TDoubleLinkedList *list)
{
    return removeNodeAt(list, list->len - 1);
}

/* Removes node on position n and returns it */
node* removeNodeAt(TDoubleLinkedList *list, int n)
{
    int i;
    node *p,*u;

    if (empty(list))
        return NULL;

    if (n >= list->len || n < 0)
        return list->sentinel;

    if (n == 0) {
        p = list->sentinel->next;
        list->sentinel->next = p->next;
        if (list->sentinel->next)
            list->sentinel->next->prev = list->sentinel;
        p->next = p->prev = NULL;
        list->len--;
        return p;
    }

    if (n == list->len - 1) {
        p = list->sentinel->prev;
        list->sentinel->prev = p->prev;
        if (list->sentinel->prev)
            list->sentinel->prev->next = list->sentinel;
        p->next = p->prev = NULL;
        list->len--;
        return p;
    }

    if (n < list->len / 2) {
        for (p = list->sentinel->next, i = 1; i < n; ++i)
            p = p->next;
        u = p->next;
        p->next = u->next;
        if (p->next)
            p->next->prev = p;
    } else {
        for (p = list->sentinel->prev,i = list->len - 2; i > n; --i)
            p = p->prev;
        u = p->prev;
        p->prev = u->prev;
        if (p->prev)
            p->prev->next = p;
    }

    u->next = u->prev = NULL;
    list->len--;
    return u;
}

int length(TDoubleLinkedList *list)
{
    return list->len;
}

int empty(TDoubleLinkedList *list)
{
    return list == NULL || length(list) == 0;
}

void free_list(TDoubleLinkedList **list)
{
    if (*list == NULL)
        return;

    node *p = (*list)->sentinel, *u;

    do
    {
        u = p->next;
        free(p->data);
        free(p);
        p = u;
    }while(p != (*list)->sentinel);

    free(*list);
    *list = NULL;
}
