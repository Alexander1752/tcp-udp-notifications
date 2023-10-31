#include <time.h>
#include <string.h>
#include "treap.h"
#include "util.h"

Treap* initLeaf(Treap *father, void *key)
{   
    Treap *leaf = (Treap*)malloc(sizeof(Treap));
    DIE(leaf == NULL, "malloc");

    leaf->key = key;
    leaf->left = leaf->right = NULL;
    leaf->father = father;
    leaf->priority = rand();

    return leaf;
}

void replaceInFather(Treap *old, Treap *new)
{
    if (old == NULL)
        return;

    if (old->father != NULL) {
        if (old->father->left == old)
            old->father->left = new;
        else old->father->right = new;
    }

    if (new != NULL)
        new->father = old->father;
}

#define eraseInFather(p) replaceInFather(p, NULL)
#define eraseLeaf(p) eraseInFather(p), free(p)

void replaceSon(Treap *father, Treap *new, enum sons son)
{
    if (father == NULL)
        return;

    if (son == leftSon)
        father->left = new;
    else father->right = new;

    if (new != NULL)
        new->father = father;
}

#define eraseSon(father, son) replaceSon(father, NULL, son)
#define replaceLeftSon(a, b) replaceSon(a, b, leftSon)
#define replaceRightSon(a, b) replaceSon(a, b, rightSon)

void rotLeft(Treap *w)
{
    Treap *z = w->father;
    replaceInFather(z, w); // w move
    replaceLeftSon(z, w->right); // B move
    replaceRightSon(w, z); // z move
}

void rotRight(Treap *z)
{
    Treap *w = z->father;
    replaceInFather(w, z); // z move
    replaceRightSon(w, z->left); // B move
    replaceLeftSon(z, w); // w move
}

void insert_key(Treap **root, void *key)
{
    Treap *p = *root;

    if (*root == NULL) {
        *root = initLeaf(NULL, key);
        return;
    }

    while (1)
        if (cmp(p->key, key) > 0) {
            if (p->left == NULL) {
                p->left = initLeaf(p, key);
                p = p->left;
                break;
            }
            else p = p->left;
        } else if (cmp(p->key, key) < 0) {
            if (p->right == NULL) {
                p->right = initLeaf(p, key);
                p = p->right;
                break;
            }
            else p = p->right;
        }

    while (1)
        if (p->father != NULL && p->father->priority < p->priority)
            if (p->father->left == p) // p is left son
                rotLeft(p);
            else rotRight(p);
        else break;

    if (p->father == NULL)
        *root = p;
}

void* delete_key(Treap **root, void *key)
{
    Treap *p = *root;
    int mx, maxSon, toUpdateRoot = 0;
    void *ret;

    while (p != NULL)
        if (cmp(p->key, key) == 0) { // key found
            if (p->left == NULL && p->right == NULL) { // leaf
                if (p == *root) // root
                    *root = NULL;
                ret = p->key;
                eraseLeaf(p);
                return ret;
            }
            else break;
        }
        else if (cmp(p->key, key) < 0)
            p = p->right;
        else p = p->left;

    if (p == NULL)
        return NULL;

    while (1)
        if (p->left == NULL && p->right == NULL) {
            ret = p->key;
            eraseLeaf(p);
            return ret;
        } else {
            mx = -1; // smaller than possible priority value

            if (p->left != NULL)
                mx = p->left->priority, maxSon = leftSon; // left is bigger
            else maxSon = rightSon;

            if (p->right != NULL && mx < p->right->priority)
                maxSon = rightSon; // right is bigger

            if (p == *root)
                toUpdateRoot = 1;

            if (maxSon == leftSon) // left is bigger
                rotLeft(p->left);
            else rotRight(p->right);

            if (toUpdateRoot) {
                *root = p->father;
                toUpdateRoot = 0;
            }
        }
}

void freeTreap(Treap **root, void (*free_data)(void*))
{
    if (*root == NULL)
        return;

    freeTreap(&((*root)->left), free_data);
    freeTreap(&((*root)->right), free_data);
    free_data((*root)->key);
    free(*root);

    *root = NULL;
}

Treap* search(Treap *root, void *key)
{
    while (root != NULL && cmp(root->key, key) != 0)
        if (cmp(root->key, key) > 0)
            root = root->left;
        else root = root->right;

    return root;
}
