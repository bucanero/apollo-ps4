#include "list.h"

list_t * list_alloc(void) {
	list_t *list;

	list = (list_t *)malloc(sizeof(list_t));
	if (!list)
		return NULL;
	memset(list, 0, sizeof(list_t));
	list->head = NULL;
	list->count = 0;

	return list;
}

void list_free(list_t *list) {
	list_node_t *node;
	list_node_t *tmp;

	if (!list)
		return;

	node = list->head;
	while (node) {
		tmp = node;
		node = node->next;
		free(tmp);
	}

	free(list);
}

list_node_t * list_append(list_t *list, void *value) {
	list_node_t *new_node;
	list_node_t *node;

	if (!list)
		return NULL;

	new_node = (list_node_t *)malloc(sizeof(list_node_t));
	if (!new_node)
		return NULL;
	new_node->value = value;
	new_node->next = NULL;

	node = list->head;
	if (!node)
		list->head = new_node;
	else {
		while (node) {
			if (!node->next) {
				node->next = new_node;
				break;
			}
			node = node->next;
		}
	}
	list->count++;

	return new_node;
}

list_node_t * list_head(list_t *list) {
	if (!list)
		return NULL;

	return list->head;
}

list_node_t * list_tail(list_t *list) {
	list_node_t *node = list_head(list);

	while (node && node->next) {
		node = node->next;
	}

	return node;
}

size_t list_count(list_t *list) {
	if (!list)
		return 0;

	return list->count;
}

list_node_t * list_next(list_node_t *node) {
	if (!node)
		return NULL;

	return node->next;
}

void * list_get(list_node_t *node) {
	if (!node)
		return NULL;

	return node->value;
}

void * list_get_item(list_t *list, size_t item) {
	list_node_t *node;

	if (!list || (list_count(list) < item))
		return NULL;

	node = list_head(list);
	while (item > 0) {
		node = list_next(node);
		item--;
	}

	return list_get(node);
}

/* function to swap data of two nodes a and b*/
void list_swap(list_node_t *a, list_node_t *b)
{
	void* temp = a->value;
	a->value = b->value;
	b->value = temp;
}

/* Bubble sort the given linked list */
void list_bubbleSort(list_t *list, int (*compar)(const void *, const void *))
{ 
	int swapped;
	list_node_t *ptr1; 
	list_node_t *lptr = NULL; 

	/* Checking for empty list */
	if (!list)
		return;

 	do
	{
		swapped = 0;
		ptr1 = list_head(list);

		while (ptr1->next != lptr)
		{
			if (compar(ptr1->value, ptr1->next->value) > 0)
			{
				list_swap(ptr1, ptr1->next);
				swapped = 1;
			}
			ptr1 = ptr1->next;
		}
		lptr = ptr1;
	}
	while (swapped);
}
