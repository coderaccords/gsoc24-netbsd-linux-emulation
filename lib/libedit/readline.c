/*	$NetBSD: readline.c,v 1.2 1997/10/23 22:51:59 christos Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jaromir Dolecek.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#if !defined(lint) && !defined(SCCSID)
__RCSID("$NetBSD: readline.c,v 1.2 1997/10/23 22:51:59 christos Exp $");
#endif /* not lint && not SCCSID */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <pwd.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include "histedit.h"
#include "readline.h"
#include "sys.h"
#include "el.h"

/* for rl_complete() */
#define TAB		'\r'

/* see comment at the #ifdef for sense of this */
#define GDB_411_HACK

/* readline compatibility stuff - look at readline sources/documentation */
/* to see what these variables mean */
const char     *rl_library_version = "EditLine wrapper";
char           *rl_readline_name = "";
FILE           *rl_instream = NULL;
FILE           *rl_outstream = NULL;
int             rl_point = 0;
int             rl_end = 0;
char           *rl_line_buffer = NULL;

int             history_base = 1;	/* probably never subject to change */
int             history_length = 0;
int             max_input_history = 0;
char            history_expansion_char = '!';
char            history_subst_char = '^';
char           *history_no_expand_chars = " \t\n=(";
Function       *history_inhibit_expansion_function = NULL;

int             rl_inhibit_completion = 0;
int             rl_attempted_completion_over = 0;
char           *rl_basic_word_break_characters = " \t\n\"\\'`@$><=;|&{(";
char           *rl_completer_word_break_characters = NULL;
char           *rl_completer_quote_characters = NULL;
CPFunction     *rl_completion_entry_function = NULL;
CPPFunction    *rl_attempted_completion_function = NULL;

/* used for readline emulation */
static History *h = NULL;
static EditLine *e = NULL;

/* internal functions */
static unsigned char _el_rl_complete __P((EditLine *, int));
static char *_get_prompt __P((EditLine *));
static HIST_ENTRY *_move_history __P((int));
static int _history_search_gen __P((const char *, int, int));
static int _history_expand_command __P((const char *, int, char **));
static char *_rl_compat_sub __P((const char *, const char *,
     const char *, int));
static int rl_complete_internal __P((int));

/*
 * needed for easy prompt switching
 */
static char    *el_rl_prompt = NULL;
static char *
_get_prompt(el)
	EditLine *el;
{
	return el_rl_prompt;
}

/*
 * generic function for moving around history
 */
static HIST_ENTRY *
_move_history(op)
	int op;
{
	HistEvent ev;
	static HIST_ENTRY rl_he;

	if (history(h, &ev, op) != 0)
		return (HIST_ENTRY *) NULL;

	rl_he.line = ev.str;
	rl_he.data = "";

	return &rl_he;
}


/* 
 * READLINE compatibility stuff 
 */

/*
 * initialize rl compat stuff
 */
int
rl_initialize()
{
	HistEvent ev;
	const LineInfo *li;

	if (e != NULL)
		el_end(e);
	if (h != NULL)
		history_end(h);

	if (!rl_instream)
		rl_instream = stdin;
	if (!rl_outstream)
		rl_outstream = stdout;
	e = el_init(rl_readline_name, rl_instream, rl_outstream);

	h = history_init();
	if (!e || !h)
		return -1;

	history(h, &ev, H_SETMAXSIZE, INT_MAX);	/* unlimited */
	history_length = 0;
	max_input_history = INT_MAX;
	el_set(e, EL_HIST, history, h);

	/* for proper prompt printing in readline() */
	el_rl_prompt = strdup("");
	el_set(e, EL_PROMPT, _get_prompt);
	el_set(e, EL_SIGNAL, 1);

	/* set default mode to "emacs"-style and read setting afterwards */
	/* so this can be overriden */
	el_set(e, EL_EDITOR, "emacs");

	/* for word completition - this has to go AFTER rebinding keys */
	/* to emacs-style */
	el_set(e, EL_ADDFN, "rl_complete",
	       "ReadLine compatible completition function",
	       _el_rl_complete);
	el_set(e, EL_BIND, "^I", "rl_complete", NULL);

	/* read settings from configuration file */
	el_source(e, NULL);

	/* some readline apps do use this */
	li = el_line(e);
	rl_line_buffer = (char *) li->buffer;
	rl_point = rl_end = 0;

	return 0;
}

/*
 * read one line from input stream and return it, chomping
 * trailing newline (if there is any)
 */
char *
readline(const char *prompt)
{
	HistEvent ev;
	size_t count;
	const char *ret;

	if (e == NULL || h == NULL)
		rl_initialize();

	/* set the prompt */
	if (strcmp(el_rl_prompt, prompt) != 0) {
		free(el_rl_prompt);
		el_rl_prompt = strdup(prompt);
	}
	/* get one line from input stream */
	ret = el_gets(e, &count);

	if (ret && count > 0) {
		char *foo;
		off_t lastidx;

		foo = strdup(ret);
		lastidx = count - 1;
		if (foo[lastidx] == '\n')
			foo[lastidx] = '\0';

		ret = foo;
	} else
		ret = NULL;

	history(h, &ev, H_GETSIZE);
	history_length = ev.num;

	return (char *) ret;
}

/*
 * history functions
 */

/*
 * is normally called before application starts to use
 * history expansion functions
 */
void
using_history()
{
	if (h == NULL || e == NULL)
		rl_initialize();
}

/*
 * substitute ``what'' with ``with'', returning resulting string; if
 * globally == 1, substitutes all occurences of what, otherwise only the
 * first one
 */
static char    *
_rl_compat_sub(str, what, with, globally)
	const char     *str, *what, *with;
	int             globally;
{
	char           *result;
	const char     *temp, *new;
	int             size, len, i, with_len, what_len, add;

	result = malloc((size = 16));
	temp = str;
	with_len = strlen(with);
	what_len = strlen(what);
	len = 0;
	do {
		new = strstr(temp, what);
		if (new) {
			i = new - temp;
			add = i + with_len;
			if (i + add + 1 >= size) {
				size += add + 1;
				result = realloc(result, size);
			}
			(void)strncpy(&result[len], temp, i);
			len += i;
			(void)strcpy(&result[len], with);	/* safe */
			len += with_len;
			temp = new + what_len;
		} else {
			add = strlen(temp);
			if (len + add + 1 >= size) {
				size += add + 1;
				result = realloc(result, size);
			}
			(void)strcpy(&result[len], temp);	/* safe */
			len += add;
			temp = NULL;
		}
	} while (temp);
	result[len] = '\0';

	return result;
}

/*
 * the real function doing history expansion - takes as argument command
 * to do and data upon which the command should be executed
 * does expansion the way I've understood readline documentation
 * word designator ``%'' isn't supported (yet ?)
 *
 * returns 0 if data was not modified, 1 if it was and 2 if the string
 * should be only printed and not executed; in case of error,
 * returns -1 and *result points to NULL
 * it's callers responsibility to free() string returned in *result
 */
static int
_history_expand_command(command, len, result)
	const char     *command;
	int             len;
	char          **result;
{
	char          **arr, *temp, *line, *search = NULL, *cmd;
	const char     *event_data = NULL;
	static const char *from = NULL, *to = NULL;
	int             start = -1, end = -1, max, i, size, idx;
	int             h_on = 0, t_on = 0, r_on = 0, e_on = 0, p_on = 0,
	                g_on = 0;
	int             event_num = 0, retval;

	*result = NULL;

	cmd = alloca(len + 1);
	(void)strncpy(cmd, command, len);
	cmd[len] = 0;

	idx = 1;
	/* find out which event to take */
	if (cmd[idx] == history_expansion_char) {
		event_num = history_length;
		idx++;
	} else {
		int             off, len, num;
		off = idx;
		while (cmd[off] && !strchr(":^$*-%", cmd[off]))
			off++;
		num = atoi(&cmd[idx]);
		if (num != 0) {
			event_num = num;
			if (num < 0)
				event_num += history_length + 1;
		} else {
			int prefix = 1, curr_num;
			HistEvent ev;

			len = off - idx;
			if (cmd[idx] == '?') {
				idx++, len--;
				if (cmd[off - 1] == '?')
					len--;
				else if (cmd[off] != '\n' && cmd[off] != '\0')
					return -1;
				prefix = 0;
			}
			search = alloca(len + 1);
			(void)strncpy(search, &cmd[idx], len);
			search[len] = '\0';

			if (history(h, &ev, H_CURR) != 0)
				return -1;
			curr_num = ev.num;

			if (prefix)
				retval = history_search_prefix(search, -1);
			else
				retval = history_search(search, -1);

			if (retval == -1) {
				fprintf(rl_outstream, "%s: Event not found\n",
					search);
				return -1;
			}
			if (history(h, &ev, H_CURR) != 0)
				return -1;
			event_data = ev.str;

			/* roll back to original position */
			history(h, &ev, H_NEXT_EVENT, curr_num);
		}
		idx = off;
	}

	if (!event_data && event_num >= 0) {
		HIST_ENTRY *rl_he;
		rl_he = history_get(event_num);
		if (!rl_he)
			return 0;
		event_data = rl_he->line;
	} else
		return -1;

	if (cmd[idx] != ':')
		return -1;
	cmd += idx + 1;

	/* recognize cmd */
	if (*cmd == '^')
		start = end = 1, cmd++;
	else if (*cmd == '$')
		start = end = -1, cmd++;
	else if (*cmd == '*')
		start = 1, end = -1, cmd++;
	else if (isdigit(*cmd)) {
		const char *temp;
		int shifted = 0;

		start = atoi(cmd);
		temp = cmd;
		for (; isdigit(*cmd); cmd++);
		if (temp != cmd)
			shifted = 1;
		if (shifted && *cmd == '-') {
			if (!isdigit(*(cmd + 1)))
				end = -2;
			else {
				end = atoi(cmd + 1);
				for (; isdigit(*cmd); cmd++);
			}
		} else if (shifted && *cmd == '*')
			end = -1, cmd++;
		else if (shifted)
			end = start;
	}
	if (*cmd == ':')
		cmd++;

	line = strdup(event_data);
	for (; *cmd; cmd++) {
		if (*cmd == ':')
			continue;
		else if (*cmd == 'h')
			h_on = 1 | g_on, g_on = 0;
		else if (*cmd == 't')
			t_on = 1 | g_on, g_on = 0;
		else if (*cmd == 'r')
			r_on = 1 | g_on, g_on = 0;
		else if (*cmd == 'e')
			e_on = 1 | g_on, g_on = 0;
		else if (*cmd == 'p')
			p_on = 1 | g_on, g_on = 0;
		else if (*cmd == 'g')
			g_on = 2;
		else if (*cmd == 's' || *cmd == '&') {
			char           *what, *with, delim;
			int             len, size, from_len;

			if (*cmd == '&' && (from == NULL || to == NULL))
				continue;
			else if (*cmd == 's') {
				delim = *(++cmd), cmd++;
				size = 16;
				what = realloc((void *) from, size);
				len = 0;
				for (; *cmd && *cmd != delim; cmd++) {
					if (*cmd == '\\'
					    && *(cmd + 1) == delim)
						cmd++;
					if (len >= size)
						what = realloc(what,
						    (size <<= 1));
					what[len++] = *cmd;
				}
				what[len] = '\0';
				from = what;
				if (*what == '\0') {
					free(what);
					if (search)
						from = strdup(search);
					else {
						from = NULL;
						return -1;
					}
				}
				cmd++;	/* shift after delim */
				if (!*cmd)
					continue;

				size = 16;
				with = realloc((void *) to, size);
				len = 0;
				from_len = strlen(from);
				for (; *cmd && *cmd != delim; cmd++) {
					if (len + from_len + 1 >= size) {
						size += from_len + 1;
						with = realloc(with, size);
					}
					if (*cmd == '&') {
						/* safe */
						(void)strcpy(&with[len], from);
						len += from_len;
						continue;
					}
					if (*cmd == '\\'
					    && (*(cmd + 1) == delim
						|| *(cmd + 1) == '&'))
						cmd++;
					with[len++] = *cmd;
				}
				with[len] = '\0';
				to = with;

				temp = _rl_compat_sub(line, from, to, 
				    (g_on) ? 1 : 0);
				free(line);
				line = temp;
				g_on = 0;
			}
		}
	}

	arr = history_tokenize(line);
	free(line);		/* no more needed */
	if (arr && *arr == NULL)
		free(arr), arr = NULL;
	if (!arr)
		return -1;

	/* find out max valid idx to array of array */
	max = 0;
	for (i = 0; arr[i]; i++)
		max++;
	max--;

	/* set boundaries to something relevant */
	if (start < 0)
		start = 1;
	if (end < 0)
		end = max - ((end < -1) ? 1 : 0);

	/* check boundaries ... */
	if (start > max || end > max || start > end)
		return -1;

	for (i = 0; i <= max; i++) {
		char           *temp;
		if (h_on && (i == 1 || h_on > 1) &&
		    (temp = strrchr(arr[i], '/')))
			*(temp + 1) = '\0';
		if (t_on && (i == 1 || t_on > 1) &&
		    (temp = strrchr(arr[i], '/')))
			(void)strcpy(arr[i], temp + 1);
		if (r_on && (i == 1 || r_on > 1) &&
		    (temp = strrchr(arr[i], '.')))
			*temp = '\0';
		if (e_on && (i == 1 || e_on > 1) &&
		    (temp = strrchr(arr[i], '.')))
			(void)strcpy(arr[i], temp);
	}

	size = 1, len = 0;
	temp = malloc(size);
	for (i = start; start <= i && i <= end; i++) {
		int             arr_len;

		arr_len = strlen(arr[i]);
		if (len + arr_len + 1 >= size) {
			size += arr_len + 1;
			temp = realloc(temp, size);
		}
		(void)strcpy(&temp[len], arr[i]);	/* safe */
		len += arr_len;
		temp[len++] = ' ';	/* add a space */
	}
	while (len > 0 && isspace(temp[len - 1]))
		len--;
	temp[len] = '\0';

	*result = temp;

	for (i = 0; i <= max; i++)
		free(arr[i]);
	free(arr), arr = (char **) NULL;
	return (p_on) ? 2 : 1;
}

/*
 * csh-style history expansion
 */
int
history_expand(str, output)
	char           *str;
	char          **output;
{
	int             i, retval = 0, size, idx;
	char           *temp, *result;

	if (h == NULL || e == NULL)
		rl_initialize();

	*output = strdup(str);	/* do it early */

	if (str[0] == history_subst_char) {
		/* ^foo^foo2^ is equivalent to !!:s^foo^foo2^ */
		temp = alloca(4 + strlen(str) + 1);
		temp[0] = temp[1] = history_expansion_char;
		temp[2] = ':';
		temp[3] = 's';
		(void)strcpy(temp + 4, str);
		str = temp;
	}
#define ADD_STRING(what, len) 						\
	{								\
		if (idx + len + 1 > size)				\
			result = realloc(result, (size += len + 1));	\
		(void)strncpy(&result[idx], what, len);			\
		idx += len;						\
		result[idx] = '\0';					\
	}

	result = NULL;
	size = idx = 0;
	for (i = 0; str[i];) {
		int start, j, len, loop_again;

		loop_again = 1;
		start = j = i;
loop:
		for (; str[j]; j++) {
			if (str[j] == '\\' &&
			    str[j + 1] == history_expansion_char) {
				(void)strcpy(&str[j], &str[j + 1]);
				continue;
			}
			if (!loop_again) {
				if (str[j] == '?') {
					while (str[j] && str[++j] != '?');
					if (str[j] == '?')
						j++;
				} else if (isspace(str[j]))
					break;
			}
			if (str[j] == history_expansion_char
			    && !strchr(history_no_expand_chars, str[j + 1])
			    && (!history_inhibit_expansion_function ||
			(*history_inhibit_expansion_function) (str, j) == 0))
				break;
		}

		if (str[j] && str[j + 1] != '#' && loop_again) {
			i = j;
			j++;
			if (str[j] == history_expansion_char)
				j++;
			loop_again = 0;
			goto loop;
		}
		len = i - start;
		temp = &str[start];
		ADD_STRING(temp, len);

		if (str[i] == '\0' || str[i] != history_expansion_char
		    || str[i + 1] == '#') {
			len = j - i;
			temp = &str[i];
			ADD_STRING(temp, len);
			if (start == 0)
				retval = 0;
			else
				retval = 1;
			break;
		}
		retval = _history_expand_command(&str[i], j - i, &temp);
		if (retval != -1) {
			len = strlen(temp);
			ADD_STRING(temp, len);
		}
		i = j;
	}			/* for(i ...) */

	if (retval == 2) {
		add_history(temp);
#ifdef GDB_411_HACK
		/* gdb 4.11 has been shipped with readline, where */
		/* history_expand() returned -1 when the line	  */
		/* should not be executed; in readline 2.1+	  */
		/* it should return 2 in such a case		  */
		retval = -1;
#endif
	}
	free(*output);
	*output = result;

	return retval;
}

/*
 * returns array of tokens parsed out of string, much as the shell might
 */
char **
history_tokenize(str)
	const char     *str;
{
	int  size = 1, result_idx = 0, i, start, len;
	char **result = NULL, *temp, delim = '\0';

	for (i = 0; str[i]; i++) {
		while (isspace(str[i]))
			i++;
		start = i;
		for (; str[i]; i++) {
			if (str[i] == '\\')
				i++;
			else if (str[i] == delim)
				delim = '\0';
			else if (!delim &&
			    (isspace(str[i]) || strchr("()<>;&|$", str[i])))
				break;
			else if (!delim && strchr("'`\"", str[i]))
				delim = str[i];
		}

		if (result_idx + 2 >= size) {
			size <<= 1;
			result = realloc(result, size * sizeof(char *));
		}
		len = i - start;
		temp = malloc(len + 1);
		(void)strncpy(temp, &str[start], len);
		temp[len] = '\0';
		result[result_idx++] = temp;
		result[result_idx] = NULL;
	}

	return result;
}

/*
 * limit size of history record to ``max'' events
 */
void
stifle_history(max)
	int max;
{
	HistEvent ev;

	if (h == NULL || e == NULL)
		rl_initialize();

	if (history(h, &ev, H_SETMAXSIZE, max) == 0)
		max_input_history = max;
}

/*
 * "unlimit" size of history - set the limit to maximum allowed int value
 */
int
unstifle_history()
{
	HistEvent ev;
	int omax;

	history(h, &ev, H_SETMAXSIZE, INT_MAX);
	omax = max_input_history;
	max_input_history = INT_MAX;
	return omax;		/* some value _must_ be returned */
}

int
history_is_stifled()
{
	/* cannot return true answer */
	return (max_input_history != INT_MAX);
}

/*
 * read history from a file given
 */
int
read_history(filename)
	const char *filename;
{
	HistEvent ev;

	if (h == NULL || e == NULL)
		rl_initialize();
	return history(h, &ev, H_LOAD, filename);
}

/*
 * write history to a file given
 */
int
write_history(filename)
	const char *filename;
{
	HistEvent ev;

	if (h == NULL || e == NULL)
		rl_initialize();
	return history(h, &ev, H_SAVE, filename);
}

/*
 * returns history ``num''th event
 *
 * returned pointer points to static variable
 */
HIST_ENTRY *
history_get(num)
	int             num;
{
	static HIST_ENTRY she;
	HistEvent ev;
	int i = 1, curr_num;

	if (h == NULL || e == NULL)
		rl_initialize();

	/* rewind to beginning */
	if (history(h, &ev, H_CURR) != 0)
		return NULL;
	curr_num = ev.num;
	if (history(h, &ev, H_LAST) != 0)
		return NULL;	/* error */
	while (i < num && history(h, &ev, H_PREV) == 0)
		i++;
	if (i != num)
		return NULL;	/* not so many entries */

	she.line = ev.str;
	she.data = NULL;

	/* rewind history to the same event it was before */
	(void) history(h, &ev, H_FIRST);
	(void) history(h, &ev, H_NEXT_EVENT, curr_num);

	return &she;
}

/*
 * add the line to history table
 */
int
add_history(line)
	const char *line;
{
	HistEvent ev;

	if (h == NULL || e == NULL)
		rl_initialize();

	(void) history(h, &ev, H_ENTER, line);
	if (history(h, &ev, H_GETSIZE) == 0)
		history_length = ev.num;

	return (!(history_length > 0));	/* return 0 if all is okay */
}

/*
 * clear the history list - delete all entries
 */
void
clear_history()
{
	HistEvent ev;
	history(h, &ev, H_CLEAR);
}

/*
 * returns offset of the current history event
 */
int
where_history()
{
	HistEvent ev;
	int curr_num, off;

	if (history(h, &ev, H_CURR) != 0)
		return 0;
	curr_num = ev.num;

	history(h, &ev, H_FIRST);
	off = 1;
	while (ev.num != curr_num && history(h, &ev, H_NEXT) == 0)
		off++;

	return off;
}

/*
 * returns current history event or NULL if there is no such event
 */
HIST_ENTRY *
current_history()
{
	return _move_history(H_CURR);
}

/*
 * returns total number of bytes history events' data are using
 */
int
history_total_bytes()
{
	HistEvent ev;
	int curr_num, size;

	if (history(h, &ev, H_CURR) != 0)
		return -1;
	curr_num = ev.num;

	history(h, &ev, H_FIRST);
	size = 0;
	do
		size += strlen(ev.str);
	while (history(h, &ev, H_NEXT) == 0);

	/* get to the same position as before */
	history(h, &ev, H_PREV_EVENT, curr_num);

	return size;
}

/*
 * sets the position in the history list to ``pos''
 */
int
history_set_pos(pos)
	int pos;
{
	HistEvent ev;
	int off, curr_num;

	if (pos > history_length || pos < 0)
		return -1;

	history(h, &ev, H_CURR);
	curr_num = ev.num;
	history(h, &ev, H_FIRST);
	off = 0;
	while (off < pos && history(h, &ev, H_NEXT) == 0)
		off++;

	if (off != pos) {	/* do a rollback in case of error */
		history(h, &ev, H_FIRST);
		history(h, &ev, H_NEXT_EVENT, curr_num);
		return -1;
	}
	return 0;
}

/*
 * returns previous event in history and shifts pointer accordingly
 */
HIST_ENTRY *
previous_history()
{
	return _move_history(H_PREV);
}

/*
 * returns next event in history and shifts pointer accordingly
 */
HIST_ENTRY *
next_history()
{
	return _move_history(H_NEXT);
}

/*
 * generic history search function
 */
static int
_history_search_gen(str, direction, pos)
	const char *str;
	int direction, pos;
{
	HistEvent       ev;
	const char     *strp;
	int             curr_num;

	if (history(h, &ev, H_CURR) != 0)
		return -1;
	curr_num = ev.num;

	for (;;) {
		strp = strstr(ev.str, str);
		if (strp && (pos < 0 || &ev.str[pos] == strp))
			return (int) (strp - ev.str);
		if (history(h, &ev, direction < 0 ? H_PREV : H_NEXT) != 0)
			break;
	}

	history(h, &ev, direction < 0 ? H_NEXT_EVENT : H_PREV_EVENT, curr_num);

	return -1;
}

/*
 * searches for first history event containing the str
 */
int
history_search(str, direction)
	const char     *str;
	int             direction;
{
	return _history_search_gen(str, direction, -1);
}

/*
 * searches for first history event beginning with str
 */
int
history_search_prefix(str, direction)
	const char     *str;
	int             direction;
{
	return _history_search_gen(str, direction, 0);
}

/*
 * search for event in history containing str, starting at offset
 * abs(pos); continue backward, if pos<0, forward otherwise
 */
int
history_search_pos(str, direction, pos)
	const char     *str;
	int             direction, pos;
{
	HistEvent       ev;
	int             curr_num, off;

	off = (pos > 0) ? pos : -pos;
	pos = (pos > 0) ? 1 : -1;

	if (history(h, &ev, H_CURR) != 0)
		return -1;
	curr_num = ev.num;

	if (history_set_pos(off) != 0 || history(h, &ev, H_CURR) != 0)
		return -1;


	for (;;) {
		if (strstr(ev.str, str))
			return off;
		if (history(h, &ev, (pos < 0) ? H_PREV : H_NEXT) != 0)
			break;
	}

	/* set "current" pointer back to previous state */
	history(h, &ev, (pos < 0) ? H_NEXT_EVENT : H_PREV_EVENT, curr_num);

	return -1;
}


/********************************/
/* completition functions	 */

/*
 * does tilde expansion of strings of type ``~user/foo''
 * if ``user'' isn't valid user name or ``txt'' doesn't start
 * w/ '~', returns pointer to strdup()ed copy of ``txt''
 *
 * it's callers's responsibility to free() returned string
 */
char *
tilde_expand(txt)
	char     *txt;
{
	struct passwd  *pass;
	char           *temp;
	size_t          len = 0;

	if (txt[0] != '~')
		return strdup(txt);

	temp = strchr(txt + 1, '/');
	if (temp == NULL)
		temp = strdup(txt + 1);
	else {
		len = temp - txt + 1;	/* text until string after slash */
		temp = malloc(len);
		(void)strncpy(temp, txt + 1, len - 2);
		temp[len - 2] = '\0';
	}
	pass = getpwnam(temp);
	free(temp);		/* value no more needed */
	if (pass == NULL)
		return strdup(txt);

	/* update pointer txt to point at string immedially following */
	/* first slash */
	txt += len;

	temp = malloc(strlen(pass->pw_dir) + 1 + strlen(txt) + 1);
	(void)sprintf(temp, "%s/%s", pass->pw_dir, txt);

	return temp;
}

/*
 * return first found file name starting by the ``text'' or NULL if no
 * such file can be found
 * value of ``state'' is ignored
 *
 * it's caller's responsibility to free returned string
 */
char           *
filename_completion_function(text, state)
	const char     *text;
	int             state;
{
	static DIR     *dir = NULL;
	static char    *filename = NULL, *dirname = NULL;
	static size_t   filename_len = 0;
	struct dirent  *entry;
	char           *temp;
	size_t          len;

	if (state == 0 || dir == NULL) {
		if (dir != NULL) {
			closedir(dir);
			dir = NULL;
		}
		temp = strrchr(text, '/');
		if (temp) {
			temp++;
			filename = realloc(filename, strlen(temp) + 1);
			(void)strcpy(filename, temp);
			len = temp - text;	/* including last slash */
			dirname = realloc(dirname, len + 1);
			(void)strncpy(dirname, text, len);
			dirname[len] = '\0';
		} else {
			filename = strdup(text);
			dirname = NULL;
		}

		/* support for ``~user'' syntax */
		if (dirname && *dirname == '~') {
			temp = tilde_expand(dirname);
			dirname = realloc(dirname, strlen(temp) + 1);
			(void)strcpy(dirname, temp);	/* safe */
			free(temp);	/* no more needed */
		}
		/* will be used in cycle */
		filename_len = strlen(filename);
		if (filename_len == 0)
			return NULL;	/* no expansion possible */

		dir = opendir(dirname ? dirname : ".");
		if (!dir)
			return NULL;	/* cannot open the directory */
	}
	/* find the match */
	while ((entry = readdir(dir))) {
		/* otherwise, get first entry where first */
		/* filename_len characters are equal	  */
		if (entry->d_name[0] == filename[0]
		    && entry->d_namlen >= filename_len
		    && strncmp(entry->d_name, filename,
			       filename_len) == 0)
			break;
	}

	if (entry) {		/* match found */

		struct stat     stbuf;
		len = entry->d_namlen +
			((dirname) ? strlen(dirname) : 0) + 1 + 1;
		temp = malloc(len);
		(void)sprintf(temp, "%s%s",
			dirname ? dirname : "", entry->d_name);	/* safe */

		/* test, if it's directory */
		if (stat(temp, &stbuf) == 0 && S_ISDIR(stbuf.st_mode))
			strcat(temp, "/");	/* safe */
	} else
		temp = NULL;

	return temp;
}

/*
 * a completion generator for usernames; returns _first_ username
 * which starts with supplied text
 * text contains a partial username preceded by random character
 * (usually '~'); state is ignored
 * it's callers responsibility to free returned value
 */
char           *
username_completion_function(text, state)
	const char     *text;
	int             state;
{
	struct passwd  *pwd;

	if (text[0] == '\0')
		return NULL;

	if (*text == '~')
		text++;

	if (state == 0)
		setpwent();

	while ((pwd = getpwent()) && text[0] == pwd->pw_name[0]
	       && strcmp(text, pwd->pw_name) == 0);

	if (pwd == NULL) {
		endpwent();
		return NULL;
	}
	return strdup(pwd->pw_name);
}

/*
 * el-compatible wrapper around rl_complete; needed for key binding
 */
static unsigned char
_el_rl_complete(el, ch)
	EditLine       *el;
	int             ch;
{
	return (unsigned char) rl_complete(0, ch);
}

/*
 * returns list of completitions for text given
 */
char          **
completion_matches(text, genfunc)
	const char     *text;
	CPFunction     *genfunc;
{
	char          **match_list = NULL, *retstr, *prevstr;
	size_t          matches, math_list_len, max_equal, len, which,
	                i;

	if (h == NULL || e == NULL)
		rl_initialize();

	matches = 0;
	math_list_len = 1;
	while ((retstr = (*genfunc) (text, matches))) {
		if (matches + 1 >= math_list_len) {
			math_list_len <<= 1;
			match_list = realloc(match_list,
			    math_list_len * sizeof(char *));
		}
		match_list[++matches] = retstr;
	}

	if (!match_list)
		return (char **) NULL;	/* nothing found */

	/* find least denominator and insert it to match_list[0] */
	which = 2;
	prevstr = match_list[1];
	len = max_equal = strlen(prevstr);
	for (; which < matches; which++) {
		for (i = 0; i < max_equal &&
		    prevstr[i] == match_list[which][i]; i++)
			continue;
		max_equal = i;
	}

	retstr = malloc(max_equal + 1);
	(void)strncpy(retstr, match_list[1], max_equal);
	retstr[max_equal] = '\0';
	match_list[0] = retstr;

	/* add NULL as last pointer to the array */
	if (matches + 1 >= math_list_len)
		match_list = realloc(match_list,
		    (math_list_len + 1) * sizeof(char *));
	match_list[matches + 1] = (char *) NULL;

	return match_list;
}

/*
 * called by rl_complete()
 */
static int
rl_complete_internal(what_to_do)
	int             what_to_do;
{
	CPFunction     *complet_func;
	const LineInfo *li;
	char           *temp, *temp2, **arr;
	int             len;

	if (h == NULL || e == NULL)
		rl_initialize();

	complet_func = rl_completion_entry_function;
	if (!complet_func)
		complet_func = filename_completion_function;

	li = el_line(e);
	temp = (char *) li->cursor;
	while (temp > li->buffer &&
	    !strchr(rl_basic_word_break_characters, *(temp - 1)))
		temp--;

	len = li->cursor - temp;
	temp2 = alloca(len + 1);
	(void)strncpy(temp2, temp, len);
	temp = temp2;
	temp[len] = '\0';

	/* these can be used by function called in completion_matches() */
	/* or (*rl_attempted_completion_function)() */
	rl_point = li->cursor - li->buffer;
	rl_end = li->lastchar - li->buffer;

	if (!rl_attempted_completion_function)
		arr = completion_matches(temp, complet_func);
	else {
		int             end = li->cursor - li->buffer;
		arr = (*rl_attempted_completion_function) (temp,
							   end - len, end);
	}
	free(temp);		/* no more needed */

	if (arr) {
		int             i;

		el_deletestr(e, len);
		el_insertstr(e, arr[0]);
		if (strcmp(arr[0], arr[1]) == 0) {
			/* lcd is valid object, so add a space to mark it */
			/* in case of filename completition, add a space  */
			/* only if object found is not directory	  */
			int             len = strlen(arr[0]);
			if (complet_func != filename_completion_function
			    || (len > 0 && (arr[0])[len - 1] != '/'))
				el_insertstr(e, " ");
		} else
			/* lcd is not a valid object - further specification */
			/* is needed */
			term_beep(e);

		/* free elements of array and the array itself */
		for (i = 0; arr[i]; i++)
			free(arr[i]);
		free(arr), arr = NULL;

		return CC_REFRESH;
	}
	return CC_NORM;
}

/*
 * complete word at current point
 */
int
rl_complete(ignore, invoking_key)
	int             ignore, invoking_key;
{
	if (h == NULL || e == NULL)
		rl_initialize();

	if (rl_inhibit_completion) {
		rl_insert(ignore, invoking_key);
		return CC_REFRESH;
	} else
		return rl_complete_internal(invoking_key);

	return CC_REFRESH;
}

/*
 * misc other functions	
 */

/*
 * bind key c to readline-type function func
 */
int
rl_bind_key(c, func)
	int             c;
	int func        __P((int, int));
{
	int             retval = -1;

	if (h == NULL || e == NULL)
		rl_initialize();

	if (func == rl_insert) {
		/* XXX notice there is no range checking of ``c'' */
		e->el_map.key[c] = ED_INSERT;
		retval = 0;
	}
	return retval;
}

/*
 * read one key from input - handles chars pushed back
 * to input stream also
 */
int
rl_read_key()
{
	char            fooarr[2 * sizeof(int)];

	if (e == NULL || h == NULL)
		rl_initialize();

	return el_getc(e, fooarr);
}

/*
 * reset the terminal
 */
void
rl_reset_terminal(p)
	const char     *p;
{
	if (h == NULL || e == NULL)
		rl_initialize();
	el_reset(e);
}

/*
 * insert character ``c'' back into input stream, ``count'' times
 */
int
rl_insert(count, c)
	int             count, c;
{
	char            arr[2];

	if (h == NULL || e == NULL)
		rl_initialize();

	/* XXX - int -> char conversion can lose on multichars */
	arr[0] = c;
	arr[1] = '\0';

	for (; count > 0; count--)
		el_push(e, arr);

	return 0;
}
