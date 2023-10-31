#ifndef TREAP_H
    #define TREAP_H
    typedef struct Treap
    {
        int priority;
        void *key;
        struct Treap *left, *right, *father;
    } Treap;
    enum sons {leftSon, rightSon};
#endif

extern int cmp(void *d1, void *d2);

void insert_key(Treap **root, void *key);
void* delete_key(Treap **root, void *key);
void freeTreap(Treap **root, void (*free_data)(void*));
Treap* search(Treap *root, void *key);
