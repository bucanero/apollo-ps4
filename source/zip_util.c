#include <zip.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>

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
