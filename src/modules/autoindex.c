#define _GNU_SOURCE

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#ifdef HAVE_RESOURCES
	#include "resources.h"
#endif /* HAVE_RESOURCES */

#define FH_LOG_MODULE_NAME "autoindex"

#include "autoindex.h"
#include "log/log.h"

static struct fh_link end_chunk_link = {
	.is_eos = true,
	.is_start = false,
	.next = NULL,
	.buf = & (struct fh_buf) {
		.type = FH_BUF_DATA,
		.freeable = false,
		.attrs.mem.cap = 5,
		.attrs.mem.len = 5,
		.attrs.mem.rd_only = true,
		.attrs.mem.data = (uint8_t *) "0\r\n\r\n", 
	},
};

#define FILEINFO_SUCCESS 0
#define FILEINFO_NEXT 1
#define FILEINFO_EXIT 2

struct autoindex_fileinfo
{
	char full_path[PATH_MAX + 1];
	struct stat64 st;
	struct tm *tm;
	char time_str[32];
	char size_buf[64];
	size_t size_buf_len, time_len;
};

static int
fh_autoindex_get_file_info (struct autoindex_fileinfo *afo,
							struct fh_response *response, const char *dir_name,
							const struct dirent64 *entry)
{
	int full_path_size = snprintf (afo->full_path, sizeof (afo->full_path), "%s/%s",
								   dir_name, entry->d_name);

	if (full_path_size < 0 || (size_t) full_path_size >= PATH_MAX)
	{
		response->status = FH_STATUS_INTERNAL_SERVER_ERROR;
		return FILEINFO_EXIT;
	}

	if (stat64 (afo->full_path, &afo->st) < 0)
	{
		fh_pr_debug ("Failed to stat '%s': %s", afo->full_path,
					 strerror (errno));
		return FILEINFO_NEXT;
	}

	time_t mtime = afo->st.st_mtim.tv_sec;
	afo->tm = localtime (&mtime);

	if (!afo->tm)
	{
		fh_pr_debug ("Failed to get mtime for '%s': %s", afo->full_path,
					 strerror (errno));
		return FILEINFO_NEXT;
	}

	afo->time_len = strftime (afo->time_str, (sizeof afo->time_str) - 1,
							  "%Y-%m-%d %H:%M:%S", afo->tm);

	if (afo->time_len == 0)
	{
		fh_pr_debug ("Failed to format mtime for '%s': %s", afo->full_path,
					 strerror (errno));
		return FILEINFO_NEXT;
	}

	if (entry->d_type != DT_DIR)
	{
		if (!format_size ((size_t) afo->st.st_size, afo->size_buf, NULL, NULL,
						  &afo->size_buf_len))
		{
			response->status = FH_STATUS_INTERNAL_SERVER_ERROR;
			return FILEINFO_EXIT;
		}
	}

	return FILEINFO_SUCCESS;
}

static bool
fh_autoindex_handle_chunked (struct fh_autoindex *autoindex)
{
	const struct fh_request *request = autoindex->request;
	struct fh_response *response = autoindex->response;

	if (request->method == FH_METHOD_HEAD)
	{
		response->status = FH_STATUS_OK;
		response->encoding = FH_ENCODING_CHUNKED;
		response->body_start = NULL;
		response->use_default_error_response = false;
		return true;
	}

	if (request->method != FH_METHOD_GET)
	{
		response->status = FH_STATUS_METHOD_NOT_ALLOWED;
		return true;
	}

	pool_t *pool = autoindex->response->pool;
	uint16_t port = autoindex->conn->extra->port;
	size_t index_start_chunk_len
		= resource_index_start_html_len - 12 + (3 * request->uri_len);
	size_t index_end_chunk_len = resource_index_end_html_len - 6
								 + autoindex->conn->extra->host_len
								 + (port < 10	   ? 1
									: port < 100   ? 2
									: port < 1000  ? 3
									: port < 10000 ? 4
												   : 5);

	size_t index_start_len = index_start_chunk_len + 32 + 4;
	size_t index_end_len = index_end_chunk_len + 32 + 4;

	struct fh_link *end_link;
	struct fh_link *start_link
		= fh_pool_alloc (pool, (sizeof (*start_link) + sizeof (*start_link->buf)
								+ index_start_len + 1 + sizeof (*end_link)
								+ sizeof (*end_link->buf) + index_end_len + 1));

	if (!start_link)
	{
		response->status = FH_STATUS_INTERNAL_SERVER_ERROR;
		return true;
	}

	start_link->buf = (struct fh_buf *) (start_link + 1);
	start_link->buf->attrs.mem.data = (uint8_t *) (start_link->buf + 1);
	start_link->buf->type = FH_BUF_DATA;
	start_link->is_eos = false;
	start_link->is_start = true;

	end_link = (struct fh_link *) (start_link->buf->attrs.mem.data
								   + index_start_len + 1);
	end_link->buf = (struct fh_buf *) (end_link + 1);
	end_link->buf->attrs.mem.data = (uint8_t *) (end_link->buf + 1);
	end_link->buf->type = FH_BUF_DATA;
	end_link->next = NULL;
	end_link->is_eos = false;

	end_chunk_link.buf->attrs.mem.data = (uint8_t *) "0\r\n\r\n";
	end_chunk_link.buf->attrs.mem.cap = end_chunk_link.buf->attrs.mem.len = 5;

	int rc1, rc2;

	if ((rc1 = snprintf ((char *) start_link->buf->attrs.mem.data, 34,
						 "%zx\r\n", index_start_chunk_len))
		< 0)
	{
		response->status = FH_STATUS_INTERNAL_SERVER_ERROR;
		return true;
	}

	if ((rc2 = snprintf ((char *) start_link->buf->attrs.mem.data + rc1,
						 index_start_len + 1 - rc1, resource_index_start_html,
						 (int) request->uri_len, request->uri,
						 (int) request->uri_len, request->uri,
						 (int) request->uri_len, request->uri))
		< 0)
	{
		response->status = FH_STATUS_INTERNAL_SERVER_ERROR;
		return true;
	}

	start_link->buf->attrs.mem.data[rc1 + rc2] = '\r';
	start_link->buf->attrs.mem.data[rc1 + rc2 + 1] = '\n';
	start_link->buf->attrs.mem.cap = start_link->buf->attrs.mem.len
		= (size_t) (rc1 + rc2 + 2);

	if ((rc1 = snprintf ((char *) end_link->buf->attrs.mem.data, 34, "%zx\r\n",
						 index_end_chunk_len))
		< 0)
	{
		response->status = FH_STATUS_INTERNAL_SERVER_ERROR;
		return true;
	}

	if ((rc2 = snprintf ((char *) end_link->buf->attrs.mem.data + rc1,
						 index_end_len + 1 - rc1, resource_index_end_html,
						 (int) autoindex->conn->extra->host_len,
						 autoindex->conn->extra->host, port))
		< 0)
	{
		response->status = FH_STATUS_INTERNAL_SERVER_ERROR;
		return true;
	}

	end_link->buf->attrs.mem.data[rc1 + rc2] = '\r';
	end_link->buf->attrs.mem.data[rc1 + rc2 + 1] = '\n';
	end_link->buf->attrs.mem.cap = end_link->buf->attrs.mem.len
		= (size_t) (rc1 + rc2 + 2);

	struct dirent64 **namelist;
	int namelist_count = 0;

	if ((namelist_count
		 = scandir64 (autoindex->filename, &namelist, NULL, &versionsort64))
		< 0)
	{
		response->status = errno == ENOENT ? FH_STATUS_NOT_FOUND
						   : (errno == EACCES || errno == EPERM)
							   ? FH_STATUS_FORBIDDEN
							   : FH_STATUS_INTERNAL_SERVER_ERROR;
		return true;
	}

	struct fh_link *tail = start_link;
	bool is_root = request->uri_len == 1 && request->uri[0] == '/';

	for (int i = 0; i < namelist_count; i++)
	{
		struct dirent64 *entry = namelist[i];
		size_t name_len = strlen (entry->d_name);
		bool is_dir = entry->d_type == DT_DIR;

		if (is_dir && name_len == 1 && entry->d_name[0] == '.')
		{
			goto next_iter;
		}

		struct fh_link *link;

		if (name_len == 2 && entry->d_name[0] == '.' && entry->d_name[1] == '.')
		{
			if (is_root)
				goto next_iter;

			link = fh_pool_alloc (pool, sizeof (*link) + sizeof (*link->buf));

			if (!link)
			{
				response->status = FH_STATUS_INTERNAL_SERVER_ERROR;
				goto ret_clean;
			}

			static const char data[]
				= "7d\r\n<tr>"
				  "<td>"
				  "<img src=\"/icons/folder.png\" alt=\"[DIR]\" />"
				  "</td>"
				  "<td>"
				  "<a href=\"../\">Parent Directory</a>"
				  "</td>"
				  "<td>-</td>"
				  "<td>-</td>"
				  "</tr>\r\n";

			link->buf = (struct fh_buf *) (link + 1);
			link->buf->attrs.mem.data = (uint8_t *) data;
			link->buf->attrs.mem.cap = link->buf->attrs.mem.len
				= sizeof (data) - 1;
			link->buf->attrs.mem.rd_only = true;
			link->is_eos = false;
		}
		else
		{
			struct autoindex_fileinfo afo = { 0 };
			int rc = fh_autoindex_get_file_info (&afo, response,
												 autoindex->filename, entry);

			if (rc == FILEINFO_EXIT)
				goto next_iter;
			else if (rc == FILEINFO_NEXT)
				goto ret_clean;

			const char format[] = "%zx\r\n<tr>"
								  "<td>"
								  "<img src=\"/icons/%s.png\" alt=\"[%s]\" />"
								  "</td>"
								  "<td>"
								  "<a href=\"%s%s\">%s%s</a>"
								  "</td>"
								  "<td>%s</td>"
								  "<td>%s</td>"
								  "</tr>\r\n";

			const size_t format_len = sizeof (format) - 1;
			size_t chunk_len
				= format_len - 19 - 4 + (2 * name_len)
				  + (is_dir ? 6 + 3 + 2 + 1 : (4 + 4 + afo.size_buf_len))
				  + afo.time_len;
			size_t len = chunk_len + 32 + 4;

			link = fh_pool_alloc (pool, sizeof (*link) + sizeof (*link->buf)
											+ len + 1);

			if (!link)
			{
				response->status = FH_STATUS_INTERNAL_SERVER_ERROR;
				goto ret_clean;
			}

			link->buf = (struct fh_buf *) (link + 1);
			link->buf->type = FH_BUF_DATA;
			link->buf->attrs.mem.data = (uint8_t *) (link->buf + 1);
			link->is_eos = false;

			if ((rc = snprintf ((char *) link->buf->attrs.mem.data, len + 1,
								format, chunk_len, is_dir ? "folder" : "file",
								is_dir ? "DIR" : "FILE", entry->d_name,
								is_dir ? "/" : "", entry->d_name,
								is_dir ? "/" : "", is_dir ? "-" : afo.size_buf,
								afo.time_str))
				< 0)
			{
				fh_pr_debug ("Failed to format entry for '%s': %s",
							 afo.full_path, strerror (errno));
				goto next_iter;
			}

			link->buf->attrs.mem.cap = link->buf->attrs.mem.len = (size_t) rc;
		}

		tail->next = link;
		tail = link;

	next_iter:
		free (entry);
		continue;

	ret_clean:
		for (int j = i; j < namelist_count; j++)
			free (namelist[i]);

		free (namelist);
		return true;
	}

	free (namelist);

	tail->next = end_link;
	tail = end_link;

	tail->next = &end_chunk_link;
	tail = &end_chunk_link;

	response->encoding = FH_ENCODING_CHUNKED;
	response->body_start = start_link;
	response->status = FH_STATUS_OK;
	response->use_default_error_response = false;

	fh_pr_debug ("Response generated successfully");
	return true;
}

static bool
fh_autoindex_handle_plain (struct fh_autoindex *autoindex)
{
	const struct fh_request *request = autoindex->request;
	const struct fh_conn *conn = autoindex->conn;
	struct fh_response *response = autoindex->response;
	const char *host = conn->extra->host;
	const size_t host_len = conn->extra->host_len;
	const uint16_t port = conn->extra->port;
	pool_t *pool = autoindex->response->pool;
	const char entry_format[] = "<tr>"
								"<td>"
								"<img src=\"/icons/%s.png\" alt=\"[%s]\" />"
								"</td>"
								"<td>"
								"<a href=\"%s%s\">%s%s</a>"
								"</td>"
								"<td>%s</td>"
								"<td>%s</td>"
								"</tr>";

	size_t index_start_len
		= resource_index_start_html_len - 12 + (3 * request->uri_len);
	size_t index_end_len = resource_index_end_html_len - 6
						   + autoindex->conn->extra->host_len
						   + (port < 10		 ? 1
							  : port < 100	 ? 2
							  : port < 1000	 ? 3
							  : port < 10000 ? 4
											 : 5);
	int rc;

	struct fh_link *end_link;
	struct fh_link *start_link = fh_pool_alloc (
		pool,
		(sizeof (*start_link) + sizeof (*start_link->buf) + index_start_len + 1)
			+ (sizeof (*end_link) + sizeof (*end_link->buf) + index_end_len
			   + 1));

	if (!start_link)
	{
		response->status = FH_STATUS_INTERNAL_SERVER_ERROR;
		return true;
	}

	start_link->buf = (struct fh_buf *) (start_link + 1);
	start_link->buf->attrs.mem.data = (uint8_t *) (start_link->buf + 1);
	
	end_link = (struct fh_link *) (start_link->buf->attrs.mem.data
								   + index_start_len + 1);
	end_link->buf = (struct fh_buf *) (end_link + 1);
	end_link->buf->attrs.mem.data = (uint8_t *) (end_link->buf + 1);

	start_link->buf->attrs.mem.len = start_link->buf->attrs.mem.cap
		= index_start_len;
	end_link->buf->attrs.mem.len = end_link->buf->attrs.mem.cap = index_end_len;

	start_link->is_start = true;
	start_link->is_eos = false;
	end_link->is_start = false;
	end_link->is_eos = true;
	end_link->next = NULL;

	start_link->buf->type = end_link->buf->type = FH_BUF_DATA;

	if ((rc = snprintf ((char *) start_link->buf->attrs.mem.data,
						 index_start_len + 1, resource_index_start_html,
						 (int) request->uri_len, request->uri,
						 (int) request->uri_len, request->uri,
						 (int) request->uri_len, request->uri))
		< 0)
	{
		response->status = FH_STATUS_INTERNAL_SERVER_ERROR;
		return true;
	}

	if ((rc = snprintf ((char *) end_link->buf->attrs.mem.data,
						 index_end_len + 1, resource_index_end_html,
						 (int) autoindex->conn->extra->host_len,
						 autoindex->conn->extra->host, port))
		< 0)
	{
		response->status = FH_STATUS_INTERNAL_SERVER_ERROR;
		return true;
	}

	struct dirent64 **namelist;
	int namelist_count = 0;

	if ((namelist_count
		 = scandir64 (autoindex->filename, &namelist, NULL, &versionsort64))
		< 0)
	{
		response->status = errno == ENOENT ? FH_STATUS_NOT_FOUND
						   : (errno == EACCES || errno == EPERM)
							   ? FH_STATUS_FORBIDDEN
							   : FH_STATUS_INTERNAL_SERVER_ERROR;
		return true;
	}

	bool is_root = request->uri_len == 1 && request->uri[0] == '/';
	struct fh_link *tail = start_link;
	size_t content_length = index_start_len + index_end_len;

	for (int i = 0; i < namelist_count; i++)
	{
		struct dirent64 *entry = namelist[i];
		const size_t name_len = strlen (entry->d_name);
		bool is_dir = entry->d_type == DT_DIR;

		if (is_dir && name_len == 1 && entry->d_name[0] == '.')
			goto plain_iter_end;

		struct fh_link *link;

		if (is_dir && name_len == 2 && entry->d_name[0] == '.'
			&& entry->d_name[1] == '.')
		{
			if (is_root)
				goto plain_iter_end;

			static const char data[]
				= "<tr>"
				  "<td>"
				  "<img src=\"/icons/folder.png\" alt=\"[DIR]\" />"
				  "</td>"
				  "<td>"
				  "<a href=\"../\">Parent Directory</a>"
				  "</td>"
				  "<td>-</td>"
				  "<td>-</td>"
				  "</tr>";

			link = fh_pool_alloc (pool, sizeof (*link) + sizeof (*link->buf));

			if (!link)
			{
				response->status = FH_STATUS_INTERNAL_SERVER_ERROR;
				goto plain_iter_ret;
			}

			link->buf = (struct fh_buf *) (link + 1);
			link->buf->type = FH_BUF_DATA;
			link->buf->attrs.mem.data = (uint8_t *) data;
			link->buf->attrs.mem.len = link->buf->attrs.mem.cap
				= sizeof (data) - 1;
			link->buf->attrs.mem.rd_only = true;

			content_length += sizeof (data) - 1;
		}
		else
		{
			struct autoindex_fileinfo afo = { 0 };
			int rc = fh_autoindex_get_file_info (&afo, response,
												 autoindex->filename, entry);

			if (rc == FILEINFO_EXIT)
				goto plain_iter_end;
			else if (rc == FILEINFO_NEXT)
				goto plain_iter_ret;

			const size_t entry_len
				= sizeof (entry_format) - 1 - 16 + (2 * name_len)
				  + (is_dir ? 6 + 3 + 2 + 1 : (4 + 4 + afo.size_buf_len))
				  + afo.time_len;

			link = fh_pool_alloc (pool, sizeof (*link) + sizeof (*link->buf)
											+ entry_len + 1);

			if (!link)
			{
				response->status = FH_STATUS_INTERNAL_SERVER_ERROR;
				goto plain_iter_ret;
			}

			link->buf = (struct fh_buf *) (link + 1);
			link->buf->attrs.mem.data = (uint8_t *) (link->buf + 1);
			link->buf->attrs.mem.cap = link->buf->attrs.mem.len = entry_len;

			if (snprintf ((char *) link->buf->attrs.mem.data, entry_len + 1,
								entry_format, is_dir ? "folder" : "file",
								is_dir ? "DIR" : "FILE", entry->d_name,
								is_dir ? "/" : "", entry->d_name,
								is_dir ? "/" : "", is_dir ? "-" : afo.size_buf,
								afo.time_str)
				< 0)
			{

				response->status = FH_STATUS_INTERNAL_SERVER_ERROR;
				goto plain_iter_ret;
			}

			content_length += entry_len;
		}

		link->is_eos = false;
		link->is_start = false;

		tail->next = link;
		tail = link;

	plain_iter_end:
		free (entry);
		continue;

	plain_iter_ret:
		for (int j = i; j < namelist_count; j++)
			free (namelist[j]);

		free (namelist);
		return true;
	}

	free (namelist);

	tail->next = end_link;
	tail = end_link;

	response->body_start = start_link;
	response->status = FH_STATUS_OK;
	response->use_default_error_response = false;
	response->content_length = content_length;

	fh_pr_debug ("Response generated successfully");
	return true;
}

bool
fh_autoindex_handle (struct fh_autoindex *autoindex)
{
	switch (autoindex->request->protocol)
	{
		case FH_PROTOCOL_HTTP_1_0:
			return fh_autoindex_handle_plain (autoindex);

		default:
			return fh_autoindex_handle_chunked (autoindex);
	}
}
