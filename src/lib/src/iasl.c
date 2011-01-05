/*
 * Copyright (C) 2010 Canonical
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
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <limits.h>

#include "fwts.h"
#include "fwts_iasl_interface.h"

int fwts_iasl_disassemble(fwts_framework *fw, const char *tablename, const int which, fwts_list **iasl_output)
{
	fwts_acpi_table_info *table;
	char tmpfile[PATH_MAX];
	char amlfile[PATH_MAX];
	int fd;
	int pid = getpid();

	if (iasl_output == NULL)
		return FWTS_ERROR;

	*iasl_output = NULL;

	if (fwts_acpi_find_table(fw, tablename, which, &table) != FWTS_OK)
		return FWTS_ERROR;

	if (table == NULL)
		return FWTS_OK;

	snprintf(tmpfile, sizeof(tmpfile), "/tmp/fwts_iasl_%d_%s.dsl", pid, tablename);
	snprintf(amlfile, sizeof(tmpfile), "/tmp/fwts_iasl_%d_%s.dat", pid, tablename);

	if ((fd = open(amlfile, O_WRONLY | O_CREAT | O_EXCL, S_IWUSR | S_IRUSR)) < 0) {
		fwts_log_error(fw, "Cannot create temporary file %s", amlfile);
		return FWTS_ERROR;
	}

	if (write(fd, table->data, table->length) != table->length) {
		fwts_log_error(fw, "Cannot write all data to temporary file");
		close(fd);
		unlink(amlfile);
		return FWTS_ERROR;
	}	
	close(fd);

	if (fwts_iasl_disassemble_aml(amlfile, tmpfile) < 0) {
		unlink(tmpfile);
		unlink(amlfile);
		return FWTS_ERROR;
	}

	unlink(amlfile);
	*iasl_output = fwts_file_open_and_read(tmpfile);
	unlink(tmpfile);

	return FWTS_OK;
}

int fwts_iasl_reassemble(fwts_framework *fw, const uint8_t *data, const int len, fwts_list **iasl_output)
{
	char tmpfile[PATH_MAX];
	char amlfile[PATH_MAX];
	char *output = NULL;
	int fd;
	int pid = getpid();

	if (iasl_output == NULL)
		return FWTS_ERROR;

	*iasl_output = NULL;

	snprintf(tmpfile, sizeof(tmpfile), "/tmp/fwts_iasl_%d.dsl", pid);
	snprintf(amlfile, sizeof(tmpfile), "/tmp/fwts_iasl_%d.dat", pid);

	if ((fd = open(amlfile, O_WRONLY | O_CREAT | O_EXCL, S_IWUSR | S_IRUSR)) < 0) {
		fwts_log_error(fw, "Cannot create temporary file %s", amlfile);
		return FWTS_ERROR;
	}

	if (write(fd, data, len) != len) {
		fwts_log_error(fw, "Cannot write all data to temporary file");
		close(fd);
		unlink(amlfile);
		return FWTS_ERROR;
	}	
	close(fd);

	if (fwts_iasl_disassemble_aml(amlfile, tmpfile) < 0) {
		unlink(tmpfile);
		unlink(amlfile);
		return FWTS_ERROR;
	}
	unlink(amlfile);

	if (fwts_iasl_assemble_aml(tmpfile, &output) < 0) {
		unlink(tmpfile);
		free(output);
		return FWTS_ERROR;
	}

	unlink(tmpfile);
	*iasl_output = fwts_list_from_text(output);
	free(output);

	return FWTS_OK;
}
