#include <iostream>
#include <stdexcept>
#include <string>

#include <Windows.h>

#include "archive.h"
#include "archive_entry.h"

#include "Downloader.hpp"
#include "Extractor.hpp"

int extract(const char* data, int len);
int copy_data(struct archive *ar, struct archive *aw);

void start_usc()
{
	char currDir[MAX_PATH];
	GetCurrentDirectoryA(sizeof(currDir), currDir);
	std::string cd(currDir);
	std::string usc_path = cd + "\\usc-game.exe";

	STARTUPINFOA info = { sizeof(info) };
	PROCESS_INFORMATION processInfo;
	CreateProcessA(NULL, &usc_path.front(),
		NULL,
		NULL,
		FALSE,
		DETACHED_PROCESS,
		NULL,
		NULL,
		&info,
		&processInfo);
	CloseHandle(processInfo.hProcess);
	CloseHandle(processInfo.hThread);
}


int main(int argc, const char* argv[])
{
	std::ios_base::sync_with_stdio(false);
	std::string archiveUrl = "https://www.drewol.me/Downloads/Game.zip";

	if (argc > 1)
	{
		std::cout << "Waiting for the game to close..." << std::endl;
		DWORD uscPid = std::stol(argv[1]);
		HANDLE uscHandle = OpenProcess(SYNCHRONIZE, false, uscPid);
		WaitForSingleObject(uscHandle, INFINITE);
	}

	if (argc > 2)
	{
		archiveUrl = argv[2];
	}

	try
	{
		Downloader downloader;
		downloader.Download(archiveUrl);

		const std::string& content = downloader.GetContent();

		int result = extract(content.c_str(), content.length());
		if (result != 0)
		{
			printf("Failed to update.\n");
		}
		else
		{
			printf("Update complete!\n");
		}
	}
	catch (std::runtime_error err)
	{
		std::cout << "An error has been occured: " << err.what() << std::endl;

		std::cout << "Press ENTER to quit." << std::endl;
		std::cin.get();
		return 1;
	}

	// start_usc();
	Sleep(500);

	return 0;
}

//https://github.com/libarchive/libarchive/wiki/Examples#a-complete-extractor
int extract(const char* data, int len)
{
	struct archive *a;
	struct archive *ext;
	struct archive_entry *entry;
	int flags;
	int r;

	/* Select which attributes we want to restore. */
	flags = ARCHIVE_EXTRACT_TIME;
	flags |= ARCHIVE_EXTRACT_PERM;
	flags |= ARCHIVE_EXTRACT_ACL;
	flags |= ARCHIVE_EXTRACT_FFLAGS;

	a = archive_read_new();
	archive_read_support_format_all(a);
	archive_read_support_compression_all(a);
	ext = archive_write_disk_new();
	archive_write_disk_set_options(ext, flags);
	archive_write_disk_set_standard_lookup(ext);
	if ((r = archive_read_open_memory(a, data, len)))
		exit(1);
	for (;;) {
		r = archive_read_next_header(a, &entry);
		printf("Extracting \"%s\"...\n", archive_entry_pathname(entry));
		if (r == ARCHIVE_EOF)
			break;
		if (r < ARCHIVE_OK)
			fprintf(stderr, "%s\n", archive_error_string(a));
		if (r < ARCHIVE_WARN)
			return 1;
		r = archive_write_header(ext, entry);
		if (r < ARCHIVE_OK)
			fprintf(stderr, "%s\n", archive_error_string(ext));
		else if (archive_entry_size(entry) > 0) {
			r = copy_data(a, ext);
			if (r < ARCHIVE_OK)
				fprintf(stderr, "%s\n", archive_error_string(ext));
			if (r < ARCHIVE_WARN)
				return 1;
		}
		r = archive_write_finish_entry(ext);
		if (r < ARCHIVE_OK)
			fprintf(stderr, "%s\n", archive_error_string(ext));
		if (r < ARCHIVE_WARN)
			return 1;
	}
	archive_read_close(a);
	archive_read_free(a);
	archive_write_close(ext);
	archive_write_free(ext);
	return 0;
}

int copy_data(struct archive *ar, struct archive *aw)
{
	int r;
	const void *buff;
	size_t size;
	la_int64_t offset;

	for (;;) {
		r = archive_read_data_block(ar, &buff, &size, &offset);
		if (r == ARCHIVE_EOF)
			return (ARCHIVE_OK);
		if (r < ARCHIVE_OK)
			return (r);
		r = archive_write_data_block(aw, buff, size, offset);
		if (r < ARCHIVE_OK) {
			fprintf(stderr, "%s\n", archive_error_string(aw));
			return (r);
		}
	}
}
