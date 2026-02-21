	// SPDX-License-Identifier: BSD-3-Clause

	#include <sys/mman.h>
	#include <string.h>
	#include <stdlib.h>
	#include <unistd.h>
	#include <stdio.h>
	#include <limits.h>
	#include "osmem.h"
	#include "block_meta.h"

	#define threshold (128 * 1024)
	#define minimal_block_use 40
	#define padded_struct 32
	//32 e structul padded plus oricat de mic as aloca memorie inseamna +8

	//4096 threshold pentru calloc

	struct block_meta *global_head; //pointer global la inceputul listei
	int bool_prealloc = 1;

	//ca sa fac split am nevoie de cel putin 40 bytes of free memory


	//itereaza prin lista pana gaseste un block liber suficient de mare pt a aloca memoria
	//trebuie facut BEST SIZE

	struct block_meta *get_free_block(size_t size)
	{
		struct block_meta *current = global_head; //pointer la global_head

		size_t min_diff = INT_MAX;
		struct block_meta *ptr = NULL;

		while (current != NULL) {
			if (current->status == 0 && current->size >= size) {
				size_t diff = current->size - size;

				if (diff < min_diff) {
					min_diff = diff;
					ptr = current;
				}
			}
			current = current->next;
		}
		return ptr;
	}

	struct block_meta *expand_block(struct block_meta *ptr, size_t size)
	{
		void *new_alloc = sbrk(size);

		if (new_alloc == (void *)-1)
			return NULL;

		ptr->next = NULL;
		ptr->status = 1;

		return ptr;
	}
	//functia uneste blocuri consecutive eliberate de memorie
	void *coalesce_blocks(struct block_meta *block)
	{
		struct block_meta *current = block;

		if (current != NULL && current->next != NULL) {
			while (current != NULL && current->next != NULL) {
				if (current->status == 0 && current->next->status == 0) {
				//adaug la marimea blockului size-ul unui nou block plus metadata
				//in cazul in care 2 blocuri se unesc nu ma mut pe urmatorul block
				//deoarece vreau sa compar current-ul marit cu urmatorul poate e liber si el
					current->size = current->size + current->next->size;
					current->next = current->next->next;
					if (current->next != NULL)
						current->next->prev = current;
				} else {
					current = current->next;
			}
		}
		}

		return block;
	}

	void *os_malloc(size_t size)
	{
		/* TODO: Implement os_malloc */

		coalesce_blocks(global_head);

		if (size <= 0)
			return NULL;

		//padded size
		if (size % 8 != 0) {
			while (size % 8 != 0)
				size++;
			}


		if (bool_prealloc == 1 && size + padded_struct <= threshold) {
			struct block_meta *block_prealloc;

			block_prealloc = sbrk(threshold);

			if (block_prealloc == NULL)
				return NULL;

			global_head = block_prealloc;
			global_head->next = NULL;
			global_head->prev = NULL;
			global_head->status = 1;
			global_head->size = threshold - padded_struct;

			bool_prealloc = 0;

			return (void *)(global_head+1);
		}


		struct block_meta *for_use = get_free_block(size + padded_struct);

		if (for_use != NULL) {
			for_use->status = 1;

			//se da split in caz ca mai ramane loc in block
			if (for_use->size - (size  + padded_struct) >= minimal_block_use) {
				struct block_meta *new_block = (struct block_meta *)((char *)for_use + size + padded_struct);

				new_block->size = for_use->size - size - padded_struct;
				new_block->status = 0;
				new_block->prev = for_use;
				new_block->next = for_use->next;
				if (for_use->next != NULL)
					for_use->next->prev = new_block;
				for_use->next = new_block;
				for_use->size = size + padded_struct;
				} else {
					if (for_use->size - (size + padded_struct) <= 39)
						for_use->status = 1;
				}
				return (void *)(for_use + 1);
			}


		//aici se da expand ultimului block
		if (global_head != NULL) {
			struct block_meta *check_last_block = global_head;

			if (check_last_block != NULL) {
				while (check_last_block->next != NULL)
					check_last_block = check_last_block->next;

			if (check_last_block->status == 0 && check_last_block->size < size + padded_struct) {
				size_t additional_size = size + padded_struct - check_last_block->size;

				check_last_block = expand_block(check_last_block, additional_size);

				check_last_block->size = check_last_block->size + additional_size;

				return (void *)(check_last_block + 1);
			}
			}
		}



		if (size + padded_struct >= threshold) { //threshold este de 128 kb
			struct block_meta *block_over_thresh = global_head;
			struct block_meta *mmap_block = NULL;

			if (global_head == NULL)
				global_head = mmap_block;

			if (block_over_thresh != NULL) {
				while (block_over_thresh->next != NULL)
					block_over_thresh = block_over_thresh->next;
			}
			mmap_block = (struct block_meta *)mmap(NULL, size + padded_struct, PROT_READ | PROT_WRITE,
			MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

			if (mmap_block == MAP_FAILED)
				return NULL;	//eroare
			mmap_block->size = size + padded_struct;
			mmap_block->status = 2;
			mmap_block->next = NULL;
			mmap_block->prev = block_over_thresh;
			if (block_over_thresh != NULL)
				block_over_thresh->next = mmap_block;
			else
				global_head = mmap_block;
			//returnez +1 pentru ca vreau sa ignor nr de bytes alocati si sa inceapa cu data alocata
			return (void *)(mmap_block + 1);
		}

		//sub threshold-ul de 128 kb folosesc brk si sbrk pentru a aloca memorie
		if (size + padded_struct > 0 && size + padded_struct < threshold) {
			struct block_meta *creation_block = global_head; // = get_free_block(size);
			struct block_meta *last_block = creation_block;

			while (creation_block->next != NULL)
				creation_block = creation_block->next;
			last_block = sbrk(size + padded_struct);
			if (last_block == (void *)-1)
				return NULL;
			last_block->size = size + padded_struct;
			last_block->status = 1;
			last_block->next = NULL;
			creation_block->next = last_block;
			last_block->prev = creation_block;
			return (void *)(last_block + 1);
			}
		return NULL;
	}

	void *os_malloc_2(size_t size)
	{
		/* TODO: Implement os_malloc */

		//coalesce_blocks(global_head);

		if (size <= 0)
			return NULL;

		//padded size
		if (size % 8 != 0) {
			while (size % 8 != 0)
				size++;
			}


		if (bool_prealloc == 1 && size + padded_struct <= 4096) {
			struct block_meta *block_prealloc;

			block_prealloc = sbrk(threshold);

			if (block_prealloc == NULL)
				return NULL;

			global_head = block_prealloc;
			global_head->next = NULL;
			global_head->prev = NULL;
			global_head->status = 1;
			global_head->size = threshold - padded_struct;

			bool_prealloc = 0;

			return (void *)(global_head+1);
		}

		//coalesce_blocks(global_head);

		struct block_meta *for_use = get_free_block(size + padded_struct);

		if (for_use != NULL) {
			for_use->status = 1;

			//se da split in caz ca mai ramane loc in block
			if (for_use->size - (size  + padded_struct) >= minimal_block_use) {
				struct block_meta *new_block = (struct block_meta *)((char *)for_use + size + padded_struct);

				new_block->size = for_use->size - size - padded_struct;
				new_block->status = 0;
				new_block->prev = for_use;
				new_block->next = for_use->next;
				if (for_use->next != NULL)
					for_use->next->prev = new_block;
				for_use->next = new_block;
				for_use->size = size + padded_struct;
			} else {
				if (for_use->size - (size + padded_struct) <= 39) //potrivire exacta
					for_use->status = 1;
			}
			return (void *)(for_use + 1);
		}


		//aici se da expand ultimului block
		struct block_meta *check_last_block = global_head;

		if (check_last_block != NULL) {
			while (check_last_block->next != NULL)
				check_last_block = check_last_block->next;

		if (check_last_block->status == 0 && check_last_block->size < size + padded_struct) {
			size_t additional_size = size + padded_struct - check_last_block->size;

			check_last_block = expand_block(check_last_block, additional_size);
			check_last_block->size = check_last_block->size + additional_size;
			return (void *)(check_last_block + 1);
		}
		}



		if (size + padded_struct >= 4096) { //threshold este de get_page_size de 4096
			struct block_meta *block_over_thresh = global_head;
			struct block_meta *mmap_block = NULL;

			if (global_head == NULL)
				global_head = mmap_block;

			if (block_over_thresh != NULL) {
				while (block_over_thresh->next != NULL)
					block_over_thresh = block_over_thresh->next;
			}

			mmap_block = (struct block_meta *)mmap(NULL, size + padded_struct, PROT_READ | PROT_WRITE,
			MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

			if (mmap_block == MAP_FAILED)
				return NULL;	//eroare

			mmap_block->size = size + padded_struct;
			mmap_block->status = 2;
			mmap_block->next = NULL;
			mmap_block->prev = block_over_thresh;

			if (block_over_thresh != NULL)
				block_over_thresh->next = mmap_block;
			else
				global_head = mmap_block;
			//returnez +1 pentru ca vreau sa ignor nr de bytes alocati si sa inceapa cu data alocata
			return (void *)(mmap_block + 1);
		}

		//sub threshold-ul de 128 kb folosesc brk si sbrk pentru a aloca memorie
		if (size + padded_struct > 0 && size + padded_struct < 4096) {
			struct block_meta *creation_block = global_head; // = get_free_block(size);
			struct block_meta *last_block = creation_block;

			while (creation_block->next != NULL)
				creation_block = creation_block->next;

			last_block = sbrk(size + padded_struct);

			if (last_block == (void *)-1)
				return NULL;

			last_block->size = size + padded_struct;
			last_block->status = 1;
			last_block->next = NULL;
			creation_block->next = last_block;
			last_block->prev = creation_block;
			return (void *)(last_block + 1);
			}
		return NULL;
	}


	//mmap are status = 2
	void os_free(void *ptr)
	{
		/* TODO: Implement os_free */

		struct block_meta *block;

		if (ptr == NULL)
			return;

		block = (struct block_meta *)((char *)ptr - padded_struct); //metadata

		if (block->status == 0)
			return;

		if (block->status == 2) {
			if (block->prev != NULL)
				block->prev->next = block->next;
			if (block->next != NULL)
				block->next->prev = block->prev;

			munmap(block, block->size);
		} else if (block->status == 1) {
			block->status = 0;
			coalesce_blocks(global_head);
		}
	}


	void *os_calloc(size_t nmemb, size_t size)
	{
		/* TODO: Implement os_calloc */

		if (size == 0 || nmemb == 0)
			return NULL;

		size_t size_calloc = nmemb * size;

		void *ptr = os_malloc_2(size_calloc);

		if (ptr == NULL)
			return NULL;

		memset(ptr, 0, size_calloc);

		return ptr;
	}

	//foarte important se da coalesce pe rand si intotdeauna verific daca size-ul meu are loc in noul free
	//se da coalesce la block-urile care sunt free cu 1 chiar daca intre ele se afla mmapuri
	//in cazul in care dau realloc la ceva ce a fost alocat cu mmap intai dau unmap apoi aloc noua dimensiune
	//pointerul 0 ca prim argument inseamna malloc
	//in dreapta la ref am adresa de unde incepe alocarea

	//cazuri pentru pointer diferit de NULL : [expand final], [merge-uri repetitive], sau daca nu se poate
	//niciuna daca e mai mic size-ul de realocat trebuie sa vad daca dau split SAU daca size-ul e mai mare aloc cu
	//malloc la final sizeul mai mare apoi cu memcpy mut blockul de realocat la final apoi dau free la cel din mijloc
	//oriocum verific free-urile de dupa ptr si daca se gaseste free de la globalhead pune si chiar in stanga ptr
	//alocat cu mmap se da free apoi se da alloc cu mmap
	//realloc in place
	void *os_realloc(void *ptr, size_t size)
	{
		/* TODO: Implement os_realloc */

		if (!ptr)
			return os_malloc(size);

		if (size == 0) {
			os_free(ptr);
			return NULL;
		}

		//daca se face prealocare atunci cand nu s-a mai alocat nimic si e mai mic decat thresholdul
		if (bool_prealloc == 1 && size + padded_struct < threshold) {
			struct block_meta *block_prealloc;

			//daca e prima alocare
			if (global_head == NULL) {
				block_prealloc = sbrk(threshold);

				if (block_prealloc == NULL)
					return NULL;

				global_head = block_prealloc;
				global_head->next = NULL;
				global_head->prev = NULL;
				global_head->status = 1;
				global_head->size = threshold - padded_struct;
			} else if (global_head != NULL) { //daca s-a mai alocat cu mmap
				block_prealloc = sbrk(threshold);

				if (block_prealloc == NULL)
					return NULL;

				munmap(global_head, global_head->size);

				global_head = block_prealloc;
				global_head->next = NULL;
				global_head->prev = NULL;
				global_head->status = 1;
				global_head->size = threshold - padded_struct;
			}
			bool_prealloc = 0;
			return (void *)(global_head+1);
		}

		coalesce_blocks(global_head);

		if (size % 8 != 0) {
			while (size % 8 != 0)
				size++;
		}

		//caut un bloc free de size cat imi trebuie
		struct block_meta *realloc_block = get_free_block(size + padded_struct);

		struct block_meta *blocky = global_head;


		//ma pun cu blocky la ptr
		while (blocky != NULL) {
			void *block_start = (void *)(blocky + 1);

			if (ptr == block_start)
				break;

			blocky = blocky->next;
		}

		if (blocky->status == 0)
			return NULL;

		if (realloc_block != NULL) {
			if (realloc_block->size - (size + padded_struct) >= minimal_block_use) {
				memcpy((void *)(realloc_block + 1), (void *)(blocky + 1), blocky->size);
				realloc_block->size = size + padded_struct;
				blocky->status = 0;
				struct block_meta *new_rblock = (struct block_meta *)(realloc_block + size + padded_struct);

				new_rblock->size = realloc_block->size - size - padded_struct;
				new_rblock->status = 0;
				if (realloc_block->next != NULL)
					realloc_block->next->prev = new_rblock;
				new_rblock->next = realloc_block->next->next;
				realloc_block->next = new_rblock;
				new_rblock->prev = realloc_block;

				return (void *)(realloc_block + 1);
				} else if (realloc_block->size - (size + padded_struct) <= 39) { //intra la fix in cel free
					memcpy((void *)(realloc_block + 1), (void *)(blocky + 1), blocky->size);
					realloc_block->size = size + padded_struct;
					blocky->status = 0;
					return (void *)(realloc_block + 1);
					} else if ((size_t)((char *)realloc_block->next - (char *)realloc_block) >= (size + padded_struct)) {
						memcpy((void *)(realloc_block + 1), (void *)(blocky + 1), blocky->size);

						realloc_block->size = size + padded_struct;

						blocky->status = 0;

						return (void *)(blocky + 1);
					}
			}

		//daca ptr este ultimul block din lista
		if (blocky->next == NULL) {
			//dau expand daca mai trebuie size
			if (blocky->size < (size + padded_struct)) {
				size_t additional_size = size + padded_struct - blocky->size;

				blocky = expand_block(blocky, additional_size);
				blocky->size = blocky->size + additional_size;
			}
			//daca e mai mult size in bloc decat am nevoie
			if (blocky->size >= size + padded_struct) {
				//dau split daca ramane cel putin 40
				if (blocky->size >= minimal_block_use + size + padded_struct) {
					struct block_meta *new_block = (struct block_meta *)((char *)blocky + size + padded_struct);

					new_block->size = blocky->size - size - padded_struct;
					new_block->status = 0;
					new_block->prev = blocky;
					new_block->next = NULL;
					blocky->next = new_block;
					blocky->size = size + padded_struct;
				} else {
					if (blocky->size - (size + padded_struct) <= 39)
						blocky->status = 1;
					}
			}
			return (void *)(blocky + 1);
		}

		if (blocky->size >= size + padded_struct) {
			//dau split daca ramane cel putin 40
			if (blocky->size >= minimal_block_use + size + padded_struct) {
				struct block_meta *new_block = (struct block_meta *)((char *)blocky + size + padded_struct);

				new_block->size = blocky->size - size - padded_struct;
				new_block->status = 0;
				new_block->prev = blocky;
				new_block->next = NULL;
				blocky->next = new_block;
				blocky->size = size + padded_struct;
			} else {
				if (blocky->size - (size + padded_struct) <= 39)  //daca intra la fix
					blocky->status = 1;
			}
			return (void *)(blocky + 1);
		}


		if (blocky->status == 2 && size + padded_struct >= threshold) {
			struct block_meta *mmap_realloc;

			mmap_realloc = (struct block_meta *)mmap(NULL, size + padded_struct, PROT_READ | PROT_WRITE,
			MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

			memcpy((void *)(mmap_realloc + 1), (void *)(blocky + 1), blocky->size);

			mmap_realloc->size = size + padded_struct;

			munmap(blocky, blocky->size);
			//copiez dupa dau unmap
			return (void *)(mmap_realloc + 1);
		}

		if (blocky->status == 1 && size + padded_struct < threshold) {
			struct block_meta *brand_new = (struct block_meta *)sbrk(size + padded_struct);

			memcpy((void *)(brand_new + 1), (void *)(blocky + 1), blocky->size);

			brand_new->size = size + padded_struct;

			blocky->status = 0;

			return (void *)(brand_new + 1);
		}

		if (blocky->status == 1 && size + padded_struct >= threshold) {
			struct block_meta *mmap_block_realloc;

			mmap_block_realloc = (struct block_meta *)mmap(NULL, size + padded_struct, PROT_READ | PROT_WRITE,
			MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

			memcpy((void *)(mmap_block_realloc + 1), (void *)(blocky + 1), blocky->size);

			mmap_block_realloc->size = size + padded_struct;

			blocky->status = 0;

			return (void *)(mmap_block_realloc + 1);
		}

		return NULL;
	}
