#define _XOPEN_SOURCE

#include <ucontext.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define handle_error(msg) \
	do {perror(msg); exit(EXIT_FAILURE); } while (0)


// function pointer
typedef void(*coroutine_body_t)(void*);

typedef struct Coroutine
{
	ucontext_t* ctx;
	char func_stack[16384];
	void* param;
	bool is_finished;
	struct Coroutine* next;
} coroutine_t;

typedef struct Registry
{
	char func_stack[16384];
	ucontext_t* ctx;
	coroutine_t* cur_coroutine;
	coroutine_t* coroutines;
} co_registry_t;

static co_registry_t* registry;
static ucontext_t main_ctx;

/* func for main context control */
static void
main_ctx_function()
{
	printf("main context: get control\n");
	if (!registry->cur_coroutine) return;
	// while loop
	while (1)
	{
		// find the next coroutine
		coroutine_t* cur = registry->cur_coroutine;
		coroutine_t* ptr = cur->next;

		while (ptr->is_finished)
		{
			ptr = ptr->next;
			if (ptr == cur) break;
		}

		// all task finished
		if ((cur->is_finished) && (ptr == cur)) break;

		registry->cur_coroutine = ptr;

		printf("main coroutine: switch\n");
		if ( swapcontext(registry->ctx, registry->cur_coroutine->ctx) == -1)
			handle_error("swapcontext");
	}
}

/* create a new coroutine for a new task */
coroutine_t* 
create_coroutine(coroutine_body_t func, void* param)
{
	coroutine_t* p;
	p = malloc(sizeof(coroutine_t));
	p->ctx = malloc(sizeof(ucontext_t));

	// set up the created coroutine
	if (getcontext(p->ctx) == -1)
		handle_error("getcontext");

	p->ctx->uc_stack.ss_sp = p->func_stack;
	p->ctx->uc_stack.ss_size = sizeof(p->func_stack);
	p->param = param;
	p->is_finished = false;
	p->next = NULL;

	makecontext(p->ctx, func, 0);

	return p;
}

co_registry_t*
create_co_registry()
{
	co_registry_t* p;
	p = malloc(sizeof(co_registry_t));
	p->ctx = malloc(sizeof(ucontext_t));
	p->cur_coroutine = malloc(sizeof(coroutine_t));

	if (getcontext(p->ctx) == -1)
		handle_error("getcontext");

	p->ctx->uc_stack.ss_sp = p->func_stack;
	p->ctx->uc_stack.ss_size = sizeof(p->func_stack);
	/* p->ctx->uc_link = NULL; */

	makecontext(p->ctx, main_ctx_function, 0);

	return p;
}

/* return the control to the administer, and 
 * determine which would be the next context 
 * by the administer. 
*/
void 
yield()
{
	/* setcontext(registry->ctx); */
	swapcontext(registry->cur_coroutine->ctx, registry->ctx);
}



void
register_coroutine(coroutine_t* coroutine)
{
	if (!registry->coroutines)
	{
		registry->coroutines = coroutine;
		registry->cur_coroutine = coroutine;
	}
	else
	{
		coroutine_t* ptr = registry->coroutines;
		while(ptr->next != NULL) ptr = ptr->next;
		ptr->next = coroutine;
	}
}

static void
func1(void)
{
	printf("func1: started\n");
	for (int i = 0; i < 10; i++)
	{
		printf("func1: %d\n", i);
		yield();
	}
}

static void
func2(void)
{
	printf("func2: started\n");
	for (int i = 0; i < 10; i++)
	{
		printf("func2: %d\n", i);
		yield();
	}
}

int
main()
{
	registry = create_co_registry();

	printf("creating coroutines...\n");
	coroutine_t* co_1 = create_coroutine(func1, (void*) 0);
	coroutine_t* co_2 = create_coroutine(func2, (void*) 0);

	printf("registering coroutines...\n");
	register_coroutine(co_1);
	register_coroutine(co_2);
	co_2->next = co_1;

	printf("starting coroutines...\n");
	if (swapcontext(&main_ctx, registry->ctx) == -1)
		handle_error("swapcontext");

	free(registry);
	free(co_1);
	free(co_2);
}
