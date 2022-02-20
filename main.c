#include <stdio.h>
#include <stdlib.h>

#include "clowncommon.h"

#include "types.h"

#define YY_NO_UNISTD_H
#include "lexer.h"
#include "parser.h"

#define ERROR(message) do { fputs("Error: " message "\n", stderr); exit_code = EXIT_FAILURE; } while (0)

/* TODO - Stupid hack */
extern StatementListNode *statement_list_head;

int yywrap(void)
{
	return 1;
}

static unsigned int ConstructEffectiveAddressBits(const Operand *operand)
{
	unsigned int m, xn;

	switch (operand->type)
	{
		case OPERAND_TYPE_DATA_REGISTER:
			m = 0; /* 000 */
			xn = operand->data.data_register;
			break;

		case OPERAND_TYPE_ADDRESS_REGISTER:
			m = 1; /* 001 */
			xn = operand->data.address_register;
			break;

		case OPERAND_TYPE_ADDRESS:
			m = 0x7; /* 111 */

			switch (operand->data.address.size)
			{
				case TOKEN_SIZE_WORD:
					xn = 0;  /* 000 */
					break;

				default:
					fprintf(stderr, "Error: Absolute address can only be word- or longword-sized - assuming longword\n");
					/* Fallthrough */
				case TOKEN_SIZE_LONG:
					xn = 1;  /* 001 */
					break;
#if 0
				case -1:
					/* Automatically determine size */
					if (operand->data.address.value
					break;
#endif
			}

			break;

		case OPERAND_TYPE_LITERAL:
			m = 0x7; /* 111 */
			xn = 4;  /* 100 */
			break;

		default:
			fprintf(stderr, "Error: Invalid operand type - USP, SR, and CCR cannot be used here\n");
			/* Just pretend it's data register 0 to keep things moving along. */
			m = 0;
			xn = 0;
			break;
	}

	return (m << 3) | (xn << 0);
}

static unsigned long ResolveValue(const Value *value)
{
	if (value->type != TOKEN_NUMBER)
	{
		fprintf(stderr, "Error: Unable to resolve this type of Value right now\n");
		return 0;
	}
	else
	{
		return value->data.integer;
	}
}

static void OutputOperands(FILE *file, const Instruction *instruction)
{
	const Operand *operand;

	for (operand = instruction->operands; operand != NULL; operand = operand->next)
	{
		unsigned int i;

		switch (operand->type)
		{
			case OPERAND_TYPE_ADDRESS:
			{
				const unsigned long value = ResolveValue(&operand->data.address.value);

				for (i = operand->data.address.size == TOKEN_SIZE_LONG ? 4 : 2; i-- > 0; )
					fputc((value >> (8 * i)) & 0xFF, file);

				break;
			}

			case OPERAND_TYPE_LITERAL:
			{
				const unsigned long value = ResolveValue(&operand->data.literal);

				for (i = instruction->opcode.size == TOKEN_SIZE_LONG ? 4 : 2; i-- > 0; )
					fputc((value >> (8 * i)) & 0xFF, file);

				break;
			}

			default:
				break;
		}
	}
}
/*
static cc_bool OperandIsUnusual(const Operand *operand)
{
	switch (operand->type)
	{
		case OPERAND_TYPE_DATA_REGISTER:
		case OPERAND_TYPE_ADDRESS_REGISTER:
		case OPERAND_TYPE_ADDRESS:
		case OPERAND_TYPE_LITERAL:
			return cc_false;

		case OPERAND_TYPE_STATUS_REGISTER:
		case OPERAND_TYPE_CONDITION_CODE_REGISTER:
		case OPERAND_TYPE_USER_STACK_REGISTER:
			return cc_true;
	}
}
*/

static unsigned int ToAlternateEffectiveAddressBits(unsigned int bits)
{
	const unsigned int m = (bits >> 3) & 7;
	const unsigned int dn = (bits >> 0) & 7;

	return (m << 6) | (dn << 9);
}

static cc_bool AssembleInstruction(FILE *file, const Instruction *instruction)
{
	unsigned int total_operands;
	const Operand *operand;
	unsigned int machine_code;
	unsigned int i;

	cc_bool success = cc_true;

	/* Count operands. */
	total_operands = 0;

	for (operand = instruction->operands; operand != NULL; operand = operand->next)
		++total_operands;

	/* Determine the machine code for the opcode and perform sanity-checking. */
	switch (instruction->opcode.type)
	{
		case TOKEN_OPCODE_MOVE:
			if (total_operands != 2)
			{
				fprintf(stderr, "Error: 'MOVE' instruction must have two operands\n");
				success = cc_false;
			}
			else
			{
				const Operand* const source_operand = instruction->operands;
				const Operand* const destination_operand = instruction->operands->next;

				if (source_operand->type == OPERAND_TYPE_USER_STACK_POINTER_REGISTER || destination_operand->type == OPERAND_TYPE_USER_STACK_POINTER_REGISTER)
				{
					/* MOVE USP */

					/* Default to address register 0, in case the other operand isn't an address register. */
					unsigned int address_register = 0;

					const cc_bool from_usp_to_address_register = source_operand->type == OPERAND_TYPE_USER_STACK_POINTER_REGISTER;

					/* Check that the opcode is the right size. */
					if (instruction->opcode.size != TOKEN_SIZE_LONG && instruction->opcode.size != -1)
						fprintf(stderr, "Warning: 'MOVE USP' instruction can only be longword-sized - the specified size will be ignored\n");

					/* Handle operands and perform validation. */
					if (from_usp_to_address_register)
					{
						if (destination_operand->type != OPERAND_TYPE_ADDRESS_REGISTER)
						{

							fprintf(stderr, "Error: 'MOVE FROM USP' instruction's destination operand must be an address register\n");
							success = cc_false;
						}
						else
						{
							address_register = destination_operand->data.address_register;
						}
					}
					else
					{
						if (source_operand->type != OPERAND_TYPE_ADDRESS_REGISTER)
						{
							fprintf(stderr, "Error: 'MOVE TO USP' instruction's source operand must be an address register\n");
							success = cc_false;
						}
						else
						{
							address_register = source_operand->data.address_register;
						}
					}

					/* Produce the machine code for this instruction. */
					machine_code = 0x4E60 | (from_usp_to_address_register << 3) | address_register;
				}
				else if (source_operand->type == OPERAND_TYPE_STATUS_REGISTER || destination_operand->type == OPERAND_TYPE_STATUS_REGISTER)
				{
					/* MOVE TO SR */
					/* MOVE FROM SR */
					const cc_bool from_sr = source_operand->type == OPERAND_TYPE_STATUS_REGISTER;

					if (instruction->opcode.size != TOKEN_SIZE_WORD && instruction->opcode.size != -1)
						fprintf(stderr, "Warning: 'MOVE SR' instruction can only be word-sized - the specified size will be ignored\n");

					if (from_sr)
					{
						/* MOVE FROM SR */
						if (destination_operand->type == OPERAND_TYPE_ADDRESS_REGISTER)
						{
							fprintf(stderr, "Error: 'MOVE FROM SR' instruction's destination operand cannot be an address register\n");
							success = cc_false;
						}

						machine_code = 0x40C0 | ConstructEffectiveAddressBits(destination_operand);
					}
					else
					{
						/* MOVE TO SR */
						if (source_operand->type == OPERAND_TYPE_ADDRESS_REGISTER)
						{
							fprintf(stderr, "Error: 'MOVE TO SR' instruction's source operand cannot be an address register\n");
							success = cc_false;
						}

						machine_code = 0x46C0 | ConstructEffectiveAddressBits(source_operand);
					}
				}
				else if (destination_operand->type == OPERAND_TYPE_CONDITION_CODE_REGISTER)
				{
					/* MOVE TO CCR */
					if (instruction->opcode.size != TOKEN_SIZE_WORD && instruction->opcode.size != -1)
						fprintf(stderr, "Warning: 'MOVE TO CCR' instruction can only be word-sized - the specified size will be ignored\n");

					if (source_operand->type == OPERAND_TYPE_ADDRESS_REGISTER)
					{
						fprintf(stderr, "Error: 'MOVE TO CCR' instruction's source operand cannot be an address register\n");
						success = cc_false;
					}

					machine_code = 0x44C0 | ConstructEffectiveAddressBits(source_operand);
				}
				else
				{
					/* MOVE */
					if (destination_operand->type == OPERAND_TYPE_ADDRESS_REGISTER)
					{
						/* MOVEA mistyped as MOVE */
						fprintf(stderr, "Error: a 'MOVE' instruction cannot move to an address register - you probably meant to use the MOVEA instruction\n");
						success = cc_false;
					}
					else if (destination_operand->type == OPERAND_TYPE_LITERAL)
					{
						fprintf(stderr, "Error: a 'MOVE' instruction's destination operand cannot be a literal\n");
						success = cc_false;
					}
					else if (destination_operand->type == OPERAND_TYPE_PROGRAM_COUNTER_WITH_DISPLACEMENT || destination_operand->type == OPERAND_TYPE_PROGRAM_COUNTER_WITH_DISPLACEMENT_AND_INDEX_REGISTER)
					{
						fprintf(stderr, "Error: a 'MOVE' instruction's destination operand cannot be PC-relative\n");
						success = cc_false;
					}

					switch (instruction->opcode.size)
					{
						case TOKEN_SIZE_BYTE:
							machine_code = 0x1000;
							break;

						case -1:
							fprintf(stderr, "Error: 'MOVE' instruction needs an explicit size\n");
							success = cc_false;
							/* Fallthrough */
						case TOKEN_SIZE_WORD:
							machine_code = 0x3000;
							break;

						case TOKEN_SIZE_LONG:
							machine_code = 0x2000;
							break;
					}

					machine_code |= ConstructEffectiveAddressBits(source_operand);
					machine_code |= ToAlternateEffectiveAddressBits(ConstructEffectiveAddressBits(destination_operand));
				}
			}

			break;

		case TOKEN_OPCODE_ADD:
			/* TODO */
			machine_code = 0x4E71;
			break;

		default:
			fprintf(stderr, "Internal error: Unrecognised instruction\n");
			success = cc_false;
			/* Just insert a NOP. */
			machine_code = 0x4E71;
			break;
	}
	fprintf(stderr, "machine code: 0x%X\n", machine_code);
	/* Output the machine code for the opcode. */
	for (i = 2; i-- > 0; )
		fputc((machine_code >> (8 * i)) & 0xFF, file);

	/* Output the data for the operands. */
	OutputOperands(file, instruction);

	return success;
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
		yyin = fopen(argv[1], "r");

		if (yyin == NULL)
		{
			ERROR("Could not open input file");
		}
		else
		{
			/*yylex(); */

		#if YYDEBUG
			yydebug = 1;
		#endif

			(void)yyparse();

			fclose(yyin);

			/* Parse the parse tree */
			{
				FILE *output_file = fopen("output.bin", "wb");

				if (output_file == NULL)
				{
					ERROR("Could not open output file");
				}
				else
				{
					StatementListNode *statement_list_node;

					for (statement_list_node = statement_list_head; statement_list_node != NULL; statement_list_node = statement_list_node->next)
					{
						fprintf(stderr, "What do we have here? A statement of type %d!\n", statement_list_node->statement.type);

						switch (statement_list_node->statement.type)
						{
							case STATEMENT_TYPE_EMPTY:
								fprintf(stderr, "  What do we have here? An empty statement!\n");
								break;

							case STATEMENT_TYPE_INSTRUCTION:
								fprintf(stderr, "  What do we have here? An instruction of type %d!\n", statement_list_node->statement.data.instruction.opcode.type);

								if (!AssembleInstruction(output_file, &statement_list_node->statement.data.instruction))
								{
									exit_code = EXIT_FAILURE;
								}

								break;

							case STATEMENT_TYPE_MACRO:
								fprintf(stderr, "  What do we have here? A macro!\n");
								break;
						}
					}

					fclose(output_file);
				}
			}
		}
	}

	return exit_code;
}

void yyerror(char *s)
{
    fprintf(stderr, "Error : Exiting %s\n", s);
}
