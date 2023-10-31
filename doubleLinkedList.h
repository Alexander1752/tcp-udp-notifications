#ifndef DOUBLE_LINKED_LIST_H
    #define DOUBLE_LINKED_LIST_H
    typedef struct node
    {
        void *data;
        struct node *next;
        struct node *prev;
    } node;

    typedef struct TDoubleLinkedList
    {
        node *sentinel;
        int len;
} TDoubleLinkedList;
#endif

void init(TDoubleLinkedList **list);
int addNode(TDoubleLinkedList *list,int n,void* data);
int addLast(TDoubleLinkedList *list, void* data);
node* removeNode(TDoubleLinkedList *list, node *n);
node* removeLast(TDoubleLinkedList *list);
node* removeNodeAt(TDoubleLinkedList *list, int n);
int length(TDoubleLinkedList *list);
int empty(TDoubleLinkedList *list);
void free_list(TDoubleLinkedList **list);
