#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

typedef struct Node Node;

typedef struct Cand {
    int w;
    int h;
    long long area;
    int left_idx;   // index into left child's candidate array, -1 for leaf
    int right_idx;  // index into right child's candidate array
} Cand;

struct Node {
    int isLeaf;
    int label;
    char cut; // 'V' or 'H' for internal, 0 for leaf
    Node *left, *right, *parent;
    // candidate list
    Cand *cands;
    size_t ncand;
    size_t candcap;
    // for output: chosen candidate index for baseline (first implementation) and optimal
    int chosen_baseline_idx;
    int chosen_opt_idx;
    // for coordinate output
    int out_w, out_h;
    int out_x, out_y;
};

// dynamic array helpers
static void push_cand(Node *n, Cand c) {
    if (n->ncand == n->candcap) {
        size_t nc = n->candcap ? n->candcap*2 : 16;
        Cand *tmp = realloc(n->cands, nc * sizeof(Cand));
        if (!tmp) { fprintf(stderr, "memory allocation failure\n"); exit(EXIT_FAILURE); }
        n->cands = tmp; n->candcap = nc;
    }
    n->cands[n->ncand++] = c;
}

// parse input and build tree from postorder lines
static Node *parse_input(const char *fname, Node ***leaves_order_out, size_t *nleaves_out) {
    FILE *f = fopen(fname, "r");
    if (!f) { perror(fname); return NULL; }
    Node **stack = NULL; size_t sz = 0, cap = 0;
    Node **leaves_order = NULL; size_t leaves_cap = 0, leaves_cnt = 0;
    char line[4096];
    while (fgets(line, sizeof(line), f)) {
        char *s = line;
        while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') ++s;
        if (*s == '\0') continue;
        if (*s == 'V' || *s == 'H') {
            // internal node: pop right then left (postorder)
            if (sz < 2) { fprintf(stderr, "malformed input: not enough children for %c\n", *s); fclose(f); return NULL; }
            Node *right = stack[--sz];
            Node *left = stack[--sz];
            Node *n = calloc(1, sizeof(Node)); if (!n) { perror("calloc"); fclose(f); return NULL; }
            n->isLeaf = 0; n->cut = *s; n->left = left; n->right = right; left->parent = n; right->parent = n;
            if (sz == cap) { size_t nc = cap ? cap*2 : 16; Node **tmp = realloc(stack, nc * sizeof(Node*)); if (!tmp) { perror("realloc"); fclose(f); return NULL; } stack = tmp; cap = nc; }
            stack[sz++] = n;
            continue;
        }
        // otherwise assume leaf: format label((w,h)(w,h)...) maybe spaces
        // parse label
        char *p = s;
        long lab = strtol(p, &p, 10);
        if (p == s) continue; // not a number
        Node *n = calloc(1, sizeof(Node)); if (!n) { perror("calloc"); fclose(f); return NULL; }
        n->isLeaf = 1; n->label = (int)lab; n->cut = 0;
        // find first '(' after label
        char *q = strchr(s, '(');
        while (q) {
            int w,h;
            if (sscanf(q, "(%d,%d)", &w, &h) == 2) {
                Cand c; c.w = w; c.h = h; c.area = (long long)w * (long long)h; c.left_idx = c.right_idx = -1;
                push_cand(n, c);
            }
            q = strchr(q+1, '(');
        }
        // push leaf to stack
        if (sz == cap) { size_t nc = cap ? cap*2 : 16; Node **tmp = realloc(stack, nc * sizeof(Node*)); if (!tmp) { perror("realloc"); fclose(f); return NULL; } stack = tmp; cap = nc; }
        stack[sz++] = n;
        // record leaves order
        if (leaves_cnt == leaves_cap) { size_t nc = leaves_cap ? leaves_cap*2 : 16; Node **tmp = realloc(leaves_order, nc * sizeof(Node*)); if (!tmp) { perror("realloc"); fclose(f); return NULL; } leaves_order = tmp; leaves_cap = nc; }
        leaves_order[leaves_cnt++] = n;
    }
    fclose(f);
    if (sz != 1) {
        fprintf(stderr, "malformed input: final stack size %zu\n", sz);
        free(stack);
        free(leaves_order);
        return NULL;
    }
    Node *root = stack[0];
    free(stack);
    *leaves_order_out = leaves_order; *nleaves_out = leaves_cnt;
    return root;
}

// combine two nodes candidates into parent based on cut type
static int cand_compare(const void *pa, const void *pb) {
    const Cand *a = pa; const Cand *b = pb;
    if (a->w != b->w) return a->w < b->w ? -1 : 1;
    if (a->h != b->h) return a->h < b->h ? -1 : 1;
    if (a->area < b->area) return -1;
    if (a->area > b->area) return 1;
    return 0;
}

static void combine_node(Node *parent) {
    Node *L = parent->left; Node *R = parent->right;
    // temporary array to collect combos
    Cand *tmp = NULL; size_t tcap = 0, tcnt = 0;
    for (size_t i = 0; i < L->ncand; ++i) {
        for (size_t j = 0; j < R->ncand; ++j) {
            int w,h;
            if (parent->cut == 'V') {
                w = L->cands[i].w + R->cands[j].w;
                h = (L->cands[i].h > R->cands[j].h) ? L->cands[i].h : R->cands[j].h;
            } else { // 'H'
                w = (L->cands[i].w > R->cands[j].w) ? L->cands[i].w : R->cands[j].w;
                h = L->cands[i].h + R->cands[j].h;
            }
            Cand c; c.w = w; c.h = h; c.area = (long long)w * (long long)h; c.left_idx = (int)i; c.right_idx = (int)j;
            if (tcnt == tcap) { size_t nc = tcap ? tcap*2 : 64; Cand *tmp2 = realloc(tmp, nc * sizeof(Cand)); if (!tmp2) { fprintf(stderr, "memory allocation failure\n"); exit(EXIT_FAILURE); } tmp = tmp2; tcap = nc; }
            tmp[tcnt++] = c;
        }
    }
    if (tcnt == 0) return;
    // dedupe keep minimal area for same (w,h)
    qsort(tmp, tcnt, sizeof(Cand), cand_compare);
    // compact unique (w,h) keeping smallest area
    size_t keep = 0;
    for (size_t i = 0; i < tcnt; ++i) {
        if (keep > 0 && tmp[i].w == tmp[keep-1].w && tmp[i].h == tmp[keep-1].h) {
            // keep the earlier (smaller area) entry
            continue;
        }
        tmp[keep++] = tmp[i];
    }
    // pareto prune: keep candidates such that no other candidate has w<=w' and h<=h'
    Cand *kept = NULL; size_t kcap = 0, kcnt = 0;
    int best_h = INT_MAX;
    for (size_t i = 0; i < keep; ++i) {
        if (tmp[i].h < best_h) {
            // keep
            if (kcnt == kcap) { size_t nc = kcap ? kcap*2 : 64; Cand *tmp2 = realloc(kept, nc * sizeof(Cand)); if (!tmp2) { fprintf(stderr, "memory allocation failure\n"); exit(EXIT_FAILURE); } kept = tmp2; kcap = nc; }
            kept[kcnt++] = tmp[i];
            best_h = tmp[i].h;
        }
    }
    // move kept into parent->cands
    for (size_t i = 0; i < kcnt; ++i) push_cand(parent, kept[i]);
    free(tmp); free(kept);
}

// postorder DP to compute candidate lists at each node
static void compute_candidates(Node *root) {
    if (!root) return;
    if (root->isLeaf) return; // leaf already has candidates
    compute_candidates(root->left);
    compute_candidates(root->right);
    combine_node(root);
}

// compute baseline sizes (first implementation) bottom-up
static void compute_baseline(Node *n) {
    if (n->isLeaf) {
        if (n->ncand == 0) { fprintf(stderr, "leaf %d has no implementations\n", n->label); return; }
        n->chosen_baseline_idx = 0;
        n->out_w = n->cands[0].w;
        n->out_h = n->cands[0].h;
        return;
    }
    compute_baseline(n->left);
    compute_baseline(n->right);
    int wl = n->left->out_w; int hl = n->left->out_h;
    int wr = n->right->out_w; int hr = n->right->out_h;
    if (n->cut == 'V') {
        n->out_w = wl + wr;
        n->out_h = (hl > hr) ? hl : hr;
    } else {
        n->out_w = (wl > wr) ? wl : wr;
        n->out_h = hl + hr;
    }
}

// compute coordinates top-down given out_w/out_h for children set for node
static void assign_coords(Node *n, int x, int y) {
    if (!n) return;
    n->out_x = x; n->out_y = y;
    if (n->isLeaf) return;
    if (n->cut == 'V') {
        // left is left, right to the right
        assign_coords(n->left, x, y);
        assign_coords(n->right, x + n->left->out_w, y);
    } else {
        // left is above, right is below
        assign_coords(n->right, x, y);
        assign_coords(n->left, x, y + n->right->out_h);
    }
}

// choose optimal candidate at root (min area) and propagate choices to reconstruct
static void reconstruct_opt(Node *n, int chosen_idx) {
    if (!n) return;
    n->chosen_opt_idx = chosen_idx;
    if (n->isLeaf) {
        n->out_w = n->cands[chosen_idx].w;
        n->out_h = n->cands[chosen_idx].h;
        return;
    }
    Cand c = n->cands[chosen_idx];
    reconstruct_opt(n->left, c.left_idx);
    reconstruct_opt(n->right, c.right_idx);
    // set this node size accordingly
    n->out_w = c.w; n->out_h = c.h;
}

// free tree
static void free_tree(Node *n) {
    if (!n) return;
    free_tree(n->left);
    free_tree(n->right);
    free(n->cands);
    free(n);
}

// write size to file as (w,h)\n
static void write_size_file(const char *fname, int w, int h) {
    FILE *f = fopen(fname, "w"); if (!f) { perror(fname); return; }
    fprintf(f, "(%d,%d)\n", w, h);
    fclose(f);
}

// write packing file with lines in order of leaves_in_input: label((w,h)(x,y))\n
static void write_packing_file(const char *fname, Node **leaves, size_t nleaves) {
    FILE *f = fopen(fname, "w"); if (!f) { perror(fname); return; }
    for (size_t i = 0; i < nleaves; ++i) {
        Node *leaf = leaves[i];
        fprintf(f, "%d((%d,%d)(%d,%d))\n", leaf->label, leaf->out_w, leaf->out_h, leaf->out_x, leaf->out_y);
    }
    fclose(f);
}

int main(int argc, char **argv) {
    if (argc != 6) {
        fprintf(stderr, "Usage: %s infile out1.dim out1.pck out2.dim out2.pck\n", argv[0]);
        return EXIT_FAILURE;
    }
    const char *infile = argv[1];
    const char *out1 = argv[2];
    const char *out2 = argv[3];
    const char *out3 = argv[4];
    const char *out4 = argv[5];

    Node **leaves = NULL; size_t nleaves = 0;
    Node *root = parse_input(infile, &leaves, &nleaves);
    if (!root) return EXIT_FAILURE;

    // baseline: use first implementation for each leaf
    compute_baseline(root);
    // assign coordinates: root placed at (0,0)
    assign_coords(root, 0, 0);
    // write baseline size and packing
    write_size_file(out1, root->out_w, root->out_h);
    // set baseline chosen_opt for output convenience
    for (size_t i = 0; i < nleaves; ++i) leaves[i]->chosen_opt_idx = leaves[i]->chosen_baseline_idx;
    write_packing_file(out2, leaves, nleaves);

    // compute optimal candidates via DP
    compute_candidates(root);
    if (root->ncand == 0) {
        fprintf(stderr, "no candidates computed for root\n");
        free_tree(root); free(leaves);
        return EXIT_FAILURE;
    }
    long long best_area = LLONG_MAX; int best_idx = 0;
    for (size_t i = 0; i < root->ncand; ++i) {
        long long a = root->cands[i].area;
        if (a < best_area) { best_area = a; best_idx = (int)i; }
    }
    reconstruct_opt(root, best_idx);
    assign_coords(root, 0, 0);
    write_size_file(out3, root->out_w, root->out_h);
    write_packing_file(out4, leaves, nleaves);

    free_tree(root);
    free(leaves);
    return EXIT_SUCCESS;
}
