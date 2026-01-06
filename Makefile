ifeq ($(HOSTTYPE),)
    HOSTTYPE := $(shell uname -m)_$(shell uname -s)
endif

NAME = libft_malloc_$(HOSTTYPE).so
SLINK = libft_malloc.so

CC = cc
CFLAGS = -Wall -Wextra -Werror -O3 -fPIC
LDFLAGS = -shared

SRCDIR = src
INCDIR = inc
LIBFTDIR = libft

SRCS = $(SRCDIR)/malloc.c $(SRCDIR)/free.c $(SRCDIR)/realloc.c $(SRCDIR)/show_alloc_mem.c $(SRCDIR)/utils.c
OBJDIR = obj
OBJECTS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRCS))

# Evaluation variables
EVAL_DIR = test/eval
EVAL_SRCS = $(wildcard $(EVAL_DIR)/*.c)
EVAL_BINS = $(EVAL_SRCS:.c=)
EVAL_LOG = test/eval.log
EVAL_TESTS = test_base test_malloc test_free test_realloc1 test_realloc2 test_error_handling test_show
COMPARE_TESTS = test_base test_malloc test_free
TIME_CMD = /usr/bin/time -f "%C\n\tPeak RSS: %M KB\n\tMinor page faults: %R\n\tPage Size: %Z bytes"

all: $(NAME)

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(INCDIR)/malloc.h Makefile
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -I$(INCDIR) -I$(LIBFTDIR)/inc -c $< -o $@

# Pattern rule for evaluation tests
$(EVAL_DIR)/%: $(EVAL_DIR)/%.c $(NAME)
	@mkdir -p $(@D)
	$(CC) -Wall -Wextra -I$(INCDIR) -I$(LIBFTDIR)/inc $< $(if $(filter test_show.c,$(notdir $<)),-L. -lft_malloc,) -o $@

libft:
	$(MAKE) -C $(LIBFTDIR)

$(NAME): libft $(OBJECTS)
	$(CC) $(LDFLAGS) -o $(NAME) $(OBJECTS) $(LIBFTDIR)/libft.a
	@ln -sf $(NAME) $(SLINK)

test: $(NAME)
	@echo "Compiling tests..."
	$(CC) -Wall -Wextra -Iinc -Ilibft/inc test/test_main.c $(LIBFTDIR)/libft.a $(SLINK) -o test/test_main
	@echo "Running tests..."
	LD_PRELOAD=./libft_malloc.so LD_LIBRARY_PATH=. ./test/test_main

evaluate: $(EVAL_BINS)
	@echo "Running evaluation tests..."
	@printf "" > $(EVAL_LOG)
	@for bin in $(EVAL_TESTS); do \
		printf -- "--- [%s] ---\n" "$$bin" >> $(EVAL_LOG); \
		if echo "$(COMPARE_TESTS)" | grep -qw "$$bin"; then \
			printf -- "System malloc:\n" >> $(EVAL_LOG); \
			(cd $(EVAL_DIR) && $(TIME_CMD) ./$$bin) >> $(EVAL_LOG) 2>&1; \
			printf "\n" >> $(EVAL_LOG); \
		fi; \
		printf -- "Custom malloc:\n" >> $(EVAL_LOG); \
		(cd $(EVAL_DIR) && LD_PRELOAD=../../$(SLINK) LD_LIBRARY_PATH=../../ $(TIME_CMD) ./$$bin) >> $(EVAL_LOG) 2>&1; \
		printf "\n" >> $(EVAL_LOG); \
	done
	@echo "Evaluation complete. Log saved to $(EVAL_LOG)"

clean:
	@rm -rf $(OBJDIR)
	@$(MAKE) -C $(LIBFTDIR) clean

fclean: clean
	@rm -f $(NAME) $(SLINK)
	@rm -f $(NAME) test/test_main
	@$(MAKE) -C $(LIBFTDIR) fclean
	@rm -rf $(EVAL_BINS)
	@rm -f $(EVAL_LOG)

re: fclean all

.PHONY: all test clean fclean re libft evaluate
