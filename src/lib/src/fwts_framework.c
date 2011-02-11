/*
 * Copyright (C) 2010-2011 Canonical
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <time.h>
#include <getopt.h>
#include <sys/utsname.h>

#include "fwts.h"

#define RESULTS_LOG	"results.log"

#define FWTS_RUN_ALL_FLAGS		\
	(FWTS_BATCH |			\
	 FWTS_INTERACTIVE |		\
	 FWTS_BATCH_EXPERIMENTAL |	\
	 FWTS_INTERACTIVE_EXPERIMENTAL |\
	 FWTS_POWER_STATES |		\
	 FWTS_UTILS)

#define FWTS_ARGS_WIDTH 	28
#define FWTS_MIN_TTY_WIDTH	50

static fwts_list *tests_to_run;
static fwts_list *tests_to_skip;

static fwts_option fwts_framework_options[] = {
	{ "stdout-summary", 	"",   0, "Output SUCCESS or FAILED to stdout at end of tests." },
	{ "help", 		"h?", 0, "Print this help." },
	{ "results-output", 	"r:", 1, "Output results to a named file. Filename can also be stout or stderr, e.g. --results-output=myresults.log,  -r stdout." },
	{ "results-no-separators", "", 0, "No horizontal separators in results log." },
	{ "log-filter", 	"",   1, "Define filters to dump out specific log fields: --log-filter=RES,SUM - dump out results and summary, --log-filter=ALL,~INF - dump out all fields except info fields." },
	{ "log-fields", 	"",   0, "Show available log filtering fields." },
	{ "log-format", 	"",   1, "Define output log format:  e.g. --log-format=\"\%date \%time [\%field] (\%owner): \".  Fields are: \%time - time, \%field - filter field, \%owner - name of test, \%level - failure error level, \%line - log line number." },
	{ "show-progress", 	"p",  0, "Output test progress report to stderr." },
	{ "show-tests", 	"s",  0, "Show available tests." },
	{ "klog", 		"k:", 1, "Specify kernel log file rather than reading it from the kernel, e.g. --klog=dmesg.log" },
	{ "dmidecode", 		"",   1, "Specify path to dmidecode executable, e.g. --dmidecode=path." },
	{ "log-width", 		"w:", 1, "Define the output log width in characters." },
	{ "lspci", 		"",   1, "Specify path to lspci, e.g. --lspci=path." },
	{ "batch", 		"b",  0, "Run non-Interactive tests." },
	{ "interactive", 	"i",  0, "Just run Interactive tests." },
	{ "force-clean", 	"f",  0, "Force a clean results log file." },
	{ "version", 		"v",  0, "Show version (" FWTS_VERSION ")." },
	{ "dump", 		"d",  0, "Dump out dmesg, dmidecode, lspci, ACPI tables to logs." },
	{ "table-path", 	"T:", 1, "Path to ACPI tables dumped by acpidump and then acpixtract, e.g. --table-path=/some/path/to/acpidumps" },
	{ "batch-experimental", "",   0, "Run Batch Experimental tests." },
	{ "interactive-experimental", "", 0, "Just run Interactive Experimental tests." },
	{ "power-states", 	"P",  0, "Test S3, S4 power states." },
	{ "all", 		"a",  0, "Run all tests." },
	{ "show-progress-dialog","D", 0, "Output test progress for use in dialog tool." },
	{ "skip-test", 		"S:", 1, "Skip listed tests, e.g. --skip-test=s3,nx,method" },
	{ "quiet", 		"q",  0, "Run quietly." },
	{ "dumpfile", 		"",   1, "Load ACPI tables using file generated by acpidump, e.g. --dumpfile=acpidump.dat" },
	{ "lp-tags", 		"",   0, "Output just LaunchPad bug tags." },
	{ "show-tests-full", 	"",   0, "Show available tests including all minor tests." },
	{ "utils", 		"u",  0, "Run Utility 'tests'." },
	{ "json-data-path", 	"j:", 1, "Specify path to fwts json data files - default is /usr/share/fwts." },
	{ "lp-tags-log", 	"",   0, "Output LaunchPad bug tags in results log." },
	{ "disassemble-aml", 	"",   0, "Disassemble AML from DSDT and SSDT tables." },
	{ NULL, NULL, 0, NULL }
};

enum {
	FWTS_PASSED_TEXT,
	FWTS_FAILED_TEXT,
	FWTS_FAILED_LOW_TEXT,
	FWTS_FAILED_HIGH_TEXT,
	FWTS_FAILED_MEDIUM_TEXT,
	FWTS_FAILED_CRITICAL_TEXT,
	FWTS_WARNING_TEXT,
	FWTS_ERROR_TEXT,
	FWTS_ADVICE_TEXT,
	FWTS_SKIPPED_TEXT,
	FWTS_ABORTED_TEXT,
};

static fwts_list *fwts_framework_test_list;

typedef struct {
	const int env_id;
	const char *env_name;
	const char *env_default;
	char *env_value;
} fwts_framework_setting;

#define ID_NAME(id)	id, # id

static const char *fwts_copyright[] = {
	"Some of this work - Copyright (c) 1999 - 2010, Intel Corp. All rights reserved.",
	"Some of this work - Copyright (c) 2010 - 2011, Canonical.",	
	NULL
};

static fwts_framework_setting fwts_framework_settings[] = {
	{ ID_NAME(FWTS_PASSED_TEXT),		"PASSED",  NULL },
	{ ID_NAME(FWTS_FAILED_TEXT),		"FAILED",  NULL },
	{ ID_NAME(FWTS_FAILED_LOW_TEXT),	"FAILED_LOW", NULL },
	{ ID_NAME(FWTS_FAILED_HIGH_TEXT),	"FAILED_HIGH", NULL },
	{ ID_NAME(FWTS_FAILED_MEDIUM_TEXT),	"FAILED_MEDIUM", NULL },
	{ ID_NAME(FWTS_FAILED_CRITICAL_TEXT),	"FAILED_CRITICAL", NULL },
	{ ID_NAME(FWTS_WARNING_TEXT),		"WARNING", NULL },
	{ ID_NAME(FWTS_ERROR_TEXT),		"ERROR",   NULL },
	{ ID_NAME(FWTS_ADVICE_TEXT),		"ADVICE",  NULL },
	{ ID_NAME(FWTS_SKIPPED_TEXT),		"SKIPPED", NULL },
	{ ID_NAME(FWTS_ABORTED_TEXT),		"ABORTED", NULL },
};

/*
 *  fwts_framework_compare_priority()
 *	used to register tests sorted on run priority
 */
static int fwts_framework_compare_priority(void *data1, void *data2)
{
	fwts_framework_test *test1 = (fwts_framework_test *)data1;
	fwts_framework_test *test2 = (fwts_framework_test *)data2;

	return (test1->priority - test2->priority);
}

/*
 * fwts_framework_test_add()
 *    register a test, called by FWTS_REGISTER() macro
 */
void fwts_framework_test_add(const char *name,
	fwts_framework_ops *ops,
	const int priority,
	const int flags)
{
	fwts_framework_test *new_test;

	if (flags & ~FWTS_RUN_ALL_FLAGS) {
		fprintf(stderr, "Test %s flags must be FWTS_BATCH, FWTS_INTERACTIVE, FWTS_BATCH_EXPERIMENTAL, \n"
			        "FWTS_INTERACTIVE_EXPERIMENTAL or FWTS_POWER_STATES, got %x\n", name, flags);
		exit(EXIT_FAILURE);
	}

	if (fwts_framework_test_list == NULL) {
		fwts_framework_test_list = fwts_list_new();
		if (fwts_framework_test_list == NULL) {
			fprintf(stderr, "FATAL: Could not allocate memory setting up test framework\n");
			exit(EXIT_FAILURE);
		}
	}

	/* This happens early, so if it goes wrong, bail out */
	if ((new_test = calloc(1, sizeof(fwts_framework_test))) == NULL) {
		fprintf(stderr, "FATAL: Could not allocate memory adding tests to test framework\n");
		exit(EXIT_FAILURE);
	}

	/* Total up minor tests in this test */
	for (ops->total_tests = 0; ops->minor_tests[ops->total_tests].test_func != NULL; ops->total_tests++)
		;

	new_test->name = name;
	new_test->ops  = ops;
	new_test->priority = priority;
	new_test->flags = flags;

	/* Add test, sorted on run order priority */
	fwts_list_add_ordered(fwts_framework_test_list, new_test, fwts_framework_compare_priority);

	/* Add any options and handler, if they exists */
	if (ops->options && ops->options_handler)
		fwts_args_add_options(ops->options, ops->options_handler, ops->options_check);
}

/*
 *  fwts_framework_compare_name()
 *	for sorting tests in name order
 */
int fwts_framework_compare_test_name(void *data1, void *data2)
{
	fwts_framework_test *test1 = (fwts_framework_test *)data1;
	fwts_framework_test *test2 = (fwts_framework_test *)data2;

	return strcmp(test1->name, test2->name);
}

/*
 *  fwts_framework_show_tests()
 *	dump out registered tests.
 */
static void fwts_framework_show_tests(fwts_framework *fw, bool full)
{
	fwts_list_link *item;
	fwts_list *sorted;
	int i;
	int need_nl = 0;
	int total = 0;

	typedef struct {
		const char *title;	/* Test category */
		const int  flag;	/* Mask of category */
	} fwts_categories;

	fwts_categories categories[] = {
		{ "Batch",			FWTS_BATCH },
		{ "Interactive",		FWTS_INTERACTIVE },
		{ "Batch Experimental",		FWTS_BATCH_EXPERIMENTAL },
		{ "Interactive Experimental",	FWTS_INTERACTIVE_EXPERIMENTAL },
		{ "Power States",		FWTS_POWER_STATES },
		{ "Utilities",			FWTS_UTILS },
		{ NULL,			0 },
	};

	/* Dump out tests registered under all categories */
	for (i=0; categories[i].title != NULL; i++) {
		fwts_framework_test *test;

		/* If no category flags are set, or category matches user requested
		   category go and dump name and purpose of tests */
		if (((fw->flags & FWTS_RUN_ALL_FLAGS) == 0) ||
		    ((fw->flags & FWTS_RUN_ALL_FLAGS) & categories[i].flag)) {
			if ((sorted = fwts_list_new()) == NULL) {
				fprintf(stderr, "FATAL: Could not sort sort tests by name, out of memory.");
				exit(EXIT_FAILURE);
			}
			fwts_list_foreach(item, fwts_framework_test_list) {
				test = fwts_list_data(fwts_framework_test *, item);
				if ((test->flags & FWTS_RUN_ALL_FLAGS) == categories[i].flag)
					fwts_list_add_ordered(sorted, fwts_list_data(fwts_framework_test *, item), 
								fwts_framework_compare_test_name);
			}

			if (fwts_list_len(sorted) > 0) {
				if (need_nl)
					printf("\n");
				need_nl = 1;
				printf("%s%s:\n", categories[i].title,
					categories[i].flag & FWTS_UTILS ? "" : " tests");
	
				fwts_list_foreach(item, sorted) {
					test = fwts_list_data(fwts_framework_test *, item);
					if (full) {
						int j;
						printf(" %-13.13s (%d test%s):\n",
							test->name, test->ops->total_tests,
							test->ops->total_tests > 1 ? "s" : "");
						for (j=0; j<test->ops->total_tests;j++)
							printf("  %s\n", test->ops->minor_tests[j].name);
						total += test->ops->total_tests;
					}
					else {
						printf(" %-13.13s %s\n", test->name, test->ops->headline());
					}
				}
			}
			fwts_list_free(sorted, NULL);
		}
	}
	if (full)
		printf("\nTotal of %d tests\n", total);
}

/*
 *  fwts_framework_strtrunc()
 *	truncate overlong string
 */
static void fwts_framework_strtrunc(char *dest, const char *src, int max)
{
	strncpy(dest, src, max);

	if ((strlen(src) > max) && (max > 3)) {
		dest[max-1] = 0;
		dest[max-2] = '.';
		dest[max-3] = '.';
	}
}

/*
 *  fwts_framework_format_results()
 *	format results into human readable summary.
 */
static void fwts_framework_format_results(char *buffer, int buflen, fwts_results const *results, bool include_zero_results)
{
	int n = 0;

	if (buflen)
		*buffer = 0;

	if ((include_zero_results || (results->passed > 0)) && (buflen > 0)) {
		n = snprintf(buffer, buflen, "%u passed", results->passed);
		buffer += n;
		buflen -= n;
	}
	if ((include_zero_results || (results->failed > 0)) && (buflen > 0)) {
		n = snprintf(buffer, buflen, "%s%u failed", n > 0 ? ", " : "", results->failed);
		buffer += n;
		buflen -= n;
	}
	if ((include_zero_results || (results->warning > 0)) && (buflen > 0)) {
		n = snprintf(buffer, buflen, "%s%u warnings", n > 0 ? ", " : "", results->warning);
		buffer += n;
		buflen -= n;
	}
	if ((include_zero_results || (results->aborted > 0)) && (buflen > 0)) {
		n = snprintf(buffer, buflen, "%s%u aborted", n > 0 ? ", " : "", results->aborted);
		buffer += n;
		buflen -= n;
	}
	if ((include_zero_results || (results->skipped > 0)) && (buflen > 0)) {
		n = snprintf(buffer, buflen, "%s%u skipped", n > 0 ? ", " : "", results->skipped);
		buffer += n;
		buflen -= n;
	}
	if ((include_zero_results || (results->infoonly > 0)) && (buflen > 0)) {
		snprintf(buffer, buflen, "%s%u informational", n > 0 ? ", " : "", results->infoonly);
	}
}

/*
 *  fwts_framework_minor_test_progress()
 *	output per test progress report or progress that can be pipe'd into
 *	dialog --guage
 *
 */
void fwts_framework_minor_test_progress(fwts_framework *fw, const int percent, const char *message)
{
	float major_percent;
	float minor_percent;
	float process_percent;
	float progress;
	int width = fwts_tty_width(fileno(stderr), 80);
	if (width > 256)
		width = 256;

	if (percent >=0 && percent <=100)
		fw->minor_test_progress = percent;

	major_percent = (float)100.0 / (float)fw->major_tests_total;
	minor_percent = ((float)major_percent / (float)fw->current_major_test->ops->total_tests);
	process_percent = ((float)minor_percent / 100.0);

	progress = (float)(fw->current_major_test_num-1) * major_percent;
	progress += (float)(fw->current_minor_test_num-1) * minor_percent;
	progress += (float)(percent) * process_percent;

	/* Feedback required? */
	if (fw->show_progress) {
		int percent;
		char buf[1024];
		char truncbuf[256];

		snprintf(buf, sizeof(buf), "%s %s",fw->current_minor_test_name, message);
		fwts_framework_strtrunc(truncbuf, buf, width-9);

		percent = (100 * (fw->current_minor_test_num-1) / fw->current_major_test->ops->total_tests) +
			  (fw->minor_test_progress / fw->current_major_test->ops->total_tests);
		fprintf(stderr, "  %-*.*s: %3.0f%%\r", width-9, width-9, truncbuf, progress);
		fflush(stderr);
	}

	/* Output for the dialog tool, dialog --title "fwts" --gauge "" 12 80 0 */
	if (fw->flags & FWTS_FRAMEWORK_FLAGS_SHOW_PROGRESS_DIALOG) {
		char buffer[128];

		fwts_framework_format_results(buffer, sizeof(buffer), &fw->total, true);

		fprintf(stdout, "XXX\n");
		fprintf(stdout, "%d\n", (int)progress);
		fprintf(stdout, "So far: %s\n\n", buffer);
		fprintf(stdout, "%s\n\n", fw->current_major_test->ops->headline());
		fprintf(stdout, "Running test #%d: %s\n",
			fw->current_major_test_num,
			fw->current_minor_test_name);
		fprintf(stdout, "XXX\n");
		fflush(stdout);
	}
}

/*
 *  fwts_framework_underline()
 *	underlining into log
 */
static inline void fwts_framework_underline(fwts_framework *fw, const int ch)
{
	fwts_log_underline(fw->results, ch);
}

/*
 *  fwts_framework_get_env()
 *	get a variable - if already fetched return cached value, otherwise
 *	try to gather from environment. If not in environment, return
 *	predefined default.
 */
static char *fwts_framework_get_env(const int env_id)
{
	int i;

	for (i=0;i<(int)sizeof(fwts_framework_settings)/sizeof(fwts_framework_setting);i++) {
		if (fwts_framework_settings[i].env_id == env_id) {	
			if (fwts_framework_settings[i].env_value)
				return fwts_framework_settings[i].env_value;
			else {
				const char *value = getenv(fwts_framework_settings[i].env_name);
				if (value == NULL) {
					value = fwts_framework_settings[i].env_default;
				}
				fwts_framework_settings[i].env_value = strdup(value);
				if (fwts_framework_settings[i].env_value)
					return fwts_framework_settings[i].env_value;
				else
					return "";
			}
		}
	}
	return "";
}

/*
 *  fwts_framework_free_env()
 *	free alloc'd environment variables
 */
static void fwts_framework_free_env(void)
{
	int i;

	for (i=0;i<(int)sizeof(fwts_framework_settings)/sizeof(fwts_framework_setting);i++)
		if (fwts_framework_settings[i].env_value)
			free(fwts_framework_settings[i].env_value);
}

static int fwts_framework_test_summary(fwts_framework *fw)
{
	char buffer[128];

	fwts_results const *results = &fw->current_major_test->results;

	fwts_framework_underline(fw,'=');
	fwts_framework_format_results(buffer, sizeof(buffer), results, true);
	fwts_log_summary(fw, "%s.", buffer);
	fwts_framework_underline(fw,'=');

	if (fw->flags & FWTS_FRAMEWORK_FLAGS_STDOUT_SUMMARY) {
		if (results->aborted > 0)
			printf("%s\n", fwts_framework_get_env(FWTS_ABORTED_TEXT));
		else if (results->skipped > 0)
			printf("%s\n", fwts_framework_get_env(FWTS_SKIPPED_TEXT));
		else if (results->failed > 0) {
			/* We intentionally report the highest logged error level */
			if (fw->failed_level & LOG_LEVEL_CRITICAL)
				printf("%s\n", fwts_framework_get_env(FWTS_FAILED_CRITICAL_TEXT));
			else if (fw->failed_level & LOG_LEVEL_HIGH)
				printf("%s\n", fwts_framework_get_env(FWTS_FAILED_HIGH_TEXT));
			else if (fw->failed_level & LOG_LEVEL_MEDIUM)
				printf("%s\n", fwts_framework_get_env(FWTS_FAILED_MEDIUM_TEXT));
			else if (fw->failed_level & LOG_LEVEL_LOW)
				printf("%s\n", fwts_framework_get_env(FWTS_FAILED_LOW_TEXT));
			else printf("%s\n", fwts_framework_get_env(FWTS_FAILED_TEXT));
		}
		else if (results->warning > 0)
			printf("%s\n", fwts_framework_get_env(FWTS_WARNING_TEXT));
		else
			printf("%s\n", fwts_framework_get_env(FWTS_PASSED_TEXT));
	}

	if (!(fw->flags & FWTS_FRAMEWORK_FLAGS_LP_TAGS))
		fwts_log_newline(fw->results);

	return FWTS_OK;
}

static int fwts_framework_total_summary(fwts_framework *fw)
{
	char buffer[128];

	fwts_framework_format_results(buffer, sizeof(buffer), &fw->total, true);
	fwts_log_summary(fw, "%s.", buffer);

	return FWTS_OK;
}

static int fwts_framework_run_test(fwts_framework *fw, const int num_tests, fwts_framework_test *test)
{		
	fwts_framework_minor_test *minor_test;	

	fw->current_major_test = test;
	fw->current_minor_test_name = "";
	fw->test_taglist = fwts_list_new();

	test->was_run = true;
	fw->total_run++;

	fwts_results_zero(&fw->current_major_test->results);

	fw->failed_level = 0;

	fwts_log_set_owner(fw->results, test->name);

	fw->current_minor_test_num = 1;
	fw->show_progress = (fw->flags & FWTS_FRAMEWORK_FLAGS_SHOW_PROGRESS) &&
			    (FWTS_TEST_INTERACTIVE(test->flags) == 0);

	/* Not a utility test?, then we require a test summary at end of the test run */
	if (!(test->flags & FWTS_UTILS))
		fw->print_summary = 1;

	if (test->ops->headline) {
		fwts_log_heading(fw, "%s", test->ops->headline());
		fwts_framework_underline(fw,'-');
		if (fw->show_progress) {
			char buf[70];
			fwts_framework_strtrunc(buf, test->ops->headline(), sizeof(buf));
			fprintf(stderr, "Test: %-70.70s\n", buf);
		}
	}

	fwts_framework_minor_test_progress(fw, 0, "");

	if (test->ops->init) {
		int ret;
		if ((ret = test->ops->init(fw)) != FWTS_OK) {
			/* Init failed or skipped, so abort */
			if (ret == FWTS_SKIP) {
				for (minor_test = test->ops->minor_tests; *minor_test->test_func != NULL; minor_test++) {
					fw->current_major_test->results.skipped++;
					fw->total.skipped++;
				}
				if (fw->show_progress)
					fprintf(stderr, " Test skipped.\n");

			} else {
				fwts_log_error(fw, "Aborted test, initialisation failed.");
				for (minor_test = test->ops->minor_tests; *minor_test->test_func != NULL; minor_test++) {
					fw->current_major_test->results.aborted++;
					fw->total.aborted++;
				}
				if (fw->show_progress)
					fprintf(stderr, " Test aborted.\n");
			}
			goto done;
		}
	}

	for (minor_test = test->ops->minor_tests; *minor_test->test_func != NULL; minor_test++, fw->current_minor_test_num++) {
		int ret;

		fw->current_minor_test_name = minor_test->name;

		fwts_results_zero(&fw->minor_tests);

		if (minor_test->name != NULL)
			fwts_log_info(fw, "Test %d of %d: %s",
				fw->current_minor_test_num,
				test->ops->total_tests, minor_test->name);

		fwts_framework_minor_test_progress(fw, 0, "");
		ret = (*minor_test->test_func)(fw);

		/* Something went horribly wrong, abort all other tests too */
		if (ret == FWTS_ABORTED)  {
			for (; *minor_test->test_func != NULL; minor_test++) {
				fw->current_major_test->results.aborted++;
				fw->total.aborted++;
			}
			break;
		}
		fwts_framework_minor_test_progress(fw, 100, "");
		fwts_framework_summate_results(&fw->current_major_test->results, &fw->minor_tests);

		if (fw->show_progress) {
			char resbuf[128];
			char namebuf[55];
			fwts_framework_format_results(resbuf, sizeof(resbuf), &fw->minor_tests, false);
			fwts_framework_strtrunc(namebuf, minor_test->name, sizeof(namebuf));
			fprintf(stderr, "  %-55.55s %s\n", namebuf,
				*resbuf ? resbuf : "     ");
		}
		fwts_log_nl(fw);
	}

	fwts_framework_summate_results(&fw->total, &fw->current_major_test->results);

	if (test->ops->deinit)
		test->ops->deinit(fw);

	if (fw->flags & FWTS_FRAMEWORK_FLAGS_LP_TAGS_LOG)
		fwts_tag_report(fw, LOG_TAG, fw->test_taglist);

done:
	fwts_list_free(fw->test_taglist, free);
	fw->test_taglist = NULL;

	if (!(test->flags & FWTS_UTILS))
		fwts_framework_test_summary(fw);

	fwts_log_set_owner(fw->results, "fwts");

	return FWTS_OK;
}

/*
 *  fwts_framework_tests_run()
 *	
 */
static void fwts_framework_tests_run(fwts_framework *fw, fwts_list *tests_to_run)
{
	fwts_list_link *item;

	fw->current_major_test_num = 1;
	fw->major_tests_total  = fwts_list_len(tests_to_run);

	fwts_list_foreach(item, tests_to_run) {
		fwts_framework_test *test = fwts_list_data(fwts_framework_test *, item);
		fwts_framework_run_test(fw, fwts_list_len(tests_to_run), test);
		fw->current_major_test_num++;
	}
}

/*
 *  fwts_framework_test_find()
 *	find a named test, return test if found, NULL otherwise
 */
static fwts_framework_test *fwts_framework_test_find(fwts_framework *fw, const char *name)
{
	fwts_list_link *item;
	
	fwts_list_foreach(item, fwts_framework_test_list) {
		fwts_framework_test *test = fwts_list_data(fwts_framework_test *, item);
		if (strcmp(name, test->name) == 0)
			return test;
	}

	return NULL;
}

/*
 *  fwts_framework_advice()
 *	log advice message
 */
void fwts_framework_advice(fwts_framework *fw, const char *fmt, ...)
{
	va_list ap;
	char buffer[4096];

	va_start(ap, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, ap);
	fwts_log_nl(fw);
	fwts_log_printf(fw->results, LOG_RESULT, LOG_LEVEL_NONE, "%s: %s",
		fwts_framework_get_env(FWTS_ADVICE_TEXT), buffer);
	fwts_log_nl(fw);
	va_end(ap);
}

/*
 *  fwts_framework_passed()
 *	log a passed test message
 */
void fwts_framework_passed(fwts_framework *fw, const char *fmt, ...)
{
	va_list ap;
	char buffer[4096];

	va_start(ap, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, ap);
	fw->minor_tests.passed++;
	fwts_log_printf(fw->results, LOG_RESULT, LOG_LEVEL_NONE, "%s: Test %d, %s",
		fwts_framework_get_env(FWTS_PASSED_TEXT), fw->current_minor_test_num, buffer);
	va_end(ap);
}

/*
 *  fwts_framework_failed()
 *	log a failed test message
 */
void fwts_framework_failed(fwts_framework *fw, fwts_log_level level, const char *fmt, ...)
{
	va_list ap;
	char buffer[4096];

	fw->failed_level |= level;

	va_start(ap, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, ap);
	fwts_summary_add(fw->current_major_test->name, level, buffer);
	fw->minor_tests.failed++;
	fwts_log_printf(fw->results, LOG_RESULT, level, "%s [%s]: Test %d, %s",
		fwts_framework_get_env(FWTS_FAILED_TEXT), fwts_log_level_to_str(level), fw->current_minor_test_num, buffer);
	va_end(ap);
}

/*
 *  fwts_framework_warning()
 *	log a warning message
 */
void fwts_framework_warning(fwts_framework *fw, const char *fmt, ...)
{
	va_list ap;
	char buffer[1024];

	va_start(ap, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, ap);
	fw->minor_tests.warning++;
	fwts_log_printf(fw->results, LOG_RESULT, LOG_LEVEL_MEDIUM, "%s: Test %d, %s",
		fwts_framework_get_env(FWTS_WARNING_TEXT), fw->current_minor_test_num, buffer);
	va_end(ap);
}

/*
 *  fwts_framework_skipped()
 *	log a skipped test message
 */
void fwts_framework_skipped(fwts_framework *fw, const char *fmt, ...)
{
	va_list ap;
	char buffer[1024];

	va_start(ap, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, ap);
	fw->minor_tests.skipped++;
	fwts_log_printf(fw->results, LOG_RESULT, LOG_LEVEL_MEDIUM, "%s: Test %d, %s",
		fwts_framework_get_env(FWTS_SKIPPED_TEXT), fw->current_minor_test_num, buffer);
	va_end(ap);
}

/*
 *  fwts_framework_aborted()
 *	log an aborted test message
 */
void fwts_framework_aborted(fwts_framework *fw, const char *fmt, ...)
{
	va_list ap;
	char buffer[1024];

	va_start(ap, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, ap);
	fw->minor_tests.aborted++;
	fwts_log_printf(fw->results, LOG_RESULT, LOG_LEVEL_MEDIUM, "%s: Test %d, %s",
		fwts_framework_get_env(FWTS_ABORTED_TEXT), fw->current_minor_test_num, buffer);
	va_end(ap);
}


/*
 *  fwts_framework_infoonly()
 *	mark a test as information only 
 */
void fwts_framework_infoonly(fwts_framework *fw)
{
	fw->minor_tests.infoonly++;
}

/*
 *  fwts_framework_show_version()
 *	dump version of fwts
 */
static void fwts_framework_show_version(char * const *argv)
{
	printf("%s, Version %s, %s\n", argv[0], FWTS_VERSION, FWTS_DATE);
}


/*
 *  fwts_framework_strdup()
 *	dup a string. if it's already allocated, free previous allocation before duping
 */
static void fwts_framework_strdup(char **ptr, const char *str)
{
	if (ptr == NULL)
		return;

	if (*ptr)
		free(*ptr);
	*ptr = strdup(str);
}

/*
 *  fwts_framework_syntax()
 *	dump some help
 */
static void fwts_framework_syntax(char * const *argv)
{
	int i;

	printf("Usage %s: [OPTION] [TEST]\n", argv[0]);

	fwts_args_show_options();
	
	/* Tag on copyright info */
	printf("\n");
	for (i=0; fwts_copyright[i]; i++) 
		printf("%s\n", fwts_copyright[i]);
}

/*
 * fwts_framework_heading_info()
 *	log basic system info so we can track the tests
 */
static void fwts_framework_heading_info(fwts_framework *fw, fwts_list *tests_to_run)
{
	struct tm tm;
	time_t now;
	struct utsname buf;
	char *tests = NULL;
	int len = 1;
	int i;
	fwts_list_link *item;

	time(&now);
	localtime_r(&now, &tm);

	uname(&buf);

	fwts_log_info(fw, "Results generated by fwts: Version %s (%s).", FWTS_VERSION, FWTS_DATE);
	fwts_log_nl(fw);
	for (i=0; fwts_copyright[i]; i++)
		fwts_log_info(fw, "%s", fwts_copyright[i]);
	fwts_log_nl(fw);

	fwts_log_info(fw, "This test run on %2.2d/%2.2d/%-2.2d at %2.2d:%2.2d:%2.2d on host %s %s %s %s %s.",
		tm.tm_mday, tm.tm_mon + 1, (tm.tm_year+1900) % 100,
		tm.tm_hour, tm.tm_min, tm.tm_sec,
		buf.sysname, buf.nodename, buf.release, buf.version, buf.machine);
	fwts_log_nl(fw);
	
	fwts_list_foreach(item, tests_to_run) {
		fwts_framework_test *test = fwts_list_data(fwts_framework_test *, item);
		len += strlen(test->name) + 1;
	}

	if ((tests = calloc(len, 1)) != NULL) {
		fwts_list_foreach(item, tests_to_run) {
			fwts_framework_test *test = fwts_list_data(fwts_framework_test *, item);
			if (item != fwts_list_head(tests_to_run))
				strcat(tests, " ");
			strcat(tests, test->name);
		}

		fwts_log_info(fw, "Running tests: %s.\n",
			fwts_list_len(tests_to_run) == 0 ? "None" : tests);
		if (!(fw->flags & FWTS_FRAMEWORK_FLAGS_LP_TAGS))
			fwts_log_newline(fw->results);
		free(tests);
	}
}

/*
 *  fwts_framework_skip_test()
 *	try to find a test in list of tests to be skipped, return NULL of cannot be found
 */
static fwts_framework_test *fwts_framework_skip_test(fwts_list *tests_to_skip, fwts_framework_test *test)
{
	fwts_list_link *item;

	fwts_list_foreach(item, tests_to_skip)
		if (test == fwts_list_data(fwts_framework_test *, item))
			return test;

	return NULL;
}

/*
 *  fwts_framework_skip_test_parse()
 *	parse optarg of comma separated list of tests to skip
 */
static int fwts_framework_skip_test_parse(fwts_framework *fw, const char *arg, fwts_list *tests_to_skip)
{
	char *str;
	char *token;
	char *saveptr = NULL;
	fwts_framework_test *test;

	for (str = (char*)arg; (token = strtok_r(str, ",", &saveptr)) != NULL; str = NULL) {
		if ((test = fwts_framework_test_find(fw, token)) == NULL) {
			fprintf(stderr, "No such test '%s'\n", token);
			return FWTS_ERROR;
		} else
			fwts_list_append(tests_to_skip, test);
	}

	return FWTS_OK;
}

int fwts_framework_options_handler(fwts_framework *fw, int argc, char * const argv[], int option_char, int long_index)
{
	switch (option_char) {
	case 0:
		switch (long_index) {
		case 0: /* --stdout-summary */
			fw->flags |= FWTS_FRAMEWORK_FLAGS_STDOUT_SUMMARY;
			break;	
		case 1: /* --help */
			fwts_framework_syntax(argv);
			return FWTS_COMPLETE;
		case 2: /* --results-output */
			fwts_framework_strdup(&fw->results_logname, optarg);
			break;
		case 3: /* --results-no-separators */
			fwts_log_filter_unset_field(LOG_SEPARATOR);
			break;
		case 4: /* --log-filter */
			fwts_log_filter_unset_field(~0);
			fwts_log_set_field_filter(optarg);
			break;
		case 5: /* --log-fields */
			fwts_log_print_fields();
			return FWTS_COMPLETE;
		case 6: /* --log-format */
			fwts_log_set_format(optarg);
			break;	
		case 7: /* --show-progress */
			fw->flags = (fw->flags &
					~(FWTS_FRAMEWORK_FLAGS_QUIET |
					  FWTS_FRAMEWORK_FLAGS_SHOW_PROGRESS_DIALOG))
					| FWTS_FRAMEWORK_FLAGS_SHOW_PROGRESS;
			break;
		case 8: /* --show-tests */
			fw->flags |= FWTS_FRAMEWORK_FLAGS_SHOW_TESTS;
			break;
		case 9: /* --klog */
			fwts_framework_strdup(&fw->klog, optarg);
			break;
		case 10: /* --dmidecode */
			fwts_framework_strdup(&fw->dmidecode, optarg);
			break;
		case 11: /* --log-width=N */
			fwts_log_set_line_width(atoi(optarg));
			break;
		case 12: /* --lspci=pathtolspci */
			fwts_framework_strdup(&fw->lspci, optarg);
			break;
		case 13: /* --batch */
			fw->flags |= FWTS_FRAMEWORK_FLAGS_BATCH;
			break;
		case 14: /* --interactive */
			fw->flags |= FWTS_FRAMEWORK_FLAGS_INTERACTIVE;
			break;
		case 15: /* --force-clean */
			fw->flags |= FWTS_FRAMEWORK_FLAGS_FORCE_CLEAN;
			break;
		case 16: /* --version */
			fwts_framework_show_version(argv);
			return FWTS_COMPLETE;
		case 17: /* --dump */
			fwts_dump_info(fw, NULL);
			return FWTS_COMPLETE;
		case 18: /* --table-path */
			fwts_framework_strdup(&fw->acpi_table_path, optarg);
			break;
		case 19: /* --batch-experimental */
			fw->flags |= FWTS_FRAMEWORK_FLAGS_BATCH_EXPERIMENTAL;
			break;
		case 20: /* --interactive-experimental */
			fw->flags |= FWTS_FRAMEWORK_FLAGS_INTERACTIVE_EXPERIMENTAL;
			break;
		case 21: /* --power-states */
			fw->flags |= FWTS_FRAMEWORK_FLAGS_POWER_STATES;
			break;
		case 22: /* --all */
			fw->flags |= FWTS_RUN_ALL_FLAGS;
			break;
		case 23: /* --show-progress-dialog */
			fw->flags = (fw->flags &
					~(FWTS_FRAMEWORK_FLAGS_QUIET |
					  FWTS_FRAMEWORK_FLAGS_SHOW_PROGRESS))
					| FWTS_FRAMEWORK_FLAGS_SHOW_PROGRESS_DIALOG;
			break;
		case 24: /* --skip-test */
			if (fwts_framework_skip_test_parse(fw, optarg, tests_to_skip) != FWTS_OK)
				return FWTS_COMPLETE;
			break;
		case 25: /* --quiet */
			fw->flags = (fw->flags &
					~(FWTS_FRAMEWORK_FLAGS_SHOW_PROGRESS |
					  FWTS_FRAMEWORK_FLAGS_SHOW_PROGRESS_DIALOG))
					| FWTS_FRAMEWORK_FLAGS_QUIET;
			break;
		case 26: /* --dumpfile */
			fwts_framework_strdup(&fw->acpi_table_acpidump_file, optarg);
			break;
		case 27: /* --lp-tags */
			fw->flags |= FWTS_FRAMEWORK_FLAGS_LP_TAGS;
			fwts_log_filter_unset_field(~0);
			fwts_log_filter_set_field(LOG_TAG);
			break;
		case 28: /* --show-tests-full */
			fw->flags |= FWTS_FRAMEWORK_FLAGS_SHOW_TESTS_FULL;
			break;
		case 29: /* --utils */
			fw->flags |= FWTS_FRAMEWORK_FLAGS_UTILS;
			break;
		case 30: /* --json-data-path */
			fwts_framework_strdup(&fw->json_data_path, optarg);
			break;
		case 31: /* --lp-tags-log */
			fw->flags |= FWTS_FRAMEWORK_FLAGS_LP_TAGS_LOG;
			break;
		case 32: /* --disassemble-aml */
			fwts_iasl_disassemble_all_to_file(fw);
			return FWTS_COMPLETE;
		}
		break;
	case 'a': /* --all */
		fw->flags |= FWTS_RUN_ALL_FLAGS;
		break;
	case 'b': /* --batch */
		fw->flags |= FWTS_FRAMEWORK_FLAGS_BATCH;
		break;
	case 'd': /* --dump */
		fwts_dump_info(fw, NULL);
		return FWTS_COMPLETE;
	case 'D': /* --show-progress-dialog */
		fw->flags = (fw->flags &
				~(FWTS_FRAMEWORK_FLAGS_QUIET |
				  FWTS_FRAMEWORK_FLAGS_SHOW_PROGRESS))
				| FWTS_FRAMEWORK_FLAGS_SHOW_PROGRESS_DIALOG;
		break;
	case 'f':
		fw->flags |= FWTS_FRAMEWORK_FLAGS_FORCE_CLEAN;
		break;
	case 'h':
	case '?':
		fwts_framework_syntax(argv);
		return FWTS_COMPLETE;
	case 'i': /* --interactive */
		fw->flags |= FWTS_FRAMEWORK_FLAGS_INTERACTIVE;
		break;
	case 'j': /* --json-data-path */
		fwts_framework_strdup(&fw->json_data_path, optarg);
		break;
	case 'k': /* --klog */
		fwts_framework_strdup(&fw->klog, optarg);
		break;
	case 'l': /* --lp-flags */
		fw->flags |= FWTS_FRAMEWORK_FLAGS_LP_TAGS;
		break;
	case 'p': /* --show-progress */
		fw->flags = (fw->flags &
				~(FWTS_FRAMEWORK_FLAGS_QUIET |
				  FWTS_FRAMEWORK_FLAGS_SHOW_PROGRESS_DIALOG))
				| FWTS_FRAMEWORK_FLAGS_SHOW_PROGRESS;
			break;
	case 'P': /* --power-states */
		fw->flags |= FWTS_FRAMEWORK_FLAGS_POWER_STATES;
		break;
	case 'q': /* --quiet */
		fw->flags = (fw->flags &
				~(FWTS_FRAMEWORK_FLAGS_SHOW_PROGRESS |
				  FWTS_FRAMEWORK_FLAGS_SHOW_PROGRESS_DIALOG))
				| FWTS_FRAMEWORK_FLAGS_QUIET;
		break;
	case 'r': /* --results-output */
		fwts_framework_strdup(&fw->results_logname, optarg);
		break;
	case 's': /* --show-tests */
		fw->flags |= FWTS_FRAMEWORK_FLAGS_SHOW_TESTS;
		break;
	case 'S': /* --skip-test */
		if (fwts_framework_skip_test_parse(fw, optarg, tests_to_skip) != FWTS_OK)
			return FWTS_COMPLETE;
		break;
	case 't': /* --table-path */
		fwts_framework_strdup(&fw->acpi_table_path, optarg);
		break;
	case 'u': /* --utils */
		fw->flags |= FWTS_FRAMEWORK_FLAGS_UTILS;
		break;
	case 'v': /* --version */
		fwts_framework_show_version(argv);
		return FWTS_COMPLETE;
	case 'w': /* --log-width=N */
		fwts_log_set_line_width(atoi(optarg));
		break;
	}
	return FWTS_OK;
}

/*
 *  fwts_framework_args()
 *	parse args and run tests
 */
int fwts_framework_args(const int argc, char **argv)
{
	int ret = FWTS_OK;
	int i;

	fwts_framework *fw;

	if ((fw = (fwts_framework *)calloc(1, sizeof(fwts_framework))) == NULL)
		return FWTS_ERROR;

	fwts_args_add_options(fwts_framework_options, fwts_framework_options_handler, NULL);

	fw->firmware_type = fwts_firmware_detect();

	fw->magic = FWTS_FRAMEWORK_MAGIC;
	fw->flags = FWTS_FRAMEWORK_FLAGS_DEFAULT |
		    FWTS_FRAMEWORK_FLAGS_SHOW_PROGRESS;

	fw->total_taglist = fwts_list_new();

	fwts_summary_init();

	fwts_framework_strdup(&fw->dmidecode, FWTS_DMIDECODE_PATH);
	fwts_framework_strdup(&fw->lspci, FWTS_LSPCI_PATH);
	fwts_framework_strdup(&fw->results_logname, RESULTS_LOG);
	fwts_framework_strdup(&fw->json_data_path, FWTS_JSON_DATA_PATH);

	tests_to_run  = fwts_list_new();
	tests_to_skip = fwts_list_new();

	if ((tests_to_run == NULL) || (tests_to_skip == NULL)) {
		fwts_log_error(fw, "Run out of memory preparing to run tests.");
		goto tidy_close;
	}

	if (fwts_args_parse(fw, argc, argv) != FWTS_OK)
		goto tidy_close;

	for (i=1; i<argc; i++)
		if (!strcmp(argv[i], "-")) {
			fwts_framework_strdup(&fw->results_logname, "stdout");
			fw->flags = (fw->flags &
					~(FWTS_FRAMEWORK_FLAGS_SHOW_PROGRESS |
					  FWTS_FRAMEWORK_FLAGS_SHOW_PROGRESS_DIALOG))
					| FWTS_FRAMEWORK_FLAGS_QUIET;
			break;
		}

	if (fw->flags & FWTS_FRAMEWORK_FLAGS_SHOW_TESTS) {
		fwts_framework_show_tests(fw, false);
		goto tidy_close;
	}
	if (fw->flags & FWTS_FRAMEWORK_FLAGS_SHOW_TESTS_FULL) {
		fwts_framework_show_tests(fw, true);
		goto tidy_close;
	}
	if ((fw->flags & FWTS_RUN_ALL_FLAGS) == 0)
		fw->flags |= FWTS_FRAMEWORK_FLAGS_BATCH;
	if ((fw->dmidecode == NULL) ||
	    (fw->lspci == NULL) ||
	    (fw->results_logname == NULL)) {
		ret = FWTS_ERROR;
		fprintf(stderr, "%s: Memory allocation failure.", argv[0]);
		goto tidy_close;
	}

	/* Results log */
	if ((fw->results = fwts_log_open("fwts",
			fw->results_logname,
			fw->flags & FWTS_FRAMEWORK_FLAGS_FORCE_CLEAN ? "w" : "a")) == NULL) {
		ret = FWTS_ERROR;
		fprintf(stderr, "%s: Cannot open results log '%s'.\n", argv[0], fw->results_logname);
		goto tidy_close;
	}

	/* Run specified tests */
	for (i=1; i < argc; i++) {
		if (*argv[i] == '-')
			continue;

		fwts_framework_test *test = fwts_framework_test_find(fw, argv[i]);

		if (test == NULL) {
			int width = fwts_tty_width(fileno(stderr), 80);
			int n = 0;
			fwts_list_link *item;
			fprintf(stderr, "No such test '%s', available tests:\n",argv[i]);

			fwts_list_foreach(item, fwts_framework_test_list) {
				int len;
				fwts_framework_test *test = fwts_list_data(fwts_framework_test*, item);
				len = strlen(test->name) + 1;
				if ((n + len) > width)  {
					fprintf(stderr, "\n");
					n = 0;
				}
				
				fprintf(stderr, "%s ", test->name);
				n += len;
			}
			fprintf(stderr, "\n\nuse: fwts --show-tests or fwts --show-tests-full for more information.\n");
			
			ret = FWTS_ERROR;
			goto tidy;
		}

		if (fwts_framework_skip_test(tests_to_skip, test) == NULL)
			fwts_list_append(tests_to_run, test);
	}

	if (fwts_list_len(tests_to_run) == 0) {
		/* Find tests that are eligible for running */
		fwts_list_link *item;
		fwts_list_foreach(item, fwts_framework_test_list) {
			fwts_framework_test *test = fwts_list_data(fwts_framework_test*, item);
			if (fw->flags & test->flags & FWTS_RUN_ALL_FLAGS)
				if (fwts_framework_skip_test(tests_to_skip, test) == NULL)
					fwts_list_append(tests_to_run, test);
		}
	}

	if (!(fw->flags & FWTS_FRAMEWORK_FLAGS_QUIET))
		printf("Running %d tests, results appended to %s\n",
			fwts_list_len(tests_to_run),
			fw->results_logname);

	fwts_framework_heading_info(fw, tests_to_run);
	fwts_framework_tests_run(fw, tests_to_run);

	if (fw->print_summary) {
		fwts_log_set_owner(fw->results, "summary");
		fwts_log_nl(fw);
		if (fw->flags & FWTS_FRAMEWORK_FLAGS_LP_TAGS_LOG)
			fwts_tag_report(fw, LOG_SUMMARY, fw->total_taglist);
		fwts_list_free(fw->test_taglist, free);
		fwts_framework_total_summary(fw);
		fwts_log_nl(fw);
		fwts_summary_report(fw, fwts_framework_test_list);
	}

	if (fw->flags & FWTS_FRAMEWORK_FLAGS_LP_TAGS)
		fwts_tag_report(fw, LOG_TAG | LOG_NO_FIELDS, fw->total_taglist);

tidy:
	fwts_list_free(tests_to_skip, NULL);
	fwts_list_free(tests_to_run, NULL);
	fwts_log_close(fw->results);

tidy_close:
	fwts_acpi_free_tables();
	fwts_summary_deinit();
	fwts_args_free();

	free(fw->dmidecode);
	free(fw->lspci);
	free(fw->results_logname);
	free(fw->klog);
	free(fw->json_data_path);
	fwts_framework_free_env();
	fwts_list_free(fw->total_taglist, free);
	fwts_list_free(fwts_framework_test_list, free);

	/* Failed tests flagged an error */
	if ((fw->total.failed > 0) ||
	    (fw->total.warning > 0))	
		ret = FWTS_ERROR;

	free(fw);
	
	return ret;
}
