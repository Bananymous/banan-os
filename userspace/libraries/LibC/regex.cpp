#include <assert.h>
#include <ctype.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void regfree_element(_regex_elem_t* element)
{
	switch (element->type)
	{
		case _re_anchor_begin:
		case _re_anchor_end:
		case _re_group_tag1:
		case _re_group_tag2:
		case _re_literal:
			break;
		case _re_group:
			for (size_t i = 0; i < element->as.group.elements_len; i++)
				regfree_element(&element->as.group.elements[i]);
			free(element->as.group.elements);
			break;
	}
}

static bool append_element(_regex_elem_t* group, _regex_elem_t new_element)
{
	assert(group->type == _re_group);

	_regex_elem_t* new_elements = static_cast<_regex_elem_t*>(realloc(
		group->as.group.elements,
		(group->as.group.elements_len + 1) * sizeof(_regex_elem_t)
	));
	if (new_elements == nullptr)
		return false;

	group->as.group.elements = new_elements;
	group->as.group.elements[group->as.group.elements_len] = new_element;
	group->as.group.elements_len++;
	return true;
}

static constexpr void literal_init(uint32_t literal[8])
{
	for (size_t i = 0; i < 8; i++)
		literal[i] = 0;
}

static constexpr void literal_add(uint32_t literal[8], char ch)
{
	const uint8_t u8 = ch;
	literal[u8 / 32] |= static_cast<uint32_t>(1) << (u8 % 32);
}

static constexpr void literal_invert(uint32_t literal[8])
{
	for (size_t i = 0; i < 8; i++)
		literal[i] ^= 0xFFFFFFFF;
}

static constexpr bool literal_has(const uint32_t literal[8], char ch)
{
	const uint8_t u8 = ch;
	return literal[u8 / 32] & (static_cast<uint32_t>(1) << (u8 % 32));
}

static size_t parse_bracket_expression(const char* expression, uint32_t literal[8], bool is_case_insensitive)
{
	assert(*expression == '[');

	literal_init(literal);

	size_t index = 1;

	const bool is_inverse = (expression[index] == '^');
	if (is_inverse)
		index++;

	if (expression[index] == ']')
	{
		literal_add(literal, ']');
		index++;
	}

	const auto add_character =
		[literal, is_case_insensitive](uint8_t ch)
		{
			if (!is_case_insensitive)
				literal_add(literal, ch);
			else
			{
				literal_add(literal, toupper(ch));
				literal_add(literal, tolower(ch));
			}
		};

	while (expression[index] && expression[index] != ']')
	{
		if (expression[index] == '[')
		{
			bool sequence = false;
			switch (expression[index + 1])
			{
				case '.': case ':': case '=':
					sequence = true;
					break;
			}

			if (sequence)
			{
				const char target = expression[index + 1];
				index += 2;

				size_t len = 0;
				while (expression[index + len] && expression[index + len] != target && expression[index + len + 1] != ']')
					len++;
				if (expression[index + len] == '\0')
					return -REG_EBRACK;

				switch (target)
				{
					case '.':
						fprintf(stddbg, "regcomp: collating elements are not supported\n");
						return -REG_ECOLLATE;
					case '=':
						fprintf(stddbg, "regcomp: equivalence classes are not supported\n");
						if (len == 1)
							add_character(expression[index]);
						break;
					case ':':
						int (*func)(int) = nullptr;
						if (false);
#define CHECK_CHARACTER_CLASS(name) else if (strncmp(#name, expression + index, len) == 0) func = is##name
						CHECK_CHARACTER_CLASS(alnum);
						CHECK_CHARACTER_CLASS(alpha);
						CHECK_CHARACTER_CLASS(blank);
						CHECK_CHARACTER_CLASS(cntrl);
						CHECK_CHARACTER_CLASS(digit);
						CHECK_CHARACTER_CLASS(graph);
						CHECK_CHARACTER_CLASS(lower);
						CHECK_CHARACTER_CLASS(print);
						CHECK_CHARACTER_CLASS(punct);
						CHECK_CHARACTER_CLASS(space);
						CHECK_CHARACTER_CLASS(upper);
						CHECK_CHARACTER_CLASS(xdigit);
#undef CHECK_CHARACTER_CLASS
						if (func == nullptr)
						{
							fprintf(stddbg, "regcomp: unrecognized character class '%.*s'\n", static_cast<int>(len), expression);
							return -REG_ECTYPE;
						}
						for (int ch = 0; ch < 0x100; ch++)
							if (func(ch))
								add_character(ch);
						break;
				}

				index += len + 2;
				continue;
			}
		}

		if (expression[index + 1] == '-' && expression[index + 2] != ']')
		{
			const int ch_s = expression[index];
			const int ch_e = expression[index + 2];
			for (int ch = ch_s; ch <= ch_e; ch++)
				add_character(ch);
			index += 3;
			continue;
		}

		add_character(expression[index]);
		index++;
	}

	if (expression[index] != ']')
		return -REG_EBRACK;

	if (is_inverse)
		literal_invert(literal);
	return index + 1;
}

int regcomp(regex_t* __restrict preg, const char* __restrict pattern, int cflags)
{
#define REGCOMP_ERROR(err) do { error = err; goto regcomp_error; } while (false)
	int error = 0;

	const bool is_extended = (cflags & REG_EXTENDED);
	const bool is_case_insensitive = (cflags & REG_ICASE);

	const auto can_add_qualifier =
		[](const _regex_elem_t& elem) -> bool
		{
			if (elem.type == _re_anchor_begin || elem.type == _re_anchor_end)
				return false;
			return elem.qualifier.min == 1 && elem.qualifier.max == 1;
		};

	size_t stack_len = 1;
	_regex_elem_t** stack = static_cast<_regex_elem_t**>(malloc(sizeof(_regex_elem_t*)));
	if (stack == nullptr)
		REGCOMP_ERROR(REG_ESPACE);

	stack[0] = static_cast<_regex_elem_t*>(malloc(sizeof(_regex_elem_t)));
	if (stack[0] == nullptr)
		REGCOMP_ERROR(REG_ESPACE);
	stack[0][0] = { _re_group, { 1, 1 }, { .group = { nullptr, 0, 0 } } };

	*preg = {
		.re_nsub = 0,
		._compiled = nullptr,
		._compiled_len = 0,
		._cflags = cflags,
	};

	for (size_t i = 0; pattern[i]; i++)
	{
		auto* current = stack[stack_len - 1];
		assert(current->type == _re_group);

		switch (pattern[i])
		{
			case '.':
				if (!append_element(current, { _re_literal, { 1, 1 }, {} }))
					REGCOMP_ERROR(REG_ESPACE);
				literal_init(current->as.group.elements[current->as.group.elements_len - 1].as.literal);
				literal_add(current->as.group.elements[current->as.group.elements_len - 1].as.literal, '\n');
				literal_invert(current->as.group.elements[current->as.group.elements_len - 1].as.literal);
				break;
			case '*':
				if (current->as.group.elements_len == 0 || current->as.group.elements[current->as.group.elements_len - 1].type == _re_anchor_begin)
					goto case_default; // valid in BRE, UB in ERE
				if (!can_add_qualifier(current->as.group.elements[current->as.group.elements_len - 1]))
					REGCOMP_ERROR(REG_BADRPT);
				current->as.group.elements[current->as.group.elements_len - 1].qualifier = { 0, RE_DUP_MAX };
				break;
			case '?':
				if (!can_add_qualifier(current->as.group.elements[current->as.group.elements_len - 1]))
					REGCOMP_ERROR(REG_BADRPT);
				current->as.group.elements[current->as.group.elements_len - 1].qualifier = { 0, 1 };
				break;
			case '+':
				if (!can_add_qualifier(current->as.group.elements[current->as.group.elements_len - 1]))
					REGCOMP_ERROR(REG_BADRPT);
				current->as.group.elements[current->as.group.elements_len - 1].qualifier = { 1, RE_DUP_MAX };
				break;
			case '(':
				if (!is_extended)
					goto case_default;
			case_openparen:
			{
				_regex_elem_t** new_stack = static_cast<_regex_elem_t**>(realloc(stack, (stack_len + 1) * sizeof(_regex_elem_t*)));
				_regex_elem_t* new_element = static_cast<_regex_elem_t*>(malloc(sizeof(_regex_elem_t)));
				if (new_stack == nullptr || new_element == nullptr)
				{
					free(new_stack);
					free(new_element);
					REGCOMP_ERROR(REG_ESPACE);
				}
				stack = new_stack;
				stack[stack_len] = new_element;
				stack[stack_len][0] = { _re_group, { 1, 1 }, { .group = { nullptr, 0, ++preg->re_nsub }} };
				stack_len++;
				break;
			}
			case ')':
				if (!is_extended)
					goto case_default;
			case_closeparen:
			{
				if (stack_len <= 1)
					REGCOMP_ERROR(REG_EPAREN);
				auto* prev = stack[stack_len - 2];
				_regex_elem_t* new_elements = static_cast<_regex_elem_t*>(realloc(
					prev->as.group.elements,
					(prev->as.group.elements_len + 1) * sizeof(_regex_elem_t)
				));
				if (new_elements == nullptr)
					REGCOMP_ERROR(REG_ESPACE);
				prev->as.group.elements = new_elements;
				prev->as.group.elements[prev->as.group.elements_len] = *current;
				prev->as.group.elements_len++;
				free(current);
				stack_len--;
				break;
			}
			case '[':
			{
				if (!append_element(current, { _re_literal, { 1, 1 }, {} }))
					REGCOMP_ERROR(REG_ESPACE);
				ssize_t consumed = parse_bracket_expression(pattern + i, current->as.group.elements[current->as.group.elements_len - 1].as.literal, is_case_insensitive);
				if (consumed <= 0)
					REGCOMP_ERROR(-consumed);
				i += consumed - 1;
				break;
			}
			case '{':
				if (!is_extended)
					goto case_default;
			case_openbrace:
				if (!can_add_qualifier(current->as.group.elements[current->as.group.elements_len - 1]))
					REGCOMP_ERROR(REG_BADRPT);
			{
				i++;
				int min = 0, max = RE_DUP_MAX;
				bool found_comma = false;
				while (pattern[i] && pattern[i] != (is_extended ? '}' : '\\'))
				{
					if (pattern[i] == ',')
					{
						if (found_comma)
							REGCOMP_ERROR(REG_BADPAT);
						found_comma = true;
						if (isdigit(pattern[i + 1]))
							max = 0;
					}
					else if (isdigit(pattern[i]))
					{
						auto& val = found_comma ? max : min;
						val = (val * 10) + (pattern[i] - '0');
						if (val > RE_DUP_MAX)
							REGCOMP_ERROR(REG_ERANGE);
					}
					else
					{
						REGCOMP_ERROR(REG_BADPAT);
					}
					i++;
				}
				if (pattern[i] == '\0')
					REGCOMP_ERROR(REG_EBRACE);
				if (!is_extended && pattern[++i] != '}')
					REGCOMP_ERROR(REG_BADPAT);
				if (!found_comma)
					max = min;
				if (max < min)
					REGCOMP_ERROR(REG_ERANGE);
				current->as.group.elements[current->as.group.elements_len - 1].qualifier = { min, max };
				break;
			}
			case '|':
				if (!is_extended)
					goto case_default;
				fprintf(stddbg, "regcomp: alternations are not supported");
				REGCOMP_ERROR(REG_BADPAT);
			case '^':
				if (!is_extended && i != 0)
					goto case_default;
				if (!append_element(current, { _re_anchor_begin, { 1, 1 }, {} }))
					REGCOMP_ERROR(REG_ESPACE);
				break;
			case '$':
				if (!is_extended && pattern[i + 1] != '\0')
					goto case_default;
				if (!append_element(current, { _re_anchor_end, { 1, 1 }, {} }))
					REGCOMP_ERROR(REG_ESPACE);
				break;
			case '\\':
				i++;
				if (pattern[i] == '\0')
					REGCOMP_ERROR(REG_EESCAPE);
				if (is_extended)
					goto case_default;
				switch (pattern[i])
				{
					case '(': goto case_openparen;
					case ')': goto case_closeparen;
					case '{': goto case_openbrace;
					case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
						fprintf(stddbg, "regcomp: back-references are not supported");
						REGCOMP_ERROR(REG_BADPAT);
				}
				[[fallthrough]];
			default:
			case_default:
				if (!append_element(current, { _re_literal, { 1, 1 }, {} }))
					REGCOMP_ERROR(REG_ESPACE);
				literal_init(current->as.group.elements[current->as.group.elements_len - 1].as.literal);
				if (!is_case_insensitive)
					literal_add(current->as.group.elements[current->as.group.elements_len - 1].as.literal, pattern[i]);
				else
				{
					literal_add(current->as.group.elements[current->as.group.elements_len - 1].as.literal, toupper(pattern[i]));
					literal_add(current->as.group.elements[current->as.group.elements_len - 1].as.literal, tolower(pattern[i]));
				}
				break;
		}
	}

	if (stack_len != 1)
		REGCOMP_ERROR(REG_EPAREN);

	preg->_compiled     = stack[0]->as.group.elements;
	preg->_compiled_len = stack[0]->as.group.elements_len;

	free(stack[0]);
	free(stack);

	return 0;

regcomp_error:
	for (size_t i = 0; i < stack_len; i++)
		regfree_element(stack[i]);
	free(stack);
	return error;

#undef REGCOMP_ERROR
}

struct regexec_result_t
{
	bool matched;
	bool out_of_memory;
	size_t length;
};

static regexec_result_t match_expr(const _regex_elem_t* elements, size_t elements_len, const char* string, size_t length, size_t nmatch, regmatch_t pmatch[], int eflags)
{
	if (elements_len == 0)
		return { true, false, length };

	// match fixed length elements
	if (elements[0].qualifier.min == elements[0].qualifier.max)
	{
		int qualifier = elements[0].qualifier.min;
		if (qualifier == 0)
			return match_expr(elements + 1, elements_len - 1, string, length, nmatch, pmatch, eflags);

		switch (elements[0].type)
		{
			case _re_anchor_begin:
				if ((eflags & REG_NOTBOL) || length > 0)
					return { false, false, 0 };
				return match_expr(elements + 1, elements_len - 1, string, length, nmatch, pmatch, eflags);

			case _re_anchor_end:
				if ((eflags & REG_NOTEOL) || string[length])
					return { false, false, 0 };
				return match_expr(elements + 1, elements_len - 1, string, length, nmatch, pmatch, eflags);

			case _re_group_tag1:
				if (const size_t index = elements[0].as.group_tag; index < nmatch)
					pmatch[index].rm_so = length;
				return match_expr(elements + 1, elements_len - 1, string, length, nmatch, pmatch, eflags);

			case _re_group_tag2:
				if (const size_t index = elements[0].as.group_tag; index < nmatch)
					pmatch[index].rm_eo = length;
				return match_expr(elements + 1, elements_len - 1, string, length, nmatch, pmatch, eflags);

			case _re_literal:
				for (ssize_t i = 0; i < qualifier; i++)
				{
					if (string[length + i] == '\0')
						return { false, false, 0 };
					if (!literal_has(elements[0].as.literal, string[length + i]))
						return { false, false, 0 };
				}
				return match_expr(elements + 1, elements_len - 1, string, length + qualifier, nmatch, pmatch, eflags);

			case _re_group:
				const size_t attempt_len = qualifier * elements[0].as.group.elements_len + 2 + elements_len - 1;
				_regex_elem_t* attempt = static_cast<_regex_elem_t*>(malloc(attempt_len * sizeof(_regex_elem_t)));
				if (attempt == nullptr)
					return { false, true, 0 };

				size_t index = 0;
				for (ssize_t i = 0; i < qualifier; i++)
				{
					if (i + 1 == qualifier)
						attempt[index++] = { _re_group_tag1, { 1, 1 }, { .group_tag = elements[0].as.group.index } };
					for (size_t j = 0; j < elements[0].as.group.elements_len; j++)
						attempt[index++] = elements[0].as.group.elements[j];
					if (i + 1 == qualifier)
						attempt[index++] = { _re_group_tag2, { 1, 1 }, { .group_tag = elements[0].as.group.index } };
				}
				for (size_t i = 1; i < elements_len; i++)
					attempt[index++] = elements[i];

				const auto result = match_expr(attempt, attempt_len, string, length, nmatch, pmatch, eflags);
				free(attempt);

				if (const size_t index = elements[0].as.group.index; !result.matched && index < nmatch)
					pmatch[index] = { -1, -1 };
				return result;
		}
	}

	// binary search upper bound for qualifier
	int qualifier_min = elements[0].qualifier.min - 1;
	int qualifier_max = elements[0].qualifier.max;
	while (qualifier_min < qualifier_max)
	{
		const int qualifier = (qualifier_min + qualifier_max) / 2 + 1;

		const _regex_elem_t dummy {
			.type      = elements[0].type,
			.qualifier = { qualifier, qualifier },
			.as        = elements[0].as
		};

		const auto result = match_expr(&dummy, 1, string, length, 0, nullptr, eflags);
		if (result.out_of_memory)
			return regexec_result_t { false, true, 0 };

		if (result.matched)
			qualifier_min = qualifier;
		else
			qualifier_max = qualifier - 1;
	}

	// greedy match longest working qualifier
	for (int qualifier = qualifier_min; qualifier >= elements[0].qualifier.min; qualifier--)
	{
		const _regex_elem_t dummy {
			.type      = elements[0].type,
			.qualifier = { qualifier, qualifier },
			.as        = elements[0].as
		};

		_regex_elem_t* attempt = static_cast<_regex_elem_t*>(malloc(elements_len * sizeof(_regex_elem_t)));
		if (attempt == nullptr)
			return regexec_result_t { false, true, 0 };

		attempt[0] = dummy;
		for (size_t i = 1; i < elements_len; i++)
			attempt[i] = elements[i];

		const auto result = match_expr(attempt, elements_len, string, length, nmatch, pmatch, eflags);
		free(attempt);

		if (result.out_of_memory || result.matched)
			return result;
	}

	return regexec_result_t { false, false, 0 };
}

int regexec(const regex_t* __restrict preg, const char* __restrict string, size_t nmatch, regmatch_t pmatch[], int eflags)
{
	if (preg->_cflags & REG_NOSUB)
		nmatch = 0;

	if (preg->_cflags & REG_NEWLINE)
		fprintf(stddbg, "regexec REG_NEWLINE is not supported");

	for (regoff_t off = 0; string[off]; off++, eflags |= REG_NOTBOL)
	{
		for (size_t i = 0; i < nmatch; i++)
			pmatch[i] = { -1, -1 };

		const auto [matched, out_of_memory, length] = match_expr(preg->_compiled, preg->_compiled_len, string + off, 0, nmatch, pmatch, eflags);
		if (out_of_memory)
			return REG_ESPACE;
		if (!matched)
			continue;

		if (nmatch > 0)
			pmatch[0] = { off, off + static_cast<regoff_t>(length) };
		for (size_t i = 1; i < nmatch; i++)
		{
			if (pmatch[i].rm_so == -1)
				continue;
			pmatch[i].rm_so += off;
			pmatch[i].rm_eo += off;
		}

		return 0;
	}

	return REG_NOMATCH;
}

void regfree(regex_t* preg)
{
	for (size_t i = 0; i < preg->_compiled_len; i++)
		regfree_element(&preg->_compiled[i]);
	free(preg->_compiled);
}

size_t regerror(int errcode, const regex_t* __restrict preg, char* __restrict errbuf, size_t errbuf_size)
{
	(void)preg;

	const char* message = "Unknown error.";
	switch (errcode)
	{
		case REG_NOMATCH:  message = "regexec() failed to match."; break;
		case REG_BADPAT:   message = "Invalid regular expression."; break;
		case REG_ECOLLATE: message = "Invalid collating element referenced."; break;
		case REG_ECTYPE:   message = "Invalid character class type referenced."; break;
		case REG_EESCAPE:  message = "Trailing <backslash> character in pattern."; break;
		case REG_ESUBREG:  message = "Number in \\digit invalid or in error."; break;
		case REG_EBRACK:   message = "\"[]\" imbalance."; break;
		case REG_EPAREN:   message = "\"\\(\\)\" or \"()\" imbalance."; break;
		case REG_EBRACE:   message = "\"\\{\\}\" imbalance."; break;
		case REG_BADBR:    message = "Content of \"\\{\\}\" invalid: not a number, number too large, more than two numbers, first larger than second."; break;
		case REG_ERANGE:   message = "Invalid endpoint in range expression."; break;
		case REG_ESPACE:   message = "Out of memory."; break;
		case REG_BADRPT:   message = "'?', '*', or '+' not preceded by valid regular expression."; break;
	}

	strncpy(errbuf, message, errbuf_size);
	return strlen(message);
}
