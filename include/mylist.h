#ifndef MYLIST_H
#define MYLIST_H

#include <stdlib.h>

/* Linkedlist used for  t_each_customer & t_each_close in cashierArgs_h */

typedef struct ElementoDiLista {
	int id;			// ID customer (0 in list t_close)
	long info;			// time
	struct ElementoDiLista* next;
} ElementoDiLista;
typedef ElementoDiLista* ListaDiElementi;

/* get next element */
int next (ListaDiElementi list) {
	static ListaDiElementi aux = NULL;
	long res;
	
	if (list==NULL && aux==NULL) {
		return -1;
	}
	
	if (list==NULL) {
		res = aux->info;
		aux = aux->next;
	}
	else {
		res = list->info;
		aux = list->next;
	}
	return res;
}

/* add element */
void put_list (ListaDiElementi* list, int id, long value) {
	ListaDiElementi aux= *list;
	
	if (aux==NULL) {
		(*list)	= (ListaDiElementi)malloc(sizeof(ElementoDiLista));
		(*list)->id	= id;
		(*list)->info	= value;
		(*list)->next	= NULL;
	}
	else {
		while (aux->next!=NULL) {
			aux 	= aux->next;
		}
		aux->next	= (ListaDiElementi)malloc(sizeof(ElementoDiLista));
		aux		= aux->next;
		aux->id	= id;
		aux->info	= value;
		aux->next	= NULL;
	}
}
/* return length */
int length_list (ListaDiElementi list) {
	ListaDiElementi aux = list;
	int len= 0;
	
	while (aux!=NULL) {
		len++;
		aux= aux->next;
	}
	return len;
}
/* free space */
void free_list (ListaDiElementi* list) {
	ListaDiElementi curr = *list;
	ListaDiElementi next;
	
	while (curr!=NULL) {
		next = curr->next;
		free(curr);
		curr = next;
	}
	*list = NULL;
}

#endif /* MYLIST_H */
