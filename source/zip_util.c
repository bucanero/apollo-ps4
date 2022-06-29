#include <zip.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unrar.h>
#include <un7zip.h>

#include "saves.h"
#include "common.h"


void walk_zip_directory(const char* startdir, const char* inputdir, struct zip_t *zipper)
{
	char fullname[256];	
	struct dirent *dirp;
	int len = strlen(startdir) + 1;
	DIR *dp = opendir(inputdir);

	if (!dp) {
		LOG("Failed to open input directory: '%s'", inputdir);
		return;
	}

	if (strlen(inputdir) > len)
	{
		LOG("Adding folder '%s'", inputdir+len);
/*
		if (zip_add_dir(zipper, inputdir+len) < 0)
		{
			LOG("Failed to add directory to zip: %s", inputdir);
			return;
		}
*/
	}

	while ((dirp = readdir(dp)) != NULL) {
		if ((strcmp(dirp->d_name, ".")  != 0) && (strcmp(dirp->d_name, "..") != 0)) {
			snprintf(fullname, sizeof(fullname), "%s%s", inputdir, dirp->d_name);

			if (dirp->d_type == DT_DIR) {
				strcat(fullname, "/");
				walk_zip_directory(startdir, fullname, zipper);
			} else {
				LOG("Adding file '%s'", fullname+len);

				zip_entry_open(zipper, fullname+len);
				if (zip_entry_fwrite(zipper, fullname) != 0) {
					LOG("Failed to add file to zip: %s", fullname);
				}
				zip_entry_close(zipper);
			}
		}
	}
	closedir(dp);
}

int zip_directory(const char* basedir, const char* inputdir, const char* output_filename)
{
    struct zip_t *archive = zip_open(output_filename, ZIP_DEFAULT_COMPRESSION_LEVEL, 'w');

    if (!archive) {
        LOG("Failed to open output file '%s'", output_filename);
        return 0;
    }

    LOG("Zipping <%s> to %s...", inputdir, output_filename);
    walk_zip_directory(basedir, inputdir, archive);
    zip_close(archive);

    return (file_exists(output_filename) == SUCCESS);
}

int on_extract_entry(const char *filename, void *arg)
{
	uint64_t* progress = (uint64_t*) arg;

    LOG("Extracted: %s", filename);
    update_progress_bar(++progress[0], progress[1], filename);

    return 0;
}

int extract_zip(const char* zip_file, const char* dest_path)
{
	int ret;
	uint64_t progress[2];
	struct zip_t *archive = zip_open(zip_file, ZIP_DEFAULT_COMPRESSION_LEVEL, 'r');

	if (!archive)
		return 0;

	progress[0] = 0;
	progress[1] = zip_entries_total(archive);
	zip_close(archive);

	LOG("Extracting ZIP (%lu) to <%s>...", progress[1], dest_path);

	init_progress_bar("Extracting files...");
	ret = zip_extract(zip_file, dest_path, on_extract_entry, progress);
	end_progress_bar();

	return (ret == SUCCESS);
}

void callback_7z(const char* fileName, unsigned long fileSize, unsigned fileNum, unsigned numFiles)
{
    LOG("Extracted: %s (%ld bytes)", fileName, fileSize);
    update_progress_bar(fileNum, numFiles, fileName);
}

int extract_7zip(const char* fpath, const char* dest_path)
{
	int ret;

	LOG("Extracting 7-Zip (%s) to <%s>...", fpath, dest_path);
	init_progress_bar("Extracting files...");

	// Extract 7-Zip archive contents
	ret = Extract7zFileEx(fpath, dest_path, &callback_7z, 0x10000);
	end_progress_bar();

	return (ret == SUCCESS);
}

int extract_rar(const char* rarFilePath, const char* dstPath)
{
	int err = 0;
	HANDLE hArcData; //Archive Handle
	struct RAROpenArchiveDataEx rarOpenArchiveData;
	struct RARHeaderDataEx rarHeaderData;

	memset(&rarOpenArchiveData, 0, sizeof(rarOpenArchiveData));
	memset(&rarHeaderData, 0, sizeof(rarHeaderData));
	rarOpenArchiveData.ArcName = (char*) rarFilePath;
	rarOpenArchiveData.OpenMode = RAR_OM_EXTRACT;

	hArcData = RAROpenArchiveEx(&rarOpenArchiveData);
	if (rarOpenArchiveData.OpenResult != ERAR_SUCCESS)
	{
		LOG("OpenArchive '%s' Failed!", rarOpenArchiveData.ArcName);
		return 0;
	}

	LOG("UnRAR Extract %s to '%s'...", rarFilePath, dstPath);
	init_progress_bar("Extracting files...");

	while (RARReadHeaderEx(hArcData, &rarHeaderData) == ERAR_SUCCESS)
	{
		LOG("Extracting '%s' (%ld bytes)", rarHeaderData.FileName, rarHeaderData.UnpSize + (((uint64_t)rarHeaderData.UnpSizeHigh) << 32));
		update_progress_bar(0, 1, rarHeaderData.FileName);

		if (RARProcessFile(hArcData, RAR_EXTRACT, (char*) dstPath, NULL) != ERAR_SUCCESS)
		{
			err++;
			LOG("ERROR: UnRAR Extract Failed!");
			continue;
		}
		update_progress_bar(1, 1, rarHeaderData.FileName);
	}
	end_progress_bar();

	RARCloseArchive(hArcData);
	return (err == 0);
}

// --- workaround to fix an Open Orbis SDK linking issue with __clock_gettime()
#include <time.h>
int __clock_gettime(clockid_t clock_id, struct timespec *tp)
{
	return clock_gettime(clock_id, tp);
}
// --- to be removed
