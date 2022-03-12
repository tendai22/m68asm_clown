#include <stdio.h>
#include <stdlib.h>

#include "clowncommon.h"

#include "syntactic.h"
#define YY_NO_UNISTD_H
#include "lexical.h"
#include "semantic.h"

#define ERROR(message) do { fputs("Error: " message "\n", stderr); exit_code = EXIT_FAILURE; } while (0)

/* TODO - Stupid hack */
extern StatementListNode *statement_list_head;

void m68kasm_error(void *scanner, const char *message)
{
	(void)scanner;

	fprintf(stderr, "Error : Exiting %s\n", message);
}

int main(int argc, char **argv)
{
	int exit_code = EXIT_SUCCESS;

	if (argc < 2)
	{
		ERROR("Input file must be passed as a parameter");
	}
	else
	{
		FILE *file = fopen(argv[1], "r");

		if (file == NULL)
		{
			ERROR("Could not open input file");
		}
		else
		{
			yyscan_t flex_state;
			if (m68kasm_lex_init(&flex_state) != 0)
			{
				ERROR("yylex_init failed");
			}
			else
			{
				m68kasm_set_in(file, flex_state);

				/*m68kasm_lex(); */

			#if M68KASM_DEBUG
				m68kasm_debug = 1;
			#endif

				if (m68kasm_parse(flex_state) != 0)
					exit_code = EXIT_FAILURE;

				if (m68kasm_lex_destroy(flex_state) != 0)
					ERROR("yylex_destroy failed");

				fclose(file);

				if (!ProcessParseTree(statement_list_head))
					exit_code = EXIT_FAILURE;
			}
		}
	}

	return exit_code;
}
