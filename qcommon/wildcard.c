/*
   Copyright (C) 1996, 1997, 1998, 1999, 2000 Florian Schintke

   This is free software; you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 2, or (at your option) any later
   version.

   This is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.

   You should have received a copy of the GNU General Public License with
   the c2html, java2html, pas2html or perl2html source package as the
   file COPYING. If not, write to the Free Software Foundation, Inc.,
   59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.
*/

#include "wildcard.h"

// Scans a set of characters and returns 0 if the set mismatches at this
// position in the teststring and 1 if it is matching.
// Wildcard is set to the closing ] and test is unmodified if mismatched
// and otherwise the char pointer is pointing to the next character.
qboolean set(char **wildcard, char **test);

// Scans an asterisk
qboolean asterisk(char **wildcard, char **test);

qboolean wildcardfit(char *wildcard, char *test)
{
	qboolean fit = true;

	for (; *wildcard != '\000' && fit && *test != '\000'; wildcard++)
	{
		switch (*wildcard)
		{
			case '[':
				wildcard++; // Leave out the opening square bracket
				fit = set(&wildcard, &test);
				// We don't need to decrement the wildcard as in case of asterisk because the closing ] is still there.
				break;

			case '?':
				test++;
				break;

			case '*':
				fit = asterisk(&wildcard, &test);
				// The asterisk was skipped by asterisk() but the loop will increment by itself. So we have to decrement.
				wildcard--;
				break;

			default:
				fit = (*wildcard == *test);
				test++;
		}
	}

	// Here the teststring is empty otherwise you cannot leave the previous loop
	if(fit)
		while (*wildcard == '*')
			wildcard++;
	
	return (fit && *test == '\0' && *wildcard == '\0');
}

qboolean set(char **wildcard, char **test)
{
	qboolean fit = false;
	qboolean negation = false;
	qboolean at_beginning = true;

	if (**wildcard == '!')
	{
		negation = true;
		(*wildcard)++;
	}

	while (**wildcard != ']' || at_beginning)
	{
		if (!fit)
		{
			if (**wildcard == '-'
				&& *(*wildcard - 1) < *(*wildcard + 1)
				&& *(*wildcard + 1) != ']'
				&& !at_beginning)
			{
				if (**test >= *(*wildcard - 1) && **test <= *(*wildcard + 1))
				{
					fit = true;
					(*wildcard)++;
				}
			}
			else if (**wildcard == **test)
			{
				fit = true;
			}
		}

		(*wildcard)++;
		at_beginning = false;
	}

	// Flip
	if (negation)
		fit = !fit;

	if (fit)
		(*test)++;

	return fit;
}

qboolean asterisk(char **wildcard, char **test)
{
	// Erase the leading asterisk
	(*wildcard)++;
	while (**test != '\000' && (**wildcard == '?' || **wildcard == '*'))
	{
		if (**wildcard == '?')
			(*test)++;
		(*wildcard)++;
	}

	// Now it could be that test is empty and wildcard contains aterisks. Then we delete them to get a proper state.
	while (**wildcard == '*')
		(*wildcard)++;

	if (**test == '\0')
		return (**wildcard == '\0');

	// Neither test nor wildcard are empty!
	// The first character of wildcard isn't in [*?]
	if (!wildcardfit(*wildcard, *test))
	{
		do
		{
			(*test)++;

			// Skip as much characters as possible in the teststring stop if a character match occurs
			while (**test != '\0' && **wildcard != **test && **wildcard != '[')
				(*test)++;

			if (**test == '\0')
				return (**wildcard == '\0');
		} while (!wildcardfit(*wildcard, *test));
	}

	return true;
}