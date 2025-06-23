#define _GNU_SOURCE

#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define FHTTPD_LOG_MODULE_NAME "autoindex"

#include "connection.h"
#include "log.h"
#include "protocol.h"
#include "utils.h"

#ifdef HAVE_RESOURCES
#include "resources.h"
#endif

struct autoindex_icon
{
	const char *name;
	const char *alt;
	const char **exts; /* Must end with a NUL byte */
};

static struct autoindex_icon const autoindex_icons[] = {
	{
		"text",
		"FILE",
		(const char *[]) { "txt", "text", NULL },
	},
};

static int
fhttpd_autoindex_sort (const struct dirent **e1, const struct dirent **e2)
{
	const struct dirent *entry1 = *e1;
	const struct dirent *entry2 = *e2;

	if (entry1->d_type == DT_DIR && entry2->d_type != DT_DIR)
		return -1; /* Directories first */

	if (entry1->d_type != DT_DIR && entry2->d_type == DT_DIR)
		return 1; /* Directories first */

	return versionsort (e1, e2); /* Sort by name */
}

static bool
fhttpd_autoindex_get_icon (const struct dirent *entry, const struct stat *st, char icon_name[static 32],
						   char icon_alt[static 16])
{
	if (S_ISDIR (st->st_mode))
	{
		strncpy (icon_name, "folder", 32);
		strncpy (icon_alt, "DIR", 16);
		return true;
	}

	strncpy (icon_alt, "FILE", 16);
	const char *ext = get_file_extension (entry->d_name);

	if (!ext)
	{
		strncpy (icon_name, "file", 32);
		return true;
	}

	for (size_t i = 0; i < sizeof (autoindex_icons) / sizeof (autoindex_icons[0]); i++)
	{
		const char **exts = autoindex_icons[i].exts;

		while (*exts)
		{
			if (strcmp (*exts, ext) == 0)
			{
				strncpy (icon_name, autoindex_icons[i].name, 32);
				strncpy (icon_alt, autoindex_icons[i].alt, 16);
				return true;
			}

			exts++;
		}
	}

	strncpy (icon_name, "file", 32);
	return true;
}

bool
fhttpd_autoindex (const struct fhttpd_request *request, struct fhttpd_response *response, const char *filepath,
				  size_t filepath_len __attribute_maybe_unused__)
{
#ifndef NDEBUG
	uint64_t start = get_current_timestamp ();
#endif /* NDEBUG */
	
	fhttpd_header_add (&response->headers, "Content-Type", "text/html; charset=UTF-8", 12, 24);

	size_t buf_size = 128, buf_len = 0;
	char *buf = malloc (buf_size);

	if (!buf)
	{
		fhttpd_wclog_error ("Failed to allocate memory for directory listing buffer");
		response->ready = true;
		response->status = FHTTPD_STATUS_INTERNAL_SERVER_ERROR;
		response->use_builtin_error_response = true;
		return true;
	}

	struct dirent **entries;
	size_t total_entries = 0;

	bool is_root = request->path_len == 1 && request->path[0] == '/';

	if (!is_root)
	{
		const char parent_entry[] = "<tr><td><img src=\"/icons/folder.png\" alt=\"[DIR]\" /></td><td colspan=\"3\">"
									"<a href=\"../\">Parent Directory</a>"
									"</td></tr>\n";
		const size_t parent_entry_len = sizeof (parent_entry) - 1;
		static_assert ((sizeof (parent_entry) - 1) < 128, "Parent entry length must be less than 128 bytes");
		memcpy (buf, parent_entry, parent_entry_len);
		buf_len += parent_entry_len;
	}

	int entry_count = scandir (filepath, &entries, NULL, &fhttpd_autoindex_sort);

	if (entry_count < 0 || !entries)
	{
		int err = errno;
		fhttpd_wclog_error ("Failed to open directory '%s': %s", filepath, strerror (err));
		response->ready = true;
		response->status = err == EACCES ? FHTTPD_STATUS_FORBIDDEN : FHTTPD_STATUS_INTERNAL_SERVER_ERROR;
		response->use_builtin_error_response = true;
		free (buf);
		return true;
	}

	for (int i = 0; i < entry_count; i++)
	{
		struct dirent *entry = entries[i];

		if (strcmp (entry->d_name, ".") == 0 || strcmp (entry->d_name, "..") == 0)
		{
			free (entry);
			continue;
		}

		char fullpath[PATH_MAX + 1] = { 0 };
		snprintf (fullpath, sizeof (fullpath), "%s/%s", filepath, entry->d_name);

		struct stat entry_st;

		if (lstat (fullpath, &entry_st) < 0)
		{
			free (entry);
			continue;
		}

		size_t entry_name_len = strlen (entry->d_name);

		char size_buf[64];
		char lastmod_buf[64];
		char icon_name[32] = { 0 };
		char icon_alt[16] = { 0 };

		fhttpd_autoindex_get_icon (entry, &entry_st, icon_name, icon_alt);
		strftime (lastmod_buf, sizeof (lastmod_buf), "%Y-%m-%d %H:%M:%S", localtime (&entry_st.st_mtime));

		if (entry->d_type != DT_REG || !format_size (entry_st.st_size, size_buf, NULL, NULL))
			strcpy (size_buf, "-");

		size_t entry_str_len = (entry_name_len * 2) + 100 + (entry->d_type == DT_DIR ? 2 : 0) + strlen (size_buf)
							   + strlen (lastmod_buf) + strlen (icon_name) + strlen (icon_alt);

		if (buf_len + entry_str_len >= buf_size)
		{
			buf_size += entry_str_len >= 128 ? entry_str_len : 128;
			char *new_buf = realloc (buf, buf_size);

			if (!new_buf)
			{
				fhttpd_wclog_error ("Failed to reallocate memory for directory listing buffer");

				for (int j = i; j < entry_count; j++)
					free (entries[j]);

				free (entries);
				free (buf);

				response->ready = true;
				response->status = FHTTPD_STATUS_INTERNAL_SERVER_ERROR;
				response->use_builtin_error_response = true;
				return true;
			}

			buf = new_buf;
		}
		const char *format = "<tr><td><img src=\"/icons/%s.png\" alt=\"[%s]\" /></td><td>"
							 "<a href=\"%s%s\">%s%s</a>"
							 "</td><td>%s</td><td>%s</td></tr>\n";

		int bytes_written = snprintf (buf + buf_len, entry_str_len, format, icon_name, icon_alt, entry->d_name,
									  entry->d_type == DT_DIR ? "/" : "", entry->d_name,
									  entry->d_type == DT_DIR ? "/" : "", size_buf, lastmod_buf);

		if (bytes_written < 0 || (size_t) bytes_written >= entry_str_len)
		{
			fhttpd_wclog_error ("Failed to format directory entry '%s': %s", entry->d_name, strerror (errno));

			for (int j = i; j < entry_count; j++)
				free (entries[j]);

			free (entries);
			free (buf);

			response->ready = true;
			response->status = FHTTPD_STATUS_INTERNAL_SERVER_ERROR;
			response->use_builtin_error_response = true;
			return true;
		}

		buf_len += bytes_written;
		total_entries++;

		free (entries[i]);
		entries[i] = NULL;
	}

	free (entries);

	if (buf_len >= buf_size)
		buf = realloc (buf, buf_len + 1);

	buf[buf_len] = 0;

	size_t html_len = resource_index_html_len + buf_len + 1 - 10 + (request->path_len * 2) + (request->conn->port / 10)
					  + 1 + request->conn->hostname_len;
	char *html = malloc (html_len);

	if (!html)
	{
		fhttpd_wclog_error ("Failed to allocate memory for directory listing HTML");
		free (buf);
		response->ready = true;
		response->status = FHTTPD_STATUS_INTERNAL_SERVER_ERROR;
		response->use_builtin_error_response = true;
		return true;
	}

	char format[resource_index_html_len + 1];
	strncpy (format, resource_index_html, resource_index_html_len);
	format[resource_index_html_len] = 0;

	int bytes_written = snprintf (html, html_len - 1, format, request->path, request->path, buf,
								  request->conn->hostname, request->conn->port);

	if (bytes_written < 0 || (size_t) bytes_written >= html_len)
	{
		fhttpd_wclog_error ("Failed to format directory listing HTML: %s", strerror (errno));
		free (buf);
		free (html);
		response->ready = true;
		response->status = FHTTPD_STATUS_INTERNAL_SERVER_ERROR;
		response->use_builtin_error_response = true;
		return true;
	}

	response->status = FHTTPD_STATUS_OK;
	response->ready = true;
	response->body = request->method == FHTTPD_METHOD_HEAD ? NULL : (uint8_t *) html;
	response->body_len = request->method == FHTTPD_METHOD_HEAD ? 0 : (size_t) bytes_written;
	response->fd = -1;
	response->set_content_length = request->method != FHTTPD_METHOD_HEAD;
	response->use_builtin_error_response = false;

	if (request->method == FHTTPD_METHOD_HEAD)
	{
		free (html);
        fhttpd_header_add_printf (&response->headers, "Content-Length", 14, "%d", bytes_written);
	}

	free (buf);

#ifndef NDEBUG
	fhttpd_wclog_debug ("Indexed directory '%s' in %lums", filepath, get_current_timestamp() - start);
#endif /* NDEBUG */

	return true;
}