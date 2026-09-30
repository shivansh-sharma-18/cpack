#include "huffman.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
  HuffmanNode **nodes;
  size_t size;
  size_t capacity;
} HuffmanHeap;

void huffman_count_frequencies(const uint8_t *data, size_t length,
                               uint64_t frequencies[HUFFMAN_SYMBOLS]) {
  for (size_t i = 0; i < HUFFMAN_SYMBOLS; i++) {
    frequencies[i] = 0;
  }

  for (size_t i = 0; i < length; i++) {
    frequencies[data[i]]++;
  }
}

HuffmanNode *huffman_create_node(uint8_t symbol, uint64_t frequency) {
  HuffmanNode *node = malloc(sizeof(HuffmanNode));
  if (node == NULL) {
    return NULL;
  }

  node->symbol = symbol;
  node->frequency = frequency;
  node->left = NULL;
  node->right = NULL;
  node->order = 0;

  return node;
}

static int heap_node_less(const HuffmanNode *a, const HuffmanNode *b) {
  if (a->frequency != b->frequency) {
    return a->frequency < b->frequency;
  }
  return a->order < b->order;
}

static void heap_swap(HuffmanNode **a, HuffmanNode **b) {
  HuffmanNode *temp = *a;
  *a = *b;
  *b = temp;
}

static void heap_sift_up(HuffmanHeap *heap, size_t index) {
  while (index > 0) {
    size_t parent = (index - 1) / 2;

    if (!heap_node_less(heap->nodes[index], heap->nodes[parent])) {
      break;
    }

    heap_swap(&heap->nodes[index], &heap->nodes[parent]);
    index = parent;
  }
}

static void heap_sift_down(HuffmanHeap *heap, size_t index) {
  while (1) {
    size_t left = index * 2 + 1;
    size_t right = index * 2 + 2;
    size_t smallest = index;

    if (left < heap->size &&
        heap_node_less(heap->nodes[left], heap->nodes[smallest])) {
      smallest = left;
    }

    if (right < heap->size &&
        heap_node_less(heap->nodes[right], heap->nodes[smallest])) {
      smallest = right;
    }

    if (smallest == index) {
      break;
    }

    heap_swap(&heap->nodes[index], &heap->nodes[smallest]);
    index = smallest;
  }
}

static int heap_push(HuffmanHeap *heap, HuffmanNode *node) {
  if (heap->size >= heap->capacity) {
    return 0;
  }

  heap->nodes[heap->size] = node;
  heap_sift_up(heap, heap->size);
  heap->size++;

  return 1;
}

static HuffmanNode *heap_pop(HuffmanHeap *heap) {
  if (heap->size == 0) {
    return NULL;
  }

  HuffmanNode *result = heap->nodes[0];
  heap->size--;

  if (heap->size > 0) {
    heap->nodes[0] = heap->nodes[heap->size];
    heap_sift_down(heap, 0);
  }

  return result;
}

HuffmanNode *huffman_build_tree(const uint64_t frequencies[HUFFMAN_SYMBOLS]) {
  HuffmanHeap heap;

  heap.capacity = HUFFMAN_SYMBOLS * 2;
  heap.size = 0;
  heap.nodes = malloc(heap.capacity * sizeof(HuffmanNode *));

  if (heap.nodes == NULL) {
    return NULL;
  }

  uint32_t order = 0;

  for (size_t i = 0; i < HUFFMAN_SYMBOLS; i++) {
    if (frequencies[i] == 0) {
      continue;
    }

    HuffmanNode *node = huffman_create_node((uint8_t)i, frequencies[i]);
    if (node == NULL) {
      free(heap.nodes);
      return NULL;
    }

    node->order = order++;

    if (!heap_push(&heap, node)) {
      free(node);
      free(heap.nodes);
      return NULL;
    }
  }

  if (heap.size == 0) {
    free(heap.nodes);
    return NULL;
  }

  if (heap.size == 1) {
    HuffmanNode *root = heap_pop(&heap);
    free(heap.nodes);
    return root;
  }

  while (heap.size > 1) {
    HuffmanNode *left = heap_pop(&heap);
    HuffmanNode *right = heap_pop(&heap);

    HuffmanNode *parent =
        huffman_create_node(0, left->frequency + right->frequency);

    if (parent == NULL) {
      free(heap.nodes);
      return NULL;
    }

    parent->left = left;
    parent->right = right;
    parent->order = order++;

    if (!heap_push(&heap, parent)) {
      free(parent);
      free(heap.nodes);
      return NULL;
    }
  }

  HuffmanNode *root = heap_pop(&heap);
  free(heap.nodes);

  return root;
}

void huffman_free_tree(HuffmanNode *root) {
  if (root == NULL) {
    return;
  }

  huffman_free_tree(root->left);
  huffman_free_tree(root->right);
  free(root);
}

static void generate_codes_recursive(const HuffmanNode *node,
                                     HuffmanCode codes[HUFFMAN_SYMBOLS],
                                     uint8_t path[HUFFMAN_MAX_CODE_LENGTH],
                                     size_t depth) {
  if (node == NULL) {
    return;
  }

  if (node->left == NULL && node->right == NULL) {
    if (depth == 0) {
      path[0] = 0;
      depth = 1;
    }

    codes[node->symbol].length = (uint16_t)depth;
    memcpy(codes[node->symbol].bits, path, depth);
    return;
  }

  if (node->left != NULL) {
    path[depth] = 0;
    generate_codes_recursive(node->left, codes, path, depth + 1);
  }

  if (node->right != NULL) {
    path[depth] = 1;
    generate_codes_recursive(node->right, codes, path, depth + 1);
  }
}

int huffman_generate_codes(const HuffmanNode *root,
                           HuffmanCode codes[HUFFMAN_SYMBOLS]) {
  if (root == NULL) {
    return 0;
  }

  for (size_t i = 0; i < HUFFMAN_SYMBOLS; i++) {
    codes[i].length = 0;
    memset(codes[i].bits, 0, sizeof(codes[i].bits));
  }

  uint8_t path[HUFFMAN_MAX_CODE_LENGTH];
  memset(path, 0, sizeof(path));

  generate_codes_recursive(root, codes, path, 0);

  return 1;
}

int huffman_encode(const uint8_t *data, size_t length,
                   const HuffmanCode codes[HUFFMAN_SYMBOLS],
                   BitWriter *writer) {
  for (size_t i = 0; i < length; i++) {
    const HuffmanCode *code = &codes[data[i]];

    if (code->length == 0) {
      return 0;
    }

    for (size_t bit = 0; bit < code->length; bit++) {
      if (!bitwriter_write_bit(writer, code->bits[bit])) {
        return 0;
      }
    }
  }

  return 1;
}

uint8_t *huffman_decode(BitReader *reader, const HuffmanNode *root,
                        size_t original_length) {
  if (root == NULL) {
    return NULL;
  }

  uint8_t *output = malloc(original_length == 0 ? 1 : original_length);
  if (output == NULL) {
    return NULL;
  }

  if (root->left == NULL && root->right == NULL) {
    for (size_t i = 0; i < original_length; i++) {
      int bit = bitreader_read_bit(reader);
      if (bit != 0) {
        free(output);
        return NULL;
      }
      output[i] = root->symbol;
    }
    return output;
  }

  for (size_t i = 0; i < original_length; i++) {
    const HuffmanNode *current = root;

    while (current->left != NULL || current->right != NULL) {
      int bit = bitreader_read_bit(reader);
      if (bit == -1) {
        free(output);
        return NULL;
      }

      if (bit == 0) {
        current = current->left;
      } else {
        current = current->right;
      }

      if (current == NULL) {
        free(output);
        return NULL;
      }
    }

    output[i] = current->symbol;
  }

  return output;
}