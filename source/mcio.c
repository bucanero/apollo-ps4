/*
 * PS2VMC Tool - PS2 Virtual Memory Card Tool by Bucanero
 *
 * based on
 * ps3mca-tool - PlayStation 3 Memory Card Adaptor Software
 * Copyright (C) 2011 - jimmikaelkael <jimmikaelkael@wanadoo.fr>
 * Copyright (C) 2011 - "someone who wants to stay anonymous"
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "mcio.h"
#include "util.h"
#include "ps2mc.h"
#include "common.h"

#include <stdio.h>
#include <time.h>

/* Card Flags */
#define CF_USE_ECC              0x01
#define CF_BAD_BLOCK            0x08
#define CF_ERASE_ZEROES         0x10

#define MCIO_CLUSTERSIZE        1024
#define MCIO_CLUSTERFATENTRIES  256

/* Memory Card device types */
#define sceMcTypeNoCard			0
#define sceMcTypePS1			1
#define sceMcTypePS2			2
#define sceMcTypePDA			3

static const uint8_t ECC_Table[256] = {
	0x00, 0x87, 0x96, 0x11, 0xa5, 0x22, 0x33, 0xb4,0xb4, 0x33, 0x22, 0xa5, 0x11, 0x96, 0x87, 0x00,
	0xc3, 0x44, 0x55, 0xd2, 0x66, 0xe1, 0xf0, 0x77,0x77, 0xf0, 0xe1, 0x66, 0xd2, 0x55, 0x44, 0xc3,
	0xd2, 0x55, 0x44, 0xc3, 0x77, 0xf0, 0xe1, 0x66,0x66, 0xe1, 0xf0, 0x77, 0xc3, 0x44, 0x55, 0xd2,
	0x11, 0x96, 0x87, 0x00, 0xb4, 0x33, 0x22, 0xa5,0xa5, 0x22, 0x33, 0xb4, 0x00, 0x87, 0x96, 0x11,
	0xe1, 0x66, 0x77, 0xf0, 0x44, 0xc3, 0xd2, 0x55,0x55, 0xd2, 0xc3, 0x44, 0xf0, 0x77, 0x66, 0xe1,
	0x22, 0xa5, 0xb4, 0x33, 0x87, 0x00, 0x11, 0x96,0x96, 0x11, 0x00, 0x87, 0x33, 0xb4, 0xa5, 0x22,
	0x33, 0xb4, 0xa5, 0x22, 0x96, 0x11, 0x00, 0x87,0x87, 0x00, 0x11, 0x96, 0x22, 0xa5, 0xb4, 0x33,
	0xf0, 0x77, 0x66, 0xe1, 0x55, 0xd2, 0xc3, 0x44,0x44, 0xc3, 0xd2, 0x55, 0xe1, 0x66, 0x77, 0xf0,
	0xf0, 0x77, 0x66, 0xe1, 0x55, 0xd2, 0xc3, 0x44,0x44, 0xc3, 0xd2, 0x55, 0xe1, 0x66, 0x77, 0xf0,
	0x33, 0xb4, 0xa5, 0x22, 0x96, 0x11, 0x00, 0x87,0x87, 0x00, 0x11, 0x96, 0x22, 0xa5, 0xb4, 0x33,
	0x22, 0xa5, 0xb4, 0x33, 0x87, 0x00, 0x11, 0x96,0x96, 0x11, 0x00, 0x87, 0x33, 0xb4, 0xa5, 0x22,
	0xe1, 0x66, 0x77, 0xf0, 0x44, 0xc3, 0xd2, 0x55,0x55, 0xd2, 0xc3, 0x44, 0xf0, 0x77, 0x66, 0xe1,
	0x11, 0x96, 0x87, 0x00, 0xb4, 0x33, 0x22, 0xa5,0xa5, 0x22, 0x33, 0xb4, 0x00, 0x87, 0x96, 0x11,
	0xd2, 0x55, 0x44, 0xc3, 0x77, 0xf0, 0xe1, 0x66,0x66, 0xe1, 0xf0, 0x77, 0xc3, 0x44, 0x55, 0xd2,
	0xc3, 0x44, 0x55, 0xd2, 0x66, 0xe1, 0xf0, 0x77,0x77, 0xf0, 0xe1, 0x66, 0xd2, 0x55, 0x44, 0xc3,
	0x00, 0x87, 0x96, 0x11, 0xa5, 0x22, 0x33, 0xb4,0xb4, 0x33, 0x22, 0xa5, 0x11, 0x96, 0x87, 0x00
};

static const char SUPERBLOCK_MAGIC[]   = "Sony PS2 Memory Card Format ";
static const char SUPERBLOCK_VERSION[] = "1.2.0.0\0\0\0\0";

static char vmcpath[256] = "";
static uint8_t *vmc_data = NULL;
static size_t vmc_size = 0;

struct MCDevInfo {			/* size = 384 */
	uint8_t  magic[28];		/* Superblock magic, on PS2 MC : "Sony PS2 Memory Card Format " */
	uint8_t  version[12];		/* Version number of the format used, 1.2 indicates full support for bad_block_list */
	uint16_t pagesize;		/* size in bytes of a memory card page */
	uint16_t pages_per_cluster;	/* number of pages in a cluster */
	uint16_t blocksize;		/* number of pages in an erase block */
	uint16_t unused;		/* unused */
	uint32_t clusters_per_card;	/* total size in clusters of the memory card */
	uint32_t alloc_offset;		/* Cluster offset of the first allocatable cluster. Cluster values in the FAT and directory entries */
					/* are relative to this. This is the cluster immediately after the FAT */
	uint32_t alloc_end;		/* The cluster after the highest allocatable cluster. Relative to alloc_offset. Not used */
	uint32_t rootdir_cluster;	/* First cluster of the root directory. Relative to alloc_offset. Must be zero */
	uint32_t backup_block1;		/* Erase block used as a backup area during programming. Normally the the last block on the card, */
					/* it may have a different value if that block was found to be bad */
	uint32_t backup_block2;		/* This block should be erased to all ones. Normally the the second last block on the card */
	uint8_t  unused2[8];
	uint32_t ifc_list[32];		/* List of indirect FAT clusters. On a standard 8M card there's only one indirect FAT cluster */
	uint32_t bad_block_list[32];	/* List of erase blocks that have errors and shouldn't be used */
	uint8_t  cardtype;		/* Memory card type. Must be 2, indicating that this is a PS2 memory card */
	uint8_t  cardflags;		/* Physical characteristics of the memory card */
	uint16_t unused3;
	uint32_t cluster_size;
	uint32_t FATentries_per_cluster;
	uint32_t clusters_per_block;
	uint32_t cardform;
	uint32_t rootdir_cluster2;
	uint32_t unknown1;
	uint32_t unknown2;
	uint32_t max_allocatable_clusters;
	uint32_t unknown3;
	uint32_t unknown4;
	uint32_t unknown5;
} __attribute__((packed));

static struct MCDevInfo mcio_devinfo;

struct MCCacheEntry {
	int32_t   cluster;
	uint8_t  *cl_data;
	uint8_t   wr_flag;
	uint8_t   rd_flag;
} __attribute__((packed));

struct MCCacheDir {
	int32_t   cluster;
	int32_t   fsindex;
	int32_t   maxent;
	uint32_t  unused;
} __attribute__((packed));

struct MCFatCluster {
	int32_t entry[MCIO_CLUSTERFATENTRIES];
} __attribute__((packed));

#define MAX_CACHEENTRY		36
static uint8_t mcio_cachebuf[MAX_CACHEENTRY * MCIO_CLUSTERSIZE];
static struct MCCacheEntry mcio_entrycache[MAX_CACHEENTRY];
static struct MCCacheEntry *mcio_mccache[MAX_CACHEENTRY];

static struct MCCacheEntry *pmcio_entrycache;
static struct MCCacheEntry **pmcio_mccache;

struct MCFatCache {
	int32_t entry[(MCIO_CLUSTERFATENTRIES * 2)+1];
} __attribute__((packed));

static struct MCFatCache mcio_fatcache;

#define MAX_CACHEDIRENTRY	3
static struct MCFsEntry mcio_dircache[MAX_CACHEDIRENTRY];


static uint8_t mcio_pagebuf[1056];
static uint8_t *mcio_pagedata[32];
static uint8_t mcio_eccdata[512]; /* size for 32 ecc */

static int32_t mcio_badblock = 0;
static int32_t mcio_replacementcluster[16];


struct MCFHandle { /* size = 48 */
	uint8_t  status;
	uint8_t  wrflag;
	uint8_t  rdflag;
	uint8_t  unknown1;
	uint8_t  drdflag;
	uint8_t  unknown2;
	uint16_t unknown3;
	uint32_t position;
	uint32_t filesize;
	uint32_t freeclink;
	uint32_t clink;
	uint32_t clust_offset;
	uint32_t cluster;
	uint32_t fsindex;
	uint32_t parent_cluster;
	uint32_t parent_fsindex;
};

#define MAX_FDHANDLES	3
struct MCFHandle mcio_fdhandles[MAX_FDHANDLES];

static int Card_FileClose(int fd);


static void long_multiply(uint32_t v1, uint32_t v2, uint32_t *HI, uint32_t *LO)
{
	uint32_t a, b, c, d;
	uint32_t x, y;

	a = (v1 >> 16) & 0xffff;
	b = v1 & 0xffff;
	c = (v2 >> 16) & 0xffff;
	d = v2 & 0xffff;

	*LO = b * d;
	x = a * d + c * b;
	y = ((*LO >> 16) & 0xffff) + x;

	*LO = (*LO & 0xffff) | ((y & 0xffff) << 16);
	*HI = (y >> 16) & 0xffff;

	*HI += a * c;
}

static void Card_DataChecksum(uint8_t *pagebuf, uint8_t *ecc)
{
	uint8_t *p, *p_ecc;
	int32_t i, a2, a3, v, t0;
	const uint8_t *mcio_xortable = ECC_Table;

	p = pagebuf;
	a2 = 0;
	a3 = 0;
	t0 = 0;

	for (i=0; i<128; i++) {
		v = mcio_xortable[*p++];
		a2 ^= v;
		if (v & 0x80) {
			a3 ^= ~i;
			t0 ^= i;
		}
	}

	p_ecc = ecc;
	p_ecc[0] = ~a2 & 0x77;
	p_ecc[1] = ~a3 & 0x7F;
	p_ecc[2] = ~t0 & 0x7F;
}

static int Card_CorrectData(uint8_t *pagebuf, uint8_t *ecc)
{
	int32_t xor0, xor1, xor2, xor3, xor4;
	uint8_t eccbuf[12];

	Card_DataChecksum(pagebuf, eccbuf);

	xor0 = ecc[0] ^ eccbuf[0];
	xor1 = ecc[1] ^ eccbuf[1];
	xor2 = ecc[2] ^ eccbuf[2];

	xor3 = xor1 ^ xor2;
	xor4 = (xor0 & 0xf) ^ (xor0 >> 4);

	if (!xor0 && !xor1 && !xor2)
		return 0;

	if ((xor3 == 0x7f) && (xor4 == 0x7)) {
		ecc[xor2] ^= 1 << (xor0 >> 4);
		return -1;
	}

	xor0 = 0;
	for (xor2=7; xor2>=0; xor2--) {
		if ((xor3 & 1))
			xor0++;
		xor3 = xor3 >> 1;
	}

	for (xor2=3; xor2>=0; xor2--) {
		if ((xor4 & 1))
			xor0++;
		xor4 = xor4 >> 1;
	}

	if (xor0 == 1)
		return -2;

	return -3;
}

static int mcio_chrpos(char *str, char chr)
{
	char *p;

	p = str;
	for (p = str; *p != 0; p++) {
		if (*p == (chr & 0xff))
			break;
	}

	if (*p != (chr & 0xff))
		return -1;

	return p - str;
}

static void mcio_getmcrtime(struct sceMcStDateTime *mc_time)
{
	time_t rawtime;
	struct tm *ptm;

	time(&rawtime);
	ptm = gmtime(&rawtime);

	mc_time->Resv2 = 0;
	mc_time->Sec = ptm->tm_sec;
	mc_time->Min = ptm->tm_min;
	mc_time->Hour = ptm->tm_hour;
	mc_time->Day = ptm->tm_mday;
	mc_time->Month = ptm->tm_mon + 1;
	append_le_uint16((uint8_t *)&mc_time->Year, ptm->tm_year + 1900);
}

static void mcio_copy_dirent(struct io_dirent *dirent, const struct MCFsEntry *fse)
{
	memset((void *)dirent, 0, sizeof(struct io_dirent));
	strncpy(dirent->name, fse->name, 32);
	dirent->name[32] = 0;

	dirent->stat.mode = read_le_uint16((uint8_t *)&fse->mode);
	dirent->stat.attr = read_le_uint32((uint8_t *)&fse->attr);
	dirent->stat.size = read_le_uint32((uint8_t *)&fse->length);

	memcpy(&dirent->stat.ctime, &fse->created, sizeof(struct sceMcStDateTime));
	memcpy(&dirent->stat.mtime, &fse->modified, sizeof(struct sceMcStDateTime));
}

static void mcio_copy_mcentry(struct MCFsEntry *fse, const struct io_dirent *dirent)
{
	append_le_uint16((uint8_t *)&fse->mode, dirent->stat.mode);
	append_le_uint32((uint8_t *)&fse->attr, dirent->stat.attr);

	memcpy(&fse->created, &dirent->stat.ctime, sizeof(struct sceMcStDateTime));
	memcpy(&fse->modified, &dirent->stat.mtime, sizeof(struct sceMcStDateTime));
}

static int Card_GetSpecs(uint16_t *pagesize, uint16_t *blocksize, int32_t *cardsize, uint8_t *flags)
{
	if (memcmp(SUPERBLOCK_MAGIC, vmc_data, 28) != 0)
		return sceMcResFailDetect2;

	struct MCDevInfo *mcdi = (struct MCDevInfo *)vmc_data;

	// check for non-ECC images
	*flags = mcdi->cardflags & ((vmc_size % 0x800000 == 0) ? ~CF_USE_ECC : 0xFF);
	append_le_uint16((uint8_t*)pagesize, read_le_uint16((uint8_t*)&mcdi->pagesize));
	append_le_uint16((uint8_t*)blocksize, read_le_uint16((uint8_t*)&mcdi->blocksize));
	*cardsize = read_le_uint32((uint8_t*)&mcdi->clusters_per_card) * read_le_uint16((uint8_t*)&mcdi->pages_per_cluster);

	return sceMcResSucceed;
}

static int Card_EraseBlock(int32_t block, uint8_t **pagebuf, uint8_t *eccbuf)
{
	int32_t size, ecc_offset, page;
	uint8_t *p_ecc;
	struct MCDevInfo *mcdi = (struct MCDevInfo *)&mcio_devinfo;

	uint16_t blocksize = read_le_uint16((uint8_t *)&mcdi->blocksize);
	uint16_t pagesize = read_le_uint16((uint8_t *)&mcdi->pagesize);
	int ecc = (mcdi->cardflags & CF_USE_ECC) ? 1 : 0;
	int val = (mcdi->cardflags & CF_ERASE_ZEROES) ? 0x00 : 0xFF;
	int sparesize = pagesize >> 5;

	page = block * blocksize;

	for (int i = 0; i < blocksize; i++, page++) {
		memset(&vmc_data[page * (pagesize + ecc*sparesize)], val, pagesize);

		if (mcdi->cardflags & CF_USE_ECC)
		{
			uint8_t* tmp_ecc = calloc(1, sparesize);
			p_ecc = tmp_ecc;
			size = 0;

			while (size < pagesize) {
				Card_DataChecksum(&vmc_data[page * (pagesize + ecc*sparesize)] + size, p_ecc);
				size += 128;
				p_ecc += 3;
			}

			memcpy(&vmc_data[page * (pagesize + ecc*sparesize) + pagesize], tmp_ecc, sparesize);
			free(tmp_ecc);
		}
	}

	if (pagebuf && eccbuf) { /* This part leave the first ecc byte of each block page in eccbuf */
		memset(eccbuf, 0, 32);

		page = 0;

		while (page < blocksize) {
			ecc_offset = page * pagesize;
			if (ecc_offset < 0)
				ecc_offset += 0x1f;
			ecc_offset = ecc_offset >> 5;
			p_ecc = (uint8_t *)(eccbuf + ecc_offset);
			size = 0;
			while (size < pagesize) {
				if (*pagebuf)
					Card_DataChecksum((uint8_t *)(*pagebuf + size), p_ecc);
				size += 128;
				p_ecc += 3;
			}
			pagebuf++;
			page++;
		}
	}

	return sceMcResSucceed;
}

static int Card_WritePageData(int32_t page, uint8_t *pagebuf, uint8_t *eccbuf)
{
	struct MCDevInfo *mcdi = (struct MCDevInfo *)&mcio_devinfo;

	uint16_t pagesize = read_le_uint16((uint8_t *)&mcdi->pagesize);
	int ecc = (mcdi->cardflags & CF_USE_ECC) ? 1 : 0;
	int sparesize = pagesize >> 5;

	memcpy(&vmc_data[page * (pagesize + ecc*sparesize)], pagebuf, pagesize);

	if (mcdi->cardflags & CF_USE_ECC)
		memcpy(&vmc_data[page * (pagesize + ecc*sparesize) + pagesize], eccbuf, sparesize);

	return sceMcResSucceed;
}

static int Card_ReadPageData(int32_t page, uint8_t *pagebuf, uint8_t *eccbuf)
{
	struct MCDevInfo *mcdi = (struct MCDevInfo *)&mcio_devinfo;

	uint16_t pagesize = read_le_uint16((uint8_t *)&mcdi->pagesize);
	int ecc = (mcdi->cardflags & CF_USE_ECC) ? 1 : 0;
	int sparesize = pagesize >> 5;

	memcpy(pagebuf, &vmc_data[page * (pagesize + ecc*sparesize)], pagesize);

	if (mcdi->cardflags & CF_USE_ECC)
		memcpy(eccbuf, &vmc_data[page * (pagesize + ecc*sparesize) + pagesize], sparesize);

	return sceMcResSucceed;
}

static int Card_SetDeviceSpecs(void)
{
	int32_t cardsize;
	uint16_t blocksize, pages_per_cluster;
	struct MCDevInfo *mcdi = &mcio_devinfo;
	void *ps = (void *)&mcdi->pagesize;
	void *bs = (void *)&mcdi->blocksize;

	if (Card_GetSpecs(ps, bs, &cardsize, &mcdi->cardflags) != sceMcResSucceed)
		return sceMcResFullDevice;

	append_le_uint16((uint8_t *)&mcdi->pages_per_cluster, MCIO_CLUSTERSIZE / read_le_uint16((uint8_t *)&mcdi->pagesize));
	append_le_uint32((uint8_t *)&mcdi->cluster_size, (uint32_t)MCIO_CLUSTERSIZE);
	append_le_uint32((uint8_t *)&mcdi->unknown1, 0);
	append_le_uint32((uint8_t *)&mcdi->unknown2, 0);
	append_le_uint16((uint8_t *)&mcdi->unused, UINT16_C(0xff00));
	append_le_uint32((uint8_t *)&mcdi->FATentries_per_cluster, (uint32_t)MCIO_CLUSTERFATENTRIES);
	append_le_uint32((uint8_t *)&mcdi->unknown5, -1);
	append_le_uint32((uint8_t *)&mcdi->rootdir_cluster2, read_le_uint32((uint8_t *)&mcdi->rootdir_cluster));
	blocksize = read_le_uint16((uint8_t *)&mcdi->blocksize);
	pages_per_cluster = read_le_uint16((uint8_t *)&mcdi->pages_per_cluster);
	append_le_uint32((uint8_t *)&mcdi->clusters_per_block,  blocksize / pages_per_cluster);
	append_le_uint32((uint8_t *)&mcdi->clusters_per_card, (cardsize / blocksize) * (blocksize / pages_per_cluster));

	return sceMcResSucceed;
}

static int Card_ReadPage(int32_t page, uint8_t *pagebuf)
{
	int r, index, ecres, retries, count, erase_byte;
	struct MCDevInfo *mcdi = (struct MCDevInfo *)&mcio_devinfo;
	uint8_t eccbuf[32];
	uint8_t *pdata, *peccb;

	uint16_t pagesize = read_le_uint16((uint8_t *)&mcdi->pagesize);
	count = pagesize >> 7;
	erase_byte = (mcdi->cardflags & CF_ERASE_ZEROES) ? 0x00 : 0xFF;

	retries = 0;
	ecres = sceMcResSucceed;
	do {
		if (Card_ReadPageData(page, pagebuf, eccbuf) == sceMcResSucceed) {
			if (mcdi->cardflags & CF_USE_ECC) { /* checking ECC from spare data block */
				/* check for erased page (last byte of spare data set to 0xFF or 0x0) */
				int32_t sparesize = pagesize >> 5;
				if (eccbuf[sparesize - 1] == erase_byte)
					break;

				index = 0;
				peccb = (uint8_t *)eccbuf;
				pdata = (uint8_t *)pagebuf;

				while (index++ < count) {
					r = Card_CorrectData(pdata, peccb);
					if (r < ecres)
						ecres = r;

					peccb += 3;
					pdata += 128;
				} ;

				if (ecres == sceMcResSucceed)
					break;

				if ((retries == 4) && (!(ecres < -2)))
					break;
			}
			else break;
		}
	} while (++retries < 5);

	if (retries < 5)
		return sceMcResSucceed;

	return (ecres != sceMcResSucceed) ? sceMcResNoFormat : sceMcResChangedCard;
}

static void Card_InitCache(void)
{
	int i, j;
	uint8_t *p;

	j = MAX_CACHEENTRY - 1;
	p = (uint8_t *)mcio_cachebuf;

	for (i = 0; i < MAX_CACHEENTRY; i++) {
		mcio_entrycache[i].cl_data = (uint8_t *)p;
		mcio_mccache[i] = (struct MCCacheEntry *)&mcio_entrycache[j - i];
		mcio_entrycache[i].cluster = -1;
		p += MCIO_CLUSTERSIZE;
	}

	pmcio_entrycache = (struct MCCacheEntry *)mcio_entrycache;
	pmcio_mccache = (struct MCCacheEntry **)mcio_mccache;

	append_le_uint32((uint8_t *)&mcio_devinfo.unknown3, -1);
	append_le_uint32((uint8_t *)&mcio_devinfo.unknown4, -1);
	append_le_uint32((uint8_t *)&mcio_devinfo.unknown5, -1);

	memset((void *)&mcio_fatcache, -1, sizeof(mcio_fatcache));

	append_le_uint32((uint8_t *)&mcio_fatcache.entry[0], 0);
}

static int Card_ClearCache(void)
{
	int i, j;
	struct MCCacheEntry **pmce = (struct MCCacheEntry **)pmcio_mccache;
	struct MCCacheEntry *mce, *mce_save;

	for (i = MAX_CACHEENTRY - 1; i >= 0; i--) {
		mce = (struct MCCacheEntry *)pmce[i];
		if (mce->cluster >= 0) {
			mce->wr_flag = 0;
			mce->cluster = -1;
		}
	}

	for (i = 0; i < (MAX_CACHEENTRY - 1); i++) {
		mce = mce_save = (struct MCCacheEntry *)pmce[i];
		if (mce->cluster < 0) {
			for (j = i+1; j < MAX_CACHEENTRY; j++) {
				mce = (struct MCCacheEntry *)pmce[j];
				if (mce->cluster >= 0)
					break;
			}
			if (j == MAX_CACHEENTRY)
				break;

			pmce[i] = (struct MCCacheEntry *)pmce[j];
			pmce[j] = (struct MCCacheEntry *)mce_save;
		}
	}

	memset((void *)&mcio_fatcache, -1, sizeof(mcio_fatcache));

	append_le_uint32((uint8_t *)&mcio_fatcache.entry[0], 0);

	return sceMcResSucceed;
}

static struct MCCacheEntry *Card_GetCacheEntry(int32_t cluster)
{
	int i;
	struct MCCacheEntry *mce = (struct MCCacheEntry *)pmcio_entrycache;

	for (i = 0; i < MAX_CACHEENTRY; i++) {
		if (mce->cluster == cluster)
			return mce;
		mce++;
	}

	return NULL;
}

static void Card_FreeCluster(int32_t cluster) /* release cluster from entrycache */
{
	int i;
	struct MCCacheEntry *mce = (struct MCCacheEntry *)pmcio_entrycache;

	for (i = 0; i < MAX_CACHEENTRY; i++) {
		if (mce->cluster == cluster) {
			mce->cluster = -1;
			mce->wr_flag = 0;
		}
		mce++;
	}
}

static void Card_AddCacheEntry(struct MCCacheEntry *mce)
{
	int i;
	struct MCCacheEntry **pmce = (struct MCCacheEntry **)pmcio_mccache;

	for (i = MAX_CACHEENTRY-1; i >= 0; i--) {
		if (pmce[i] == mce)
			break;
	}

	for ( ; i != 0; i--) {
		pmce[i] = pmce[i-1];
	}

	pmce[0] = (struct MCCacheEntry *)mce;
}

static int Card_ReplaceBackupBlock(int32_t block)
{
	int i;
	struct MCDevInfo *mcdi = (struct MCDevInfo *)&mcio_devinfo;

	if (mcio_badblock > 0)
		return sceMcResFailReplace;

	for (i = 0; i < 16; i++) {
		if ((int32_t)read_le_uint32((uint8_t *)&mcdi->bad_block_list[i]) == -1)
			break;
	}

	uint32_t max_allocatable_clusters = read_le_uint32((uint8_t *)&mcdi->max_allocatable_clusters);

	if (i < 16) {
		uint32_t alloc_offset = read_le_uint32((uint8_t *)&mcdi->alloc_offset);
		uint32_t alloc_end = read_le_uint32((uint8_t *)&mcdi->alloc_end);
		if ((alloc_end - max_allocatable_clusters) < 8)
			return sceMcResFullDevice;

		append_le_uint32((uint8_t *)&mcdi->alloc_end, alloc_end - 8);
		append_le_uint32((uint8_t *)&mcdi->bad_block_list[i], block);
		mcio_badblock = -1;

		uint32_t clusters_per_block = read_le_uint32((uint8_t *)&mcdi->clusters_per_block);

		return (alloc_offset + read_le_uint32((uint8_t *)&mcdi->alloc_end)) / clusters_per_block;
	}

	return sceMcResFullDevice;
}

static int Card_FillBackupBlock1(int32_t block, uint8_t **pagedata, uint8_t *eccdata)
{
	int r, i;
	int32_t sparesize, page_offset;
	struct MCDevInfo *mcdi = (struct MCDevInfo *)&mcio_devinfo;
	uint8_t *p_ecc;

	int16_t pagesize = read_le_uint16((uint8_t *)&mcdi->pagesize);
	sparesize = pagesize >> 5;

	uint32_t backup_block1 = read_le_uint32((uint8_t *)&mcdi->backup_block1);
	uint32_t backup_block2 = read_le_uint32((uint8_t *)&mcdi->backup_block2);
	uint32_t alloc_offset = read_le_uint32((uint8_t *)&mcdi->alloc_offset);
	uint32_t clusters_per_block = read_le_uint32((uint8_t *)&mcdi->clusters_per_block);
	uint16_t blocksize = read_le_uint16((uint8_t *)&mcdi->blocksize);

	if ((mcio_badblock != 0) && (mcio_badblock != block))
		return sceMcResFailReplace;

	if ((int32_t)(alloc_offset / clusters_per_block) == block) /* this refuse to take care of a bad rootdir cluster */
		return sceMcResFailReplace;

	r = Card_EraseBlock(backup_block2, NULL, NULL);
	if (r != sceMcResSucceed)
		return r;

	r = Card_EraseBlock(backup_block1, NULL, NULL);
	if (r != sceMcResSucceed)
		return r;

	for (i = 0; i < 16; i++) {
		if ((int32_t)read_le_uint32((uint8_t *)&mcdi->bad_block_list[i]) == -1)
			break;
	}

	if (i >= 16)
		return sceMcResFailReplace;

	page_offset = backup_block1 * blocksize;
	p_ecc = (uint8_t *)eccdata;

	for (i = 0; i < blocksize; i++) {
		r = Card_WritePageData(page_offset + i, pagedata[i], p_ecc);
		if (r != sceMcResSucceed)
			return r;
		p_ecc += sparesize;
	}

	mcio_badblock = block;

	i = 15;
	do {
		mcio_replacementcluster[i] = 0;
	} while (--i >= 0);

	return sceMcResSucceed;
}

static int Card_FlushCacheEntry(struct MCCacheEntry *mce)
{
	int r, i, j, ecc_count;
	int temp1, temp2, offset, pageindex;
	int32_t clusters_per_block, blocksize, cardtype, pagesize, sparesize, flag, cluster, block;
	struct MCCacheEntry *pmce[16];
	struct MCDevInfo *mcdi = (struct MCDevInfo *)&mcio_devinfo;
	struct MCCacheEntry *mcee;
	uint8_t mcio_backupbuf[16384];
	uint8_t eccbuf[32];
	uint8_t *p_page, *p_ecc;

	if (mce->wr_flag == 0)
		return sceMcResSucceed;

	pagesize = read_le_uint16((uint8_t *)&mcdi->pagesize);
	int16_t pages_per_cluster = (int16_t)read_le_uint16((uint8_t *)&mcdi->pages_per_cluster);
	cardtype = mcdi->cardtype;

	if (cardtype == 0) {
		mce->wr_flag = 0;
		return sceMcResSucceed;
	}

	uint32_t backup_block1 = read_le_uint32((uint8_t *)&mcdi->backup_block1);
	uint32_t backup_block2 = read_le_uint32((uint8_t *)&mcdi->backup_block2);
			
	clusters_per_block = (int32_t)read_le_uint32((uint8_t *)&mcdi->clusters_per_block);
	block = mce->cluster / clusters_per_block;
	blocksize = read_le_uint16((uint8_t *)&mcdi->blocksize);
	sparesize = pagesize >> 5;
	flag = 0;

	memset((void *)pmce, 0, sizeof(pmce));

	i = 0;
	if (MAX_CACHEENTRY > 0) {
		mcee = (struct MCCacheEntry *)pmcio_entrycache;
		do {
			if (mcee->wr_flag == mce->wr_flag) {
				temp1 = mcee->cluster / clusters_per_block;
				temp2 = mcee->cluster % clusters_per_block;

				if (temp1 == block) {
					pmce[temp2] = (struct MCCacheEntry *)mcee;
					if (mcee->rd_flag == 0)
						flag = 1;
				}
			}
			mcee++;
		} while (++i < MAX_CACHEENTRY);
	}

	if (clusters_per_block > 0) {
		i = 0;
		pageindex = 0;
		cluster = block * clusters_per_block;

		do {
			if (pmce[i] != 0) {
				j = 0;
				offset = 0;
				for (j = 0; j < pages_per_cluster; j++) {
					mcio_pagedata[pageindex + j] = (uint8_t *)(pmce[i]->cl_data + offset);
					offset += pagesize;
				}
			}
			else {
				j = 0;
				do {
					offset = (pageindex + j) * pagesize;
					mcio_pagedata[pageindex + j] = (uint8_t *)(mcio_backupbuf + offset);

					r = Card_ReadPage(((cluster + i) * pages_per_cluster) + j, mcio_backupbuf + offset);
					if (r != sceMcResSucceed)
						return -51;
				} while (++j < pages_per_cluster);
			}

			pageindex += pages_per_cluster;
		} while (++i < clusters_per_block);
	}

lbl1:
	if ((flag != 0) && (mcio_badblock <= 0)) {
		r = Card_EraseBlock(backup_block1, (uint8_t **)mcio_pagedata, mcio_eccdata);
		if (r == sceMcResFailReplace) {
lbl2:
			r = Card_ReplaceBackupBlock(backup_block1);
			append_le_uint32((uint8_t *)&mcdi->backup_block1, r);
			backup_block1 = r;
			goto lbl1;
		}
		if (r != sceMcResSucceed)
			return -52;

		append_le_uint32((uint8_t *)&mcio_pagebuf, block | 0x80000000);
		p_page = (uint8_t *)mcio_pagebuf;
		p_ecc = (uint8_t *)eccbuf;

		i = 0;
		do {
			if (pagesize < 0)
				ecc_count = (pagesize + 0x7f) >> 7;
			else
				ecc_count = pagesize >> 7;

			if (i >= ecc_count)
				break;

			Card_DataChecksum(p_page, p_ecc);

			p_ecc += 3;
			p_page += 128;
			i++;
		} while (1);

		r = Card_WritePageData(backup_block2 * blocksize, mcio_pagebuf, eccbuf);
		if (r == sceMcResFailReplace)
			goto lbl3;
		if (r != sceMcResSucceed)
			return -53;

		if (r < blocksize) {
			i = 0;
			p_ecc = (void *)mcio_eccdata;

			do {
				r = Card_WritePageData((backup_block1 * blocksize) + i, mcio_pagedata[i], p_ecc);
				if (r == sceMcResFailReplace)
					goto lbl2;
				if (r != sceMcResSucceed)
					return -54;
				p_ecc += sparesize;
			} while (++i < blocksize);
		}

		r = Card_WritePageData((backup_block2 * blocksize) + 1, mcio_pagebuf, eccbuf);
		if (r == sceMcResFailReplace)
			goto lbl3;
		if (r != sceMcResSucceed)
			return -55;
	}

	r = Card_EraseBlock(block, (uint8_t **)mcio_pagedata, mcio_eccdata);
	if (r == sceMcResFailReplace) {
		r = Card_FillBackupBlock1(block, (uint8_t **)mcio_pagedata, mcio_eccdata);
		for (i = 0; i < clusters_per_block; i++) {
			if (pmce[i] != 0)
				pmce[i]->wr_flag = 0;
		}
		if (r == sceMcResFailReplace)
			return r;
		return -58;
	}
	if (r != sceMcResSucceed)
		return -57;

	if (blocksize > 0) {
		i = 0;
		p_ecc = (uint8_t *)mcio_eccdata;

		do {
			if (pmce[i / pages_per_cluster] == 0) {
				r = Card_WritePageData((block * blocksize) + i, mcio_pagedata[i], p_ecc);
				if (r == sceMcResFailReplace) {
					r = Card_FillBackupBlock1(block, (uint8_t **)mcio_pagedata, mcio_eccdata);
					for (i = 0; i < clusters_per_block; i++) {
						if (pmce[i] != 0)
							pmce[i]->wr_flag = 0;
					}
					if (r == sceMcResFailReplace)
						return r;
					return -58;
				}
				if (r != sceMcResSucceed)
					return -57;
			}
			p_ecc += sparesize;
		} while (++i < blocksize);
	}

	if (blocksize > 0) {
		i = 0;
		p_ecc = (uint8_t *)mcio_eccdata;

		do {
			if (pmce[i / pages_per_cluster] != 0) {
				r = Card_WritePageData((block * blocksize) + i, mcio_pagedata[i], p_ecc);
				if (r == sceMcResFailReplace) {
					r = Card_FillBackupBlock1(block, (uint8_t **)mcio_pagedata, mcio_eccdata);
					for (i = 0; i < clusters_per_block; i++) {
						if (pmce[i] != 0)
							pmce[i]->wr_flag = 0;
					}
					if (r == sceMcResFailReplace)
						return r;
					return -58;
				}
				if (r != sceMcResSucceed)
					return -57;
			}
			p_ecc += sparesize;
		} while (++i < blocksize);
	}

	if (clusters_per_block > 0) {
		i = 0;
		do {
			if (pmce[i] != 0)
				pmce[i]->wr_flag = 0;
		} while (++i < clusters_per_block);
	}

	if ((flag != 0) && (mcio_badblock <= 0)) {
		r = Card_EraseBlock(backup_block2, NULL, NULL);
		if (r == sceMcResFailReplace) {
			goto lbl3;
		}
		if (r != sceMcResSucceed)
			return -58;
	}
	goto lbl_exit;

lbl3:
	r = Card_ReplaceBackupBlock(backup_block2);
	append_le_uint32((uint8_t *)&mcdi->backup_block2, r);
	backup_block2 = r;

	goto lbl1;

lbl_exit:
	return sceMcResSucceed;
}

static int Card_FlushMCCache(void)
{
	int i, r;
	struct MCCacheEntry **pmce = (struct MCCacheEntry **)pmcio_mccache;
	struct MCCacheEntry *mce;

	i = MAX_CACHEENTRY - 1;

	if (i >= 0) {
		do {
			mce = (struct MCCacheEntry *)pmce[i];
			if (mce->wr_flag != 0) {
				r = Card_FlushCacheEntry((struct MCCacheEntry *)mce);
				if (r != sceMcResSucceed)
					return r;
			}
			i--;
		} while (i >= 0);
	}

	return sceMcResSucceed;
}

static int Card_ReadCluster(int32_t cluster, struct MCCacheEntry **pmce)
{
	int r, i;
	int32_t block, block_offset;
	struct MCDevInfo *mcdi = (struct MCDevInfo *)&mcio_devinfo;
	struct MCCacheEntry *mce;

	if (mcio_badblock > 0) {
		uint32_t clusters_per_block = read_le_uint32((uint8_t *)&mcdi->clusters_per_block);
		uint32_t backup_block1 = read_le_uint32((uint8_t *)&mcdi->backup_block1);

		block = cluster / clusters_per_block;
		block_offset = cluster % clusters_per_block;
		
		if (block == mcio_badblock) {
			cluster = (backup_block1 * clusters_per_block) + block_offset;
		}
		else {
			if (mcio_badblock > 0) {
				for (i = 0; i < (int32_t)clusters_per_block; i++) {
					if ((mcio_replacementcluster[i] != 0) && (mcio_replacementcluster[i] == cluster)) {
						block_offset = i % clusters_per_block;
						cluster = (backup_block1 * clusters_per_block) + block_offset;
					}
				}
			}
		}
	}

	mce = Card_GetCacheEntry(cluster);
	if (mce == NULL) {
		mce = pmcio_mccache[MAX_CACHEENTRY - 1];

		if (mce->wr_flag != 0) {
			r = Card_FlushCacheEntry((struct MCCacheEntry *)mce);
			if (r != sceMcResSucceed)
				return r;
		}

		mce->cluster = cluster;
		mce->rd_flag = 0;

		uint16_t pages_per_cluster = read_le_uint16((uint8_t *)&mcdi->pages_per_cluster);
		uint16_t pagesize = read_le_uint16((uint8_t *)&mcdi->pagesize);

		for (i = 0; i < pages_per_cluster; i++) {
			r = Card_ReadPage((cluster * pages_per_cluster) + i, (uint8_t *)(mce->cl_data + (i * pagesize)));
			if (r != sceMcResSucceed)
				return sceMcResFailReadCluster;
		}
	}

	Card_AddCacheEntry(mce);
	*pmce = (struct MCCacheEntry *)mce;

	return sceMcResSucceed;
}

static int Card_WriteCluster(int32_t cluster, int32_t flag)
{
	int r, i, j;
	int32_t page, block;
	uint32_t erase_value;
	struct MCDevInfo *mcdi = (struct MCDevInfo *)&mcio_devinfo;

	uint32_t clusters_per_block = read_le_uint32((uint8_t *)&mcdi->clusters_per_block);
	uint16_t pagesize = read_le_uint16((uint8_t *)&mcdi->pagesize);
	uint16_t blocksize = read_le_uint16((uint8_t *)&mcdi->blocksize);

	block = cluster / clusters_per_block;
	
	for (i = 0; i < 16; i++) { /* check only 16 bad blocks ? */
		if ((int32_t)read_le_uint32((uint8_t *)&mcdi->bad_block_list[i]) < 0)
			break;
		if ((int32_t)read_le_uint32((uint8_t *)&mcdi->bad_block_list[i]) == block)
			return 0;
	}

	if (flag) {
		for (i = 1; i < blocksize; i++)
			mcio_pagedata[i] = NULL;

		mcio_pagedata[0] = mcio_pagebuf;

		page = block * blocksize;
		
		if (mcdi->cardflags & CF_ERASE_ZEROES)
			erase_value = 0xffffffff;
		else
			erase_value = 0x00000000;
			
		memset(mcio_pagebuf, erase_value, pagesize);

		r = Card_EraseBlock(block, (uint8_t **)mcio_pagedata, (uint8_t *)mcio_eccdata);
		if (r == sceMcResFailReplace)
			return 0;
		if (r != sceMcResSucceed)
			return sceMcResChangedCard;

		for (i = 1; i < blocksize; i++) {
			r = Card_WritePageData(page + i, mcio_pagebuf, mcio_eccdata);
			if (r == sceMcResFailReplace)
				return 0;
			if (r != sceMcResSucceed)
				return sceMcResNoFormat;
		}

		for (i = 1; i < blocksize; i++) {
			r = Card_ReadPage(page + i, mcio_pagebuf);
			if (r == sceMcResNoFormat)
				return 0;
			if (r != sceMcResSucceed)
				return sceMcResFullDevice;

			for (j = 0; j < (pagesize >> 2); j++) {
				if (*((uint32_t *)&mcio_pagebuf + j) != erase_value)
					return sceMcResSucceed;
			}
		}

		r = Card_EraseBlock(block, NULL, NULL);
		if (r != sceMcResSucceed)
			return sceMcResChangedCard;

		r = Card_WritePageData(page, mcio_pagebuf, mcio_eccdata);
		if (r == sceMcResFailReplace)
			return 0;
		if (r != sceMcResSucceed)
			return sceMcResNoFormat;

		r = Card_ReadPage(page, mcio_pagebuf);
		if (r == sceMcResNoFormat)
			return 0;
		if (r != sceMcResSucceed)
			return sceMcResFullDevice;

		for (j = 0; j < (pagesize >> 2); j++) {
			if (*((uint32_t *)&mcio_pagebuf + j) != erase_value)
				return 0;
		}

		r = Card_EraseBlock(block, NULL, NULL);
		if (r == sceMcResFailReplace)
			return 0;
		if (r != sceMcResSucceed)
			return sceMcResFullDevice;

		erase_value = ~erase_value;

		for (i = 0; i < blocksize; i++) {
			r = Card_ReadPage(page + i, mcio_pagebuf);
			if (r != sceMcResSucceed)
				return sceMcResDeniedPermit;

			for (j = 0; j < (pagesize >> 2); j++) {
				if (*((uint32_t *)&mcio_pagebuf + j) != erase_value)
					return 0;
			}
		}
	}

	return 1;
}

static int Card_FindFree(int reserve)
{
	int r;
	int32_t rfree, indirect_index, ifc_index, fat_offset, indirect_offset, fat_index, block;
	struct MCDevInfo *mcdi = (struct MCDevInfo *)&mcio_devinfo;
	struct MCCacheEntry *mce1, *mce2;

	int32_t unknown2 = (int32_t)read_le_uint32((uint8_t *)&mcdi->unknown2);
	int32_t FATentries_per_cluster = (int32_t)read_le_uint32((uint8_t *)&mcdi->FATentries_per_cluster);
	rfree = 0;

	int32_t max_allocatable_clusters = (int32_t)read_le_uint32((uint8_t *)&mcdi->max_allocatable_clusters);

	for (fat_index = unknown2; fat_index < max_allocatable_clusters; fat_index++) {

		indirect_index = fat_index / FATentries_per_cluster;
		fat_offset = fat_index % FATentries_per_cluster;

		if ((fat_offset == 0) || (fat_index == unknown2)) {

			ifc_index = indirect_index / FATentries_per_cluster;
			int32_t ifc = (int32_t)read_le_uint32((uint8_t *)&mcdi->ifc_list[ifc_index]);
			r = Card_ReadCluster(ifc, &mce1);
			if (r != sceMcResSucceed)
				return r;

			indirect_offset = indirect_index % FATentries_per_cluster;
			struct MCFatCluster *fc = (struct MCFatCluster *)mce1->cl_data;
			r = Card_ReadCluster(read_le_uint32((uint8_t *)&fc->entry[indirect_offset]), &mce2);
			if (r != sceMcResSucceed)
				return r;
		}

		struct MCFatCluster *fc = (struct MCFatCluster *)mce2->cl_data;

		if ((int32_t)read_le_uint32((uint8_t *)&fc->entry[fat_offset]) >= 0) {
			int32_t alloc_offset = (int32_t)read_le_uint32((uint8_t *)&mcdi->alloc_offset);
			int32_t clusters_per_block = (int32_t)read_le_uint32((uint8_t *)&mcdi->clusters_per_block);
			block = (alloc_offset + fat_offset) / clusters_per_block;
			if (block != mcio_badblock) {
				if (reserve) {
					append_le_uint32((uint8_t *)&fc->entry[fat_offset], 0xffffffff);
					mce2->wr_flag = 1;
					append_le_uint32((uint8_t *)&mcdi->unknown2, fat_index);
					return fat_index;
				}
				rfree++;
			}
		}
	}

	if (reserve)
		return sceMcResFullDevice;

	return (rfree) ? rfree : sceMcResFullDevice;
}

static int Card_SetFatEntry(int32_t fat_index, int32_t fat_entry)
{
	int r;
	int32_t ifc_index, indirect_index, indirect_offset, fat_offset;
	struct MCCacheEntry *mce;
	struct MCDevInfo *mcdi = (struct MCDevInfo *)&mcio_devinfo;

	int32_t FATentries_per_cluster = (int32_t)read_le_uint32((uint8_t *)&mcdi->FATentries_per_cluster);

	indirect_index = fat_index / FATentries_per_cluster;
	fat_offset = fat_index % FATentries_per_cluster;

	ifc_index = indirect_index / FATentries_per_cluster;
	indirect_offset = indirect_index % FATentries_per_cluster;

	int32_t ifc = (int32_t)read_le_uint32((uint8_t *)&mcdi->ifc_list[ifc_index]);

	r = Card_ReadCluster(ifc, &mce);
	if (r != sceMcResSucceed)
		return r;

	struct MCFatCluster *fc = (struct MCFatCluster *)mce->cl_data;

	r = Card_ReadCluster(read_le_uint32((uint8_t *)&fc->entry[indirect_offset]), &mce);
	if (r != sceMcResSucceed)
		return r;

	fc = (struct MCFatCluster *)mce->cl_data;

	append_le_uint32((uint8_t *)&fc->entry[fat_offset], fat_entry);
	mce->wr_flag = 1;

	return sceMcResSucceed;
}

static int Card_GetFatEntry(int32_t fat_index, int32_t *fat_entry)
{
	int r, ifc_index, indirect_index, indirect_offset, fat_offset;
	struct MCCacheEntry *mce;
	struct MCDevInfo *mcdi = (struct MCDevInfo *)&mcio_devinfo;

	int32_t FATentries_per_cluster = (int32_t)read_le_uint32((uint8_t *)&mcdi->FATentries_per_cluster);

	indirect_index = fat_index / FATentries_per_cluster;
	fat_offset = fat_index % FATentries_per_cluster;

	ifc_index = indirect_index / FATentries_per_cluster;
	indirect_offset = indirect_index % FATentries_per_cluster;

	int32_t ifc = (int32_t)read_le_uint32((uint8_t *)&mcdi->ifc_list[ifc_index]);

	r = Card_ReadCluster(ifc, &mce);
	if (r != sceMcResSucceed)
		return r;

	struct MCFatCluster *fc = (struct MCFatCluster *)mce->cl_data;

	r = Card_ReadCluster(read_le_uint32((uint8_t *)&fc->entry[indirect_offset]), &mce);
	if (r != sceMcResSucceed)
		return r;

	fc = (struct MCFatCluster *)mce->cl_data;

	*fat_entry = read_le_uint32((uint8_t *)&fc->entry[fat_offset]);

	return sceMcResSucceed;
}

static int Card_FatRSeek(int fd)
{
	int r;
	int32_t entries_to_read, fat_index;
	struct MCFHandle *fh = (struct MCFHandle *)&mcio_fdhandles[fd];
	struct MCDevInfo *mcdi = (struct MCDevInfo *)&mcio_devinfo;
	int32_t fat_entry;

	int32_t cluster_size = (int32_t)read_le_uint32((uint8_t *)&mcdi->cluster_size);
	entries_to_read = fh->position / cluster_size;

	if (entries_to_read < (int32_t)fh->clust_offset)
		fat_index = fh->freeclink;
	else {
		fat_index = fh->clink;
		entries_to_read -= fh->clust_offset;
	}

	int32_t alloc_offset = (int32_t)read_le_uint32((uint8_t *)&mcdi->alloc_offset);

	if (entries_to_read == 0) {
		if (fat_index >= 0)
			return fat_index + alloc_offset;

		return sceMcResFullDevice;
	}

	do {
		r = Card_GetFatEntry(fat_index, &fat_entry);
		if (r != sceMcResSucceed)
			return r;

		fat_index = fat_entry;

		if (fat_index >= -1)
			return sceMcResFullDevice;

		entries_to_read--;

		fat_index &= 0x7fffffff;
		fh->clink = fat_index;
		fh->clust_offset = (fh->position / cluster_size) - entries_to_read;

	} while (entries_to_read > 0);

	return fat_index + alloc_offset;
}

static int Card_FatWSeek(int fd) /* modify FAT to hold new content for a file */
{
	int r;
	int32_t entries_to_write, fat_index, fat_entry;
	struct MCFHandle *fh = (struct MCFHandle *)&mcio_fdhandles[fd];
	struct MCDevInfo *mcdi = (struct MCDevInfo *)&mcio_devinfo;
	struct MCCacheEntry *mce;

	int32_t cluster_size = (int32_t)read_le_uint32((uint8_t *)&mcdi->cluster_size);
	entries_to_write = fh->position / cluster_size;

	if ((fh->clust_offset == 0) || (entries_to_write < (int32_t)fh->clust_offset)) {
		fat_index = fh->freeclink;

		if (fat_index < 0) {
			fat_index = Card_FindFree(1);

			if (fat_index < 0)
				return sceMcResFullDevice;

			mce = (struct MCCacheEntry *)*pmcio_mccache;
			fh->freeclink = fat_index;

			r = Card_FileClose(fd);
			if (r != sceMcResSucceed)
				return r;

			Card_AddCacheEntry(mce);
			Card_FlushMCCache();
		}
	}
	else {
		fat_index = fh->clink;
		entries_to_write -= fh->clust_offset;
	}

	if (entries_to_write != 0) {
		do {
			r = Card_GetFatEntry(fat_index, &fat_entry);
			if (r != sceMcResSucceed)
				return r;

			if (fat_entry >= (int32_t)0xffffffff) {
				r = Card_FindFree(1);
				if (r < 0)
					return r;
				fat_entry = r;
				fat_entry |= 0x80000000;

				mce = (struct MCCacheEntry *)*pmcio_mccache;

				r = Card_SetFatEntry(fat_index, fat_entry);
				if (r != sceMcResSucceed)
					return r;

				Card_AddCacheEntry(mce);
			}

			entries_to_write--;
			fat_index = fat_entry & 0x7fffffff;
		} while (entries_to_write > 0);
	}

	fh->clink = fat_index;
	fh->clust_offset = fh->position / cluster_size;

	return sceMcResSucceed;
}

static int Card_ReadDirEntry(int32_t cluster, int32_t fsindex, struct MCFsEntry **pfse)
{
	int r, i;
	int32_t maxent, index, clust;
	struct MCDevInfo *mcdi = (struct MCDevInfo *)&mcio_devinfo;
	struct MCFatCache *fci = (struct MCFatCache *)&mcio_fatcache;
	struct MCCacheEntry *mce;

	uint32_t cluster_size = (int32_t)read_le_uint32((uint8_t *)&mcdi->cluster_size);

	maxent = 1026 / (cluster_size >> 9);
	index = fsindex / (cluster_size >> 9);

	clust = cluster;
	i = 0;
	if ((cluster == 0) && (index != 0)) {
		if (index < maxent) {
			i = index;
			if ((read_le_uint32((uint8_t *)&fci->entry[index])) >= 0 )
				clust = read_le_uint32((uint8_t *)&fci->entry[index]);
		}
		i = 0;
		if (index > 0) {
			do {
				if (i >= maxent)
					break;
				if (read_le_uint32((uint8_t *)&fci->entry[i]) < 0)
					break;
				clust = read_le_uint32((uint8_t *)&fci->entry[i]);
			} while (++i < index);
		}
		i--;
	}

	if (i < index) {
		do {
			r = Card_GetFatEntry(clust, &clust);
			if (r != sceMcResSucceed)
				return r;

			if (clust == (int32_t)0xffffffff)
				return sceMcResNoEntry;
			clust &= 0x7fffffff;

			i++;
			if (cluster == 0) {
				if (i < maxent)
					append_le_uint32((uint8_t *)&fci->entry[i], clust);
			}
		} while (i < index);
	}

	uint32_t alloc_offset = read_le_uint32((uint8_t *)&mcdi->alloc_offset);

	r = Card_ReadCluster(alloc_offset + clust, &mce);
	if (r != sceMcResSucceed)
		return r;

	*pfse = (struct MCFsEntry *)(mce->cl_data + ((fsindex % (cluster_size >> 9)) << 9));

	return sceMcResSucceed;
}

static int Card_CreateDirEntry(int32_t parent_cluster, int32_t num_entries, int32_t cluster, struct sceMcStDateTime *ctime)
{
	int r;
	struct MCCacheEntry *mce;
	struct MCDevInfo *mcdi = (struct MCDevInfo *)&mcio_devinfo;
	struct MCFsEntry *mfe, *mfe_next, *pfse;

	uint32_t alloc_offset = read_le_uint32((uint8_t *)&mcdi->alloc_offset);

	r = Card_ReadCluster(alloc_offset + cluster, &mce);
	if (r != sceMcResSucceed)
		return r;

	memset(mce->cl_data, 0, MCIO_CLUSTERSIZE);

	mfe = (struct MCFsEntry *)mce->cl_data;
	mfe_next = (struct MCFsEntry *)(mce->cl_data + sizeof(struct MCFsEntry));

	append_le_uint16((uint8_t *)&mfe->mode, sceMcFileAttrReadable | sceMcFileAttrWriteable | sceMcFileAttrExecutable \
			  | sceMcFileAttrSubdir | sceMcFile0400 | sceMcFileAttrExists); /* 0x8427 */

	if (ctime == NULL)
		mcio_getmcrtime(&mfe->created);
	else
		append_le_uint64((uint8_t *)&mfe->created, read_le_uint64((uint8_t *)ctime));

	append_le_uint64((uint8_t *)&mfe->modified, read_le_uint64((uint8_t *)&mfe->created));

	append_le_uint32((uint8_t *)&mfe->length, 0);
	append_le_uint32((uint8_t *)&mfe->dir_entry, num_entries);
	append_le_uint32((uint8_t *)&mfe->cluster, parent_cluster);
	strcpy(mfe->name, ".");

	if ((parent_cluster == 0) && (num_entries == 0)) {
		/* entry is root directory */
		append_le_uint64((uint8_t *)&mfe_next->created, read_le_uint64((uint8_t *)&mfe->created));
		append_le_uint32((uint8_t *)&mfe->length, 2);
		mfe++;

		append_le_uint16((uint8_t *)&mfe->mode, sceMcFileAttrWriteable | sceMcFileAttrExecutable | sceMcFileAttrSubdir \
			  | sceMcFile0400 | sceMcFileAttrExists | sceMcFileAttrHidden); /* 0xa426 */

		append_le_uint32((uint8_t *)&mfe->dir_entry, 0);
		append_le_uint32((uint8_t *)&mfe->cluster, 0);
	}
	else {
		/* entry is normal "." / ".." */
		Card_ReadDirEntry(parent_cluster, 0, &pfse);

		append_le_uint64((uint8_t *)&mfe_next->created, read_le_uint64((uint8_t *)&pfse->created));
		mfe++;

		append_le_uint16((uint8_t *)&mfe->mode, sceMcFileAttrReadable | sceMcFileAttrWriteable | sceMcFileAttrExecutable \
			  | sceMcFileAttrSubdir | sceMcFile0400 | sceMcFileAttrExists); /* 0x8427 */

		append_le_uint32((uint8_t *)&mfe->dir_entry, read_le_uint32((uint8_t *)&pfse->dir_entry));
		append_le_uint32((uint8_t *)&mfe->cluster, read_le_uint32((uint8_t *)&pfse->cluster));
	}

	append_le_uint64((uint8_t *)&mfe->modified, read_le_uint64((uint8_t *)&mfe->created));
	append_le_uint32((uint8_t *)&mfe->length, 0);
	strcpy(mfe->name, "..");

	mce->wr_flag = 1;

	return sceMcResSucceed;
}

static int Card_GetDirInfo(struct MCFsEntry *pfse, char *filename, struct MCCacheDir *pcd, int32_t unknown_flag)
{
	int i, r;
	int32_t ret, len, pos;
	struct MCFsEntry *fse;

	pos = mcio_chrpos(filename, '/');
	if (pos < 0)
		pos = strlen(filename);

	ret = 0;
	if ((pos == 2) && (!strncmp(filename, "..", 2))) {

		r = Card_ReadDirEntry((int32_t)read_le_uint32((uint8_t *)&pfse->cluster), 0, &fse);
		if (r != sceMcResSucceed)
			return r;

		r = Card_ReadDirEntry(fse->cluster, 0, &fse);
		if (r != sceMcResSucceed)
			return r;

		int32_t cluster = (int32_t)read_le_uint32((uint8_t *)&fse->cluster);
		int32_t dir_entry = (int32_t)read_le_uint32((uint8_t *)&fse->dir_entry);

		if (pcd) {
			pcd->cluster = cluster;
			pcd->fsindex = dir_entry;
		}

		r = Card_ReadDirEntry(cluster, dir_entry, &fse);
		if (r != sceMcResSucceed)
			return r;

		memcpy((void *)pfse, (void *)fse, sizeof(struct MCFsEntry));

		uint16_t mode = read_le_uint16((uint8_t *)&fse->mode);

		if ((mode & sceMcFileAttrHidden) != 0) {
			ret = 1;
		}

		if ((pcd == NULL) || (pcd->maxent < 0))
			return sceMcResSucceed;
	}
	else {
		if ((pos == 1) && (!strncmp(filename, ".", 1))) {
			
			r = Card_ReadDirEntry((int32_t)read_le_uint32((uint8_t *)&pfse->cluster), 0, &fse);
			if (r != sceMcResSucceed)
				return r;

			int32_t cluster = (int32_t)read_le_uint32((uint8_t *)&fse->cluster);
			int32_t dir_entry = (int32_t)read_le_uint32((uint8_t *)&fse->dir_entry);

			if (pcd) {
				pcd->cluster = cluster;
				pcd->fsindex = dir_entry;
			}

			uint16_t mode = read_le_uint16((uint8_t *)&fse->mode);

			ret = 1;
			if ((mode & sceMcFileAttrHidden) != 0) {
				if ((pcd == NULL) || (pcd->maxent < 0))
					return sceMcResSucceed;
			}
			else {
				if ((pcd == NULL) || (pcd->maxent < 0))
					return sceMcResSucceed;
			}
		}
	}

	uint32_t length = read_le_uint32((uint8_t *)&pfse->length);

	if ((pcd) && (pcd->maxent >= 0))
		pcd->maxent = length;

	if (length > 0) {

		i = 0;
		do {
			r = Card_ReadDirEntry((int32_t)read_le_uint32((uint8_t *)&pfse->cluster), i, &fse);
			if (r != sceMcResSucceed)
				return r;

			uint16_t mode = read_le_uint16((uint8_t *)&fse->mode);
			if (((mode & sceMcFileAttrExists) == 0) && (pcd) && (i < pcd->maxent))
				 pcd->maxent = i;

			if (unknown_flag) {
				if ((mode & sceMcFileAttrExists) == 0)
					continue;
			}
			else {
				if ((mode & sceMcFileAttrExists) != 0)
					continue;
			}

			if (ret != 0)
				continue;

			if ((pos >= 11) && (!strncmp(&filename[10], &fse->name[10], pos-10))) {
				len = pos;
				if (strlen(fse->name) >= (uint32_t)pos)
					len = strlen(fse->name);

				if (!strncmp(filename, fse->name, len))
					goto continue_check;
			}

			if (strlen(fse->name) >= (uint32_t)pos)
				len = strlen(fse->name);
			else
				len = pos;

			if (strncmp(filename, fse->name, len))
				continue;

continue_check:
			ret = 1;

			if (pcd == NULL)
				break;

			pcd->fsindex = i;
			pcd->cluster = (int32_t)read_le_uint32((uint8_t *)&pfse->cluster);

			if (pcd->maxent < 0)
				break;

		} while (++i < (int32_t)read_le_uint32((uint8_t *)&pfse->length));
	}

	if (ret == 2)
		return 2;

	return ((ret < 1) ? 1 : 0);
}

static int Card_SetDirEntryState(int32_t cluster, int32_t fsindex, int32_t flags)
{
	int r, i;
	int32_t fat_index, fat_entry;
	struct MCDevInfo *mcdi = (struct MCDevInfo *)&mcio_devinfo;
	struct MCFsEntry *fse;

	r = Card_ReadDirEntry(cluster, fsindex, &fse);
	if (r != sceMcResSucceed)
		return r;

	if (fse->name[0] == '.') {
		if ((fse->name[1] == 0) || (fse->name[1] == '.'))
			return sceMcResNoEntry;
	}

	i = 0;
	do {
		if (mcio_fdhandles[i].status == 0)
			continue;

		if ((int32_t)mcio_fdhandles[i].cluster != cluster)
			continue;

		if ((int32_t)mcio_fdhandles[i].fsindex == fsindex)
			return sceMcResDeniedPermit;

	} while (++i < MAX_FDHANDLES);

	uint16_t mode = read_le_uint16((uint8_t *)&fse->mode);

	if (flags == 0)
		append_le_uint16((uint8_t *)&fse->mode, mode & (sceMcFileAttrExists - 1));
	else
		append_le_uint16((uint8_t *)&fse->mode, mode | sceMcFileAttrExists);

	struct MCCacheEntry *mce = (struct MCCacheEntry *)*pmcio_mccache;
	mce->wr_flag = -1;

	fat_index = read_le_uint32((uint8_t *)&fse->cluster);

	if (fat_index >= 0) {
		if (fat_index < (int32_t)read_le_uint32((uint8_t *)&mcdi->unknown2))
			append_le_uint32((uint8_t *)&mcdi->unknown2, fat_index);
		append_le_uint32((uint8_t *)&mcdi->unknown5, -1);

		do {
			r = Card_GetFatEntry(fat_index, &fat_entry);
			if (r != sceMcResSucceed)
				return r;

			if (flags == 0) {
				fat_entry &= 0x7fffffff;
				if (fat_index < (int32_t)read_le_uint32((uint8_t *)&mcdi->unknown2))
					append_le_uint32((uint8_t *)&mcdi->unknown2, fat_entry);
			}
			else
				fat_entry |= 0x80000000;

			r = Card_SetFatEntry(fat_index, fat_entry);
			if (r != sceMcResSucceed)
				return r;

			fat_index = fat_entry & 0x7fffffff;

		} while (fat_index != 0x7fffffff);
	}

	return sceMcResSucceed;
}

static int Card_CacheDirEntry(const char *filename, struct MCCacheDir *pcacheDir, struct MCFsEntry **pfse, int unknown_flag)
{
	int r;
	int32_t fsindex, cluster, fmode;
	struct MCDevInfo *mcdi = (struct MCDevInfo *)&mcio_devinfo;
	struct MCFsEntry *fse;
	struct MCCacheDir cacheDir;
	char *p;

	if (pcacheDir == NULL) {
		pcacheDir = &cacheDir;
		pcacheDir->maxent = -1;
	}

	p = (char *)filename;
	if (*p == '/') {
		p++;
		cluster = 0;
		fsindex = 0;
	}
	else {
		cluster = read_le_uint32((uint8_t *)&mcdi->rootdir_cluster2);
		fsindex = read_le_uint32((uint8_t *)&mcdi->unknown1);
	}

	r = Card_ReadDirEntry(cluster, fsindex, &fse);
	if (r != sceMcResSucceed)
		return r;

	if (*p == 0) {
		uint16_t mode = read_le_uint16((uint8_t *)&fse->mode);
		if (!(mode & sceMcFileAttrExists))
			return 2;

		if (pcacheDir == NULL) {
			*pfse = (struct MCFsEntry *)fse;
			return sceMcResSucceed;
		}

		memcpy((void *)&mcio_dircache[0], (void *)fse, sizeof(struct MCFsEntry));

		r = Card_GetDirInfo((struct MCFsEntry *)&mcio_dircache[0], ".", pcacheDir, unknown_flag);

		Card_ReadDirEntry(pcacheDir->cluster, pcacheDir->fsindex, pfse);

		if (r > 0)
			return 2;

		return r;

	} else {

		do {
			fmode = sceMcFileAttrReadable | sceMcFileAttrExecutable;
			uint16_t mode = read_le_uint16((uint8_t *)&fse->mode);
			if ((mode & fmode) != fmode)
				return sceMcResDeniedPermit;

			memcpy((void *)&mcio_dircache[0], (void *)fse, sizeof(struct MCFsEntry));

			r = Card_GetDirInfo((struct MCFsEntry *)&mcio_dircache[0], p, pcacheDir, unknown_flag);

			if (r > 0) {
				if (mcio_chrpos(p, '/') >= 0)
					return 2;

				pcacheDir->cluster = cluster;
				pcacheDir->fsindex = fsindex;

				return 1;
			}

			r = mcio_chrpos(p, '/');
			if ((r >= 0) && (p[r + 1] != 0)) {
				p += mcio_chrpos(p, '/') + 1;
				cluster = pcacheDir->cluster;
				fsindex = pcacheDir->fsindex;

				Card_ReadDirEntry(cluster, fsindex, &fse);
			}
			else {
				Card_ReadDirEntry(pcacheDir->cluster, pcacheDir->fsindex, pfse);

				return sceMcResSucceed;
			}
		} while (*p != 0);
	}
	return sceMcResSucceed;
}

static int Card_GetDirEntryCluster(int32_t cluster, int32_t fsindex)
{
	int r, i;
	int32_t maxent, index, clust;
	struct MCDevInfo *mcdi = (struct MCDevInfo *)&mcio_devinfo;
	struct MCFatCache *fci = (struct MCFatCache *)&mcio_fatcache;

	uint32_t cluster_size = (int32_t)read_le_uint32((uint8_t *)&mcdi->cluster_size);

	maxent = 1026 / (cluster_size >> 9);
	index = fsindex / (cluster_size >> 9);

	clust = cluster;
	i = 0;
	if ((cluster == 0) && (index != 0)) {
		if (index < maxent) {
			i = index;
			if ((read_le_uint32((uint8_t *)&fci->entry[index])) >= 0 )
				clust = read_le_uint32((uint8_t *)&fci->entry[index]);
		}
		i = 0;
		if (index > 0) {
			do {
				if (i >= maxent)
					break;
				if (read_le_uint32((uint8_t *)&fci->entry[i]) < 0)
					break;
				clust = read_le_uint32((uint8_t *)&fci->entry[i]);
			} while (++i < index);
		}
		i--;
	}

	if (i < index) {
		do {
			r = Card_GetFatEntry(clust, &clust);
			if (r != sceMcResSucceed)
				return r;

			if (clust == (int32_t)0xffffffff)
				return sceMcResNoEntry;
			clust &= 0x7fffffff;

			i++;
			if (cluster == 0) {
				if (i < maxent)
					append_le_uint32((uint8_t *)&fci->entry[i], clust);
			}
		} while (i < index);
	}

	uint32_t alloc_offset = read_le_uint32((uint8_t *)&mcdi->alloc_offset);

	return clust + alloc_offset;
}

static int Card_CheckBackupBlocks(void)
{
	int r1, r2, r;
	int32_t value1, value2, eccsize;
	struct MCDevInfo *mcdi = (struct MCDevInfo *)&mcio_devinfo;
	struct MCCacheEntry *mce;
	int32_t *pagebuf = (int32_t *)&mcio_pagebuf;

	uint32_t backup_block1 = read_le_uint32((uint8_t *)&mcdi->backup_block1);
	uint32_t backup_block2 = read_le_uint32((uint8_t *)&mcdi->backup_block2);
	uint32_t clusters_per_block = read_le_uint32((uint8_t *)&mcdi->clusters_per_block);
	uint16_t pagesize = read_le_uint16((uint8_t *)&mcdi->pagesize);
	uint16_t blocksize = read_le_uint16((uint8_t *)&mcdi->blocksize);
	uint16_t pages_per_cluster = read_le_uint16((uint8_t *)&mcdi->pages_per_cluster);

	/* First check backup block2 to see if it's in erased state */
	r1 = Card_ReadPage(backup_block2 * blocksize, mcio_pagebuf);

	value1 = *pagebuf;
	if (((mcdi->cardflags & CF_ERASE_ZEROES) != 0) && (value1 == 0))
		value1 = -1;
	if (value1 != -1)
		value1 = value1 & 0x7fffffff;

	r2 = Card_ReadPage((backup_block2 * blocksize) + 1, mcio_pagebuf);

	value2 = *pagebuf;
	if (((mcdi->cardflags & CF_ERASE_ZEROES) != 0) && (value2 == 0))
		value2 = -1;
	if (value2 != -1)
		value2 = value2 & 0x7fffffff;

	if ((value1 != -1) && (value2 == -1))
		goto check_done;
	if ((r1 < 0) || (r2 < 0))
		goto check_done;

	if ((value1 == -1) && (value1 == value2))
		return sceMcResSucceed;

	/* backup block2 is not erased, so programming is assumed to have not been completed
	 * reads content of backup block1
	 */
	for (r1 = 0; r1 < (int32_t) clusters_per_block; r1++) {

		Card_ReadCluster((backup_block1 * clusters_per_block) + r1, &mce);
		mce->rd_flag = 1;

		for (r2 = 0; r2 < pages_per_cluster; r2++) {
			mcio_pagedata[(r1 * ((pages_per_cluster << 16) >> 16)) + r2] = \
				(void *)(mce->cl_data + (r2 * pagesize));
		}
	}

	/* Erase the block where data must be written */
	r = Card_EraseBlock(value1, (uint8_t **)mcio_pagedata, (void *)mcio_eccdata);
	if (r != sceMcResSucceed)
		return r;

	/* Write the block */
	for (r1 = 0; r1 < blocksize; r1++) {
		
		eccsize = pagesize;
		if (eccsize < 0)
			eccsize += 0x1f;
		eccsize = eccsize >> 5;

		r = Card_WritePageData((value1 * ((blocksize << 16) >> 16)) + r1, \
			mcio_pagedata[r1], (uint8_t *)(mcio_eccdata + (eccsize * r1)));

		if (r != sceMcResSucceed)
			return r;
	}

	for (r1 = 0; r1 < (int32_t)clusters_per_block; r1++)
		Card_FreeCluster((backup_block1 * clusters_per_block) + r1);

check_done:
	/* Finally erase backup block2 */
	return Card_EraseBlock(backup_block2, NULL, NULL);
}

static int Card_SetDeviceInfo(void)
{
	int32_t r, allocatable_clusters_per_card, iscluster_valid, current_allocatable_cluster, cluster_cnt;
	struct MCDevInfo *mcdi = &mcio_devinfo;
	struct MCFsEntry *pfse;

	memset((void *)mcdi, 0, sizeof(struct MCDevInfo));

	r = Card_SetDeviceSpecs();
	if (r != sceMcResSucceed)
		return sceMcResFailSetDeviceSpecs;

	r = Card_ReadPage(0, mcio_pagebuf);
	if (r == sceMcResNoFormat)
		return sceMcResNoFormat; /* should rebuild a valid superblock here */
	if (r != sceMcResSucceed)
		return sceMcResFailIO;

	if (strncmp(SUPERBLOCK_MAGIC, (char *)mcio_pagebuf, 28) != 0)
		return sceMcResNoFormat;
	
	if (((mcio_pagebuf[28] - 48) == 1) && ((mcio_pagebuf[30] - 48) == 0)) /* check ver major & minor */
		return sceMcResNoFormat;

	uint8_t *p = (uint8_t *)mcdi;
	for (r=0; r<336; r++)
		p[r] = mcio_pagebuf[r];

	mcdi->cardtype = sceMcTypePS2; /* <-- */

	r = Card_CheckBackupBlocks();
	if (r != sceMcResSucceed)
		return sceMcResFailCheckBackupBlocks;

	r = Card_ReadDirEntry(0, 0, &pfse);
	if (r != sceMcResSucceed)
		return sceMcResNoFormat;

	if (strcmp(pfse->name, ".") != 0)
		return sceMcResNoFormat;

	if (Card_ReadDirEntry(0, 1, &pfse) != sceMcResSucceed)
		return -45;

	if (strcmp(pfse->name, "..") != 0)
		return sceMcResNoFormat;

	append_le_uint32((uint8_t *)&mcdi->cardform, 1);

	uint32_t clusters_per_block, alloc_offset, alloc_end, backup_block2;
	clusters_per_block = read_le_uint32((uint8_t *)&mcdi->clusters_per_block);
	alloc_offset = read_le_uint32((uint8_t *)&mcdi->alloc_offset);
	alloc_end = read_le_uint32((uint8_t *)&mcdi->alloc_end);
	backup_block2 = read_le_uint32((uint8_t *)&mcdi->backup_block2);

	if (((mcio_pagebuf[28] - 48) == 1) && ((mcio_pagebuf[30] - 48) == 1)) { /* check ver major & minor */
		if ((clusters_per_block * backup_block2) == alloc_end)
			append_le_uint32((uint8_t *)&mcdi->alloc_end, (clusters_per_block * backup_block2) - alloc_offset);
	}

	uint32_t hi, lo, temp, clusters_per_card;

	clusters_per_card = read_le_uint32((uint8_t *)&mcdi->clusters_per_card);

	long_multiply(clusters_per_card, 0x10624dd3, &hi, &lo);
	temp = (hi >> 6) - (clusters_per_card >> 31);
	allocatable_clusters_per_card = (((((temp << 5) - temp) << 2) + temp) << 3) + 1;
	iscluster_valid = 0;
	cluster_cnt = 0;
	current_allocatable_cluster = alloc_offset;

	while (cluster_cnt < allocatable_clusters_per_card) {
		if (current_allocatable_cluster >= (int32_t)clusters_per_card)
			break;

		if (((current_allocatable_cluster % clusters_per_block) == 0) \
			|| (alloc_offset == (uint32_t)current_allocatable_cluster)) {
			iscluster_valid = 1;
			for (r=0; r<16; r++) {
				if ((current_allocatable_cluster / clusters_per_block) == read_le_uint32((uint8_t *)&mcdi->bad_block_list[r]))
					iscluster_valid = 0;
			}
		}
		if (iscluster_valid == 1)
			cluster_cnt++;
		current_allocatable_cluster++;
	}

	append_le_uint32((uint8_t *)&mcdi->max_allocatable_clusters, current_allocatable_cluster - alloc_offset);

	return sceMcResSucceed;
}

static int Card_ReportBadBlocks(void)
{
	int r, i;
	int32_t block, bad_blocks, page, erase_byte, err_cnt, err_limit;
	struct MCDevInfo *mcdi = (struct MCDevInfo *)&mcio_devinfo;
	uint8_t *p;

	memset((void *)mcdi->bad_block_list, -1, 128);

	if ((mcdi->cardflags & CF_BAD_BLOCK) == 0)
		return sceMcResSucceed;

	uint16_t pagesize = read_le_uint16((uint8_t *)&mcdi->pagesize);
	uint16_t blocksize = read_le_uint16((uint8_t *)&mcdi->blocksize);
	uint32_t clusters_per_card = read_le_uint32((uint8_t *)&mcdi->clusters_per_card);
	uint32_t clusters_per_block = read_le_uint32((uint8_t *)&mcdi->clusters_per_block);

	err_limit = (((pagesize << 16) >> 16) + ((pagesize << 16) >> 31)) >> 1;

	erase_byte = 0x00;
	if ((mcdi->cardflags & CF_ERASE_ZEROES) != 0)
		erase_byte = 0xff;

	for (block = 0, bad_blocks = 0; (block < (int32_t)(clusters_per_card / clusters_per_block)) && (bad_blocks < 16); block++) {

		err_cnt = 0;
		for (page = 0; page < 2; page++) {
			r = Card_ReadPage((block * blocksize) + page, mcio_pagebuf);
			if (r == sceMcResNoFormat) {
				append_le_uint32((uint8_t *)&mcdi->bad_block_list[bad_blocks], block);
				bad_blocks++;
				break;
			}
			if (r != sceMcResSucceed)
				return r;

			if ((mcdi->cardflags & CF_USE_ECC) == 0) {
				p = (uint8_t *)&mcio_pagebuf;
				for (i = 0; i < pagesize; i++) {
					/* check if the content of page is clean */
					if (*p++ != erase_byte)
						err_cnt++;

					if (err_cnt >= err_limit) {
						append_le_uint32((uint8_t *)&mcdi->bad_block_list[bad_blocks], block);
						bad_blocks++;
						break;
					}
				}
			}
		}
	}

	return sceMcResSucceed;
}

static int Card_Unformat(void)
{
	int r, i, j, z, l;
	int32_t page, blocks_on_card, erase_byte, err_cnt;
	uint32_t erase_value;
	struct MCDevInfo *mcdi = (struct MCDevInfo *)&mcio_devinfo;

	uint16_t pagesize = read_le_uint16((uint8_t *)&mcdi->pagesize);
	uint16_t blocksize = read_le_uint16((uint8_t *)&mcdi->blocksize);
	uint32_t clusters_per_card = read_le_uint32((uint8_t *)&mcdi->clusters_per_card);
	uint32_t clusters_per_block = read_le_uint32((uint8_t *)&mcdi->clusters_per_block);

	blocks_on_card = clusters_per_card / clusters_per_block;

	if (mcdi->cardflags & CF_ERASE_ZEROES)
		erase_value = 0xffffffff;
	else
		erase_value = 0x00000000;

	memset(mcio_pagebuf, erase_value, pagesize);
	memset(mcio_eccdata, erase_value, 128);

	erase_byte = erase_value & 0xff;

	for (i = 1; i < blocks_on_card; i++) {
		page = i * blocksize;
		if (read_le_uint32((uint8_t *)&mcdi->cardform) > 0) {
			j = 0;
			for (j = 0; j < 16; j++) {
				if (read_le_uint32((uint8_t *)&mcdi->bad_block_list[j]) <= 0) {
					j = 16;
					goto lbl1;
				}
				if ((int32_t)read_le_uint32((uint8_t *)&mcdi->bad_block_list[j]) == i)
					goto lbl1;
			}
		}
		else {
			err_cnt = 0;
			j = -1;
			for (z = 0; z < blocksize; z++) {
				r = Card_ReadPage(page + z, mcio_pagebuf);
				if (r == sceMcResNoFormat) {
					j = -2;
					break;
				}
				if (r != sceMcResSucceed)
					return -42;

				if ((mcdi->cardflags & CF_USE_ECC) == 0) {
					for (l = 0; l < pagesize; l++) {
						if (mcio_pagebuf[l] != erase_byte)
							err_cnt++;
						if (err_cnt >= (int32_t)(clusters_per_block << 6)) {
							j = 16;
							break;
						}
					}
					if (j != -1)
						break;
				}
			}
		}

		if (((mcdi->cardflags & CF_USE_ECC) != 0) && (j == -1))
			j = 16;
lbl1:
		if (j == 16) {
			r = Card_EraseBlock(i, NULL, NULL);
			if (r != sceMcResSucceed)
				return -43;
		}
		else {
			memset(mcio_pagebuf, erase_value, pagesize);
			for (z = 0; z < blocksize; z++) {
				r = Card_WritePageData(page + z, mcio_pagebuf, mcio_eccdata);
				if (r != sceMcResSucceed)
					return -44;
			}
		}
	}

	r = Card_EraseBlock(0, NULL, NULL);
	if (r != sceMcResSucceed)
		return -45;

	return sceMcResSucceed;
}

static int Card_Format(void)
{
	int r;
	uint32_t i;
	int32_t size, ifc_index, indirect_offset, allocatable_clusters_per_card;
	int32_t ifc_length, fat_length, fat_entry, alloc_offset;
	int j = 0, z = 0;
	struct MCDevInfo *mcdi = (struct MCDevInfo *)&mcio_devinfo;
	struct MCCacheEntry *mce;

	if ((int32_t)read_le_uint32((uint8_t *)&mcdi->cardform) == sceMcResNoFormat) {
		for (i = 0; i < 32; i++)
			append_le_uint32((uint8_t *)&mcdi->bad_block_list[i], -1);
		append_le_uint32((uint8_t *)&mcdi->rootdir_cluster, 0);
		append_le_uint32((uint8_t *)&mcdi->rootdir_cluster2, 0);
		goto lbl1;
	}

	if ((int32_t)read_le_uint32((uint8_t *)&mcdi->cardform) > 0) {
		if (((mcdi->version[0] - 48) >= 2) || ((mcdi->version[2] - 48) >= 2))
			goto lbl1;
	}

	r = Card_ReportBadBlocks();
	if ((r != sceMcResSucceed) && (r != sceMcResNoFormat))
		return sceMcResChangedCard;

lbl1:
	/* set superblock magic & version */
	memset((void *)&mcdi->magic, 0, sizeof (mcdi->magic) + sizeof (mcdi->version));
	memcpy((uint8_t *)mcdi->magic, SUPERBLOCK_MAGIC, sizeof (mcdi->magic));
	memcpy((uint8_t *)mcdi->version, SUPERBLOCK_VERSION, sizeof (mcdi->version));

	uint16_t blocksize = read_le_uint16((uint8_t *)&mcdi->blocksize);
	uint32_t cluster_size = read_le_uint32((uint8_t *)&mcdi->cluster_size);
	uint32_t clusters_per_card = read_le_uint32((uint8_t *)&mcdi->clusters_per_card);
	uint32_t FATentries_per_cluster = read_le_uint32((uint8_t *)&mcdi->FATentries_per_cluster);

	size = blocksize;
	append_le_uint32((uint8_t *)&mcdi->cardform, -1);

	if (blocksize <= 0)
		size = 8;

	/* clear first 8 clusters */
	for (i = 0; i < (uint32_t)size; i++) {
		r = Card_WriteCluster(i, 1);
		if (r == 0)
			return sceMcResNoFormat;

		if (r != 1)
			return -40;
	}

	fat_length = (((clusters_per_card << 2) - 1) / cluster_size) + 1; /* get length of fat in clusters */
	ifc_length = (((fat_length << 2) - 1) / cluster_size) + 1; /* get number of needed ifc clusters    */

	if (!(ifc_length <= 32)) {
		ifc_length = 32;
		fat_length = FATentries_per_cluster << 5;
	}

	/* clear ifc clusters */
	for(j = 0; j < ifc_length; j++) {
		if (i >= clusters_per_card)
			return sceMcResNoFormat;

		for ( ; i < clusters_per_card; i++) {
			if (Card_WriteCluster(i, 1) != 0)
				break;
		}

		if (i >= clusters_per_card)
			return sceMcResNoFormat;

		append_le_uint32((uint8_t *)&mcdi->ifc_list[j], i);
		i++;
	}

	/* read ifc clusters to mc cache and clear fat clusters */
	if (fat_length > 0) {
		j = 0;

		do {
			ifc_index = j / FATentries_per_cluster;
			indirect_offset = j % FATentries_per_cluster;

			if (indirect_offset == 0) {
				if (Card_ReadCluster(read_le_uint32((uint8_t *)&mcdi->ifc_list[ifc_index]), &mce) != sceMcResSucceed)
					return -42;
				mce->wr_flag = 1;
			}

			if (i >= clusters_per_card)
				return sceMcResNoFormat;

			do {	
				r = Card_WriteCluster(i, 1);
				if (r == 1)
					break;

				if (r < 0)
					return -43;

				i++;
			} while (i < clusters_per_card);

			if (i >= clusters_per_card)
				return sceMcResNoFormat;

			j++;
			struct MCFatCluster *fc = (struct MCFatCluster *)mce->cl_data;
			append_le_uint32((uint8_t *)&fc->entry[indirect_offset], i);
			i++;

		} while (j < fat_length);
	}
	alloc_offset = i;

	append_le_uint32((uint8_t *)&mcdi->backup_block1, 0);
	append_le_uint32((uint8_t *)&mcdi->backup_block2, 0);

	uint32_t clusters_per_block = read_le_uint32((uint8_t *)&mcdi->clusters_per_block);

	/* clear backup blocks */
	for (i = (clusters_per_card / clusters_per_block) - 1; i > 0; i--) {

		r = Card_WriteCluster(clusters_per_block * i, 1);
		if (r < 0)
			return -44;

		if ((r != 0) && (read_le_uint32((uint8_t *)&mcdi->backup_block1) == 0))
			append_le_uint32((uint8_t *)&mcdi->backup_block1, i);
		else if ((r != 0) && (read_le_uint32((uint8_t *)&mcdi->backup_block2) == 0)) {
			append_le_uint32((uint8_t *)&mcdi->backup_block2, i);
			break;
		}
	}

	/* set backup block2 to erased state */
	if (Card_EraseBlock(read_le_uint32((uint8_t *)&mcdi->backup_block2), NULL, NULL) != sceMcResSucceed)
		return -45;

	uint32_t hi, lo, temp;

	long_multiply(clusters_per_card, 0x10624dd3, &hi, &lo);
	temp = (hi >> 6) - (clusters_per_card >> 31);
	allocatable_clusters_per_card = (((((temp << 5) - temp) << 2) + temp) << 3) + 1;
	j = alloc_offset;

	/* checking for bad allocated clusters and building FAT */
	if (j < (int32_t)(i * clusters_per_block)) {
		z = 0;
		do { /* quick check for bad clusters */
			r = Card_WriteCluster(j, 0);
			if (r == 1) {
				if (z == 0) {
					append_le_uint32((uint8_t *)&mcdi->alloc_offset, j);
					append_le_uint32((uint8_t *)&mcdi->rootdir_cluster, 0);
					fat_entry = 0xffffffff; /* marking rootdir end */
				}
				else
					fat_entry = 0x7fffffff; /* marking free cluster */
				z++;
				if (z == allocatable_clusters_per_card)
					append_le_uint32((uint8_t *)&mcdi->max_allocatable_clusters, (j - mcdi->alloc_offset) + 1);
			}
			else {
				if (r != 0)
					return -45;
				fat_entry = 0xfffffffd; /* marking bad cluster */
			}

			if (Card_SetFatEntry(j - read_le_uint32((uint8_t *)&mcdi->alloc_offset), fat_entry) != sceMcResSucceed)
				return -46;

			j++;
		} while (j < (int32_t)(i * clusters_per_block));
	}

	append_le_uint32((uint8_t *)&mcdi->alloc_end, (i * clusters_per_block) - read_le_uint32((uint8_t *)&mcdi->alloc_offset));

	if (read_le_uint32((uint8_t *)&mcdi->max_allocatable_clusters) == 0)
		append_le_uint32((uint8_t *)&mcdi->max_allocatable_clusters, i * clusters_per_block);

	if (z < (int32_t)clusters_per_block)
		return sceMcResNoFormat;

	/* read superblock to mc cache */
	for (i = 0; i < sizeof(struct MCDevInfo); i += MCIO_CLUSTERSIZE) {
		if (i < 0)
			size = i + (MCIO_CLUSTERSIZE - 1);
		else
			size = i;

		if (Card_ReadCluster(size >> 10, &mce) != sceMcResSucceed)
			return -48;

		size = MCIO_CLUSTERSIZE;
		mce->wr_flag = 1;

		if ((sizeof(struct MCDevInfo) - i) <= 1024)
			size = sizeof(struct MCDevInfo) - i;

		memcpy((void *)mce->cl_data, (void *)(mcdi + i), size);
	}

	append_le_uint32((uint8_t *)&mcdi->unknown1, 0);
	append_le_uint32((uint8_t *)&mcdi->unknown2, 0);
	append_le_uint32((uint8_t *)&mcdi->unknown5, -1);
	append_le_uint32((uint8_t *)&mcdi->rootdir_cluster2, mcdi->rootdir_cluster);

	/* Create root dir */
	if (Card_CreateDirEntry(0, 0, 0, NULL) != sceMcResSucceed)
		return -49;

	/* finally flush cache to memcard */
	r = Card_FlushMCCache();
	if (r != sceMcResSucceed)
		return r;

	append_le_uint32((uint8_t *)&mcdi->cardform, 1);

	return sceMcResSucceed;
}

static int Card_Delete(const char *filename, int flags)
{
	int r, i;
	struct MCCacheDir cacheDir;
	struct MCFsEntry *fse1, *fse2;

	r = Card_CacheDirEntry(filename, &cacheDir, &fse1, ((uint32_t)(flags < 1)) ? 1 : 0);
	if (r > 0)
		return sceMcResNoEntry;
	if (r < 0)
		return r;

	uint16_t mode = read_le_uint16((uint8_t *)&fse1->mode);
	if (!flags) {
		if (!(mode & sceMcFileAttrExists))
			return sceMcResNoEntry;
	}
	else {
		if (mode & sceMcFileAttrExists)
			return sceMcResNoEntry;
	}

	int32_t cluster = (int32_t)read_le_uint32((uint8_t *)&fse1->cluster);
	int32_t dir_entry = (int32_t)read_le_uint32((uint8_t *)&fse1->dir_entry);
	int32_t length = (int32_t)read_le_uint32((uint8_t *)&fse1->length);

	if (!cluster && !dir_entry)
		return sceMcResNoEntry;

	i = 2;
	if ((!flags) && (mode & sceMcFileAttrSubdir) && (i < length)) {
		do {
			r = Card_ReadDirEntry(cluster, i, &fse2);
			if (r != sceMcResSucceed)
				return r;

			if (read_le_uint16((uint8_t *)&fse2->mode) & sceMcFileAttrExists)
				return sceMcResNotEmpty;

		} while (++i < length);
	}

	r = Card_SetDirEntryState(cacheDir.cluster, cacheDir.fsindex, flags);
	if (r != sceMcResSucceed)
		return r;

	r = Card_FlushMCCache();
	if (r != sceMcResSucceed)
		return r;

	return sceMcResSucceed;
}

static int Card_Probe(void)
{
	int r;
	struct MCDevInfo *mcdi;

	r = Card_SetDeviceInfo();
	if (r == sceMcResSucceed)
		return sceMcResSucceed;

	if (r != sceMcResNoFormat)
		return sceMcResFailDetect2;

	mcdi = &mcio_devinfo;
	append_le_uint32((uint8_t *)&mcdi->cardform, r);

	return r;
}

static void Card_InvFileHandles(void)
{
	int i;
	struct MCFHandle *fh = (struct MCFHandle *)&mcio_fdhandles[0];

	for (i=0; i<MAX_FDHANDLES; i++) {
		fh->status = 0;
		fh++;
	}
}

static int Card_FileOpen(const char *filename, int flags)
{
	int i, r;
	int32_t fd, fsindex, fsoffset, fat_index, rdflag, wrflag, pos, mcfree;
	struct MCFHandle *fh, *fh2;
	struct MCCacheDir cacheDir;
	struct MCFsEntry *fse1, *fse2;
	struct MCCacheEntry *mce;
	char *p;
	int32_t fat_entry;
	struct MCDevInfo *mcdi = (struct MCDevInfo *)&mcio_devinfo;

	if ((flags & sceMcFileCreateFile) != 0)
		flags |= sceMcFileAttrWriteable;

	if (filename[0] == 0)
		return sceMcResNoEntry;

	fd = 0;
	do {
		fh = (struct MCFHandle *)&mcio_fdhandles[fd];
		if (fh->status == 0)
			break;
	} while (++fd < MAX_FDHANDLES);

	if (fd == MAX_FDHANDLES)
		return sceMcResUpLimitHandle;

	fh = (struct MCFHandle *)&mcio_fdhandles[fd];

	memset((void *)fh, 0, sizeof(struct MCFHandle));

	if ((flags & (sceMcFileCreateFile | sceMcFileCreateDir)) == 0)
		cacheDir.maxent = -1;
	else
		cacheDir.maxent = 0;

	fse1 = NULL;
	r = Card_CacheDirEntry(filename, &cacheDir, &fse1, 1);
	if (r < 0)
		return r;

	if (fse1) {
		memcpy((void *)&mcio_dircache[1], (void *)fse1, sizeof(struct MCFsEntry));

		uint16_t mode = read_le_uint16((uint8_t *)&fse1->mode);
		if ((flags == 0) && ((mode & sceMcFileAttrExists) == 0))
			r = 1;
	}

	if (r == 2)
		return sceMcResNoEntry;

	if (r == 3)
		return sceMcResDeniedPermit;

	if ((r == 0) && ((flags & sceMcFileCreateDir) != 0))
		return sceMcResNoEntry;

	if ((r == 1) && ((flags & (sceMcFileCreateFile | sceMcFileCreateDir)) == 0))
		return sceMcResNoEntry;

	rdflag = flags & sceMcFileAttrReadable;
	wrflag = flags & sceMcFileAttrWriteable;
	fh->freeclink = -1;
	fh->clink = -1;
	fh->clust_offset = 0;
	fh->filesize = 0;
	fh->position = 0;
	fh->unknown2 = 0;
	fh->rdflag = rdflag;
	fh->wrflag = wrflag;
	fh->unknown1 = 0;
	fh->cluster = cacheDir.cluster;
	fh->fsindex = cacheDir.fsindex;
	
	if (r == 0) {
		uint16_t dircache_mode = read_le_uint16((uint8_t *)&mcio_dircache[1].mode);

		if ((wrflag != 0) && ((dircache_mode & sceMcFileAttrWriteable) == 0))
			return sceMcResDeniedPermit;

		r = Card_ReadDirEntry(cacheDir.cluster, 0, &fse2);
		if (r != sceMcResSucceed)
			return r;

		fh->parent_cluster = read_le_uint32((uint8_t *)&fse2->cluster);
		fh->parent_fsindex = read_le_uint32((uint8_t *)&fse2->dir_entry);

		if ((dircache_mode & sceMcFileAttrSubdir) != 0) {
			if ((dircache_mode & sceMcFileAttrReadable) == 0)
				return sceMcResDeniedPermit;

			if ((flags & sceMcFileAttrSubdir) == 0)
				return sceMcResNotFile;

			fh->freeclink = read_le_uint32((uint8_t *)&mcio_dircache[1].cluster);
			fh->rdflag = 0;
			fh->wrflag = 0;
			fh->unknown1 = 0;
			fh->drdflag = 1;
			fh->status = 1;
			fh->filesize = read_le_uint32((uint8_t *)&mcio_dircache[1].length);
			fh->clink = fh->freeclink;

			return fd;
		}

		if ((flags & sceMcFileAttrWriteable) != 0) {
			i = 0;
			do {
				fh2 = (struct MCFHandle *)&mcio_fdhandles[i];

				if ((fh2->status == 0) \
					|| (fh2->cluster != (uint32_t) cacheDir.cluster) || (fh2->fsindex != (uint32_t) cacheDir.fsindex))
					continue;

				if (fh2->wrflag != 0)
					return sceMcResDeniedPermit;

			} while (++i < MAX_FDHANDLES);
		}

		if ((flags & sceMcFileCreateFile) != 0) {
			r = Card_SetDirEntryState(cacheDir.cluster, cacheDir.fsindex, 0);
			Card_FlushMCCache();

			if (r != sceMcResSucceed)
				return r;

			if (cacheDir.fsindex < cacheDir.maxent)
				cacheDir.maxent = cacheDir.fsindex;
		}
		else {
			fh->freeclink = read_le_uint32((uint8_t *)&mcio_dircache[1].cluster);
			fh->filesize = read_le_uint32((uint8_t *)&mcio_dircache[1].length);
			fh->clink = fh->freeclink;

			if (fh->rdflag != 0)
				fh->rdflag = (*((uint8_t *)&mcio_dircache[1].mode)) & sceMcFileAttrReadable;
			else
				fh->rdflag = 0;

			if (fh->wrflag != 0)
				fh->wrflag = (dircache_mode >> 1) & sceMcFileAttrReadable;
			else
				fh->wrflag = 0;

			fh->status = 1;

			return fd;
		}
	}
	else {
		fh->parent_cluster = cacheDir.cluster;
		fh->parent_fsindex = cacheDir.fsindex;
	}

	r = Card_ReadDirEntry(fh->parent_cluster, fh->parent_fsindex, &fse1);
	if (r != sceMcResSucceed)
		return r;

	memcpy((void *)&mcio_dircache[2], (void *)fse1, sizeof(struct MCFsEntry));
	uint32_t dircache_length = read_le_uint32((uint8_t *)&mcio_dircache[2].length);
	uint32_t dircache_cluster = read_le_uint32((uint8_t *)&mcio_dircache[2].cluster);

	i = -1;
	if (dircache_length == (uint32_t) cacheDir.maxent) {

		int32_t cluster_size = (int32_t)read_le_uint32((uint8_t *)&mcdi->cluster_size);

		fsindex = dircache_length / (cluster_size >> 9);
		fsoffset = dircache_length % (cluster_size >> 9);

		if (fsoffset == 0) {
			fat_index = dircache_cluster;
			i = fsindex;

			if ((dircache_cluster == 0) && (i >= 2)) {
				if (read_le_uint32((uint8_t *)&mcio_fatcache.entry[i-1]) >= 0) {
					fat_index = read_le_uint32((uint8_t *)&mcio_fatcache.entry[i-1]);
					i = 1;
				}
			}
			i--;

			if (i != -1) {

				do {
					r = Card_GetFatEntry(fat_index, &fat_entry);
					if (r != sceMcResSucceed)
						return r;

					if (fat_entry >= -1) {
						r = Card_FindFree(1);
						if (r < 0)
							return r;

						fat_entry = r;
						mce = *pmcio_mccache;

						fat_entry |= 0x80000000;

						r = Card_SetFatEntry(fat_index, fat_entry);
						if (r != sceMcResSucceed)
							return r;

						Card_AddCacheEntry(mce);
					}
					i--;
					fat_index = fat_entry & 0x7fffffff;

				} while (i != -1);
			}
		}

		r = Card_FlushMCCache();
		if (r != sceMcResSucceed)
			return r;

		i = -1;

		dircache_length++;
		append_le_uint32((uint8_t *)&mcio_dircache[2].length, dircache_length);
	}

	do {
		p = (char *)(filename + i + 1);
		pos = i + 1;
		r = mcio_chrpos(p, '/');
		if (r < 0)
			break;
		i = pos + r;
	} while (1);

	p = (char *)(filename + pos);

	mcfree = 0;

	if ((flags & sceMcFileCreateDir) != 0) {
		r = Card_FindFree(1);
		if (r < 0)
			return r;
		mcfree = r;
	}

	mce = *pmcio_mccache;

	mcio_getmcrtime(&mcio_dircache[2].modified);

	r = Card_ReadDirEntry(dircache_cluster, cacheDir.maxent, &fse2);
	if (r != sceMcResSucceed)
		return r;

	memset((void *)fse2, 0, sizeof(struct MCFsEntry));

	strncpy((void *)fse2->name, p, 32);

	uint64_t modified = read_le_uint64((uint8_t *)&mcio_dircache[2].modified);
	append_le_uint64((uint8_t *)&fse2->created, modified);
	append_le_uint64((uint8_t *)&fse2->modified, modified);

	struct MCCacheEntry *mce_1st = (struct MCCacheEntry *)*pmcio_mccache;
	mce_1st->wr_flag = -1;

	Card_AddCacheEntry(mce);
	
	if ((flags & sceMcFileCreateDir) != 0) {

		uint16_t fmode = ((flags & sceMcFileAttrHidden) | sceMcFileAttrReadable | sceMcFileAttrWriteable \
				| sceMcFileAttrExecutable | sceMcFileAttrSubdir | sceMcFile0400 | sceMcFileAttrExists) /* 0x8427 */
					| (flags & (sceMcFileAttrPS1 | sceMcFileAttrPDAExec));

		append_le_uint16((uint8_t *)&fse2->mode, fmode);
		append_le_uint32((uint8_t *)&fse2->cluster, mcfree);
		append_le_uint32((uint8_t *)&fse2->length, 2);

		r = Card_CreateDirEntry(dircache_cluster, cacheDir.maxent, mcfree, (struct sceMcStDateTime *)&fse2->created);
		if (r != sceMcResSucceed)
			return -46;

		r = Card_ReadDirEntry(fh->parent_cluster, fh->parent_fsindex, &fse1);
		if (r != sceMcResSucceed)
			return r;

		memcpy((void *)fse1, (void *)&mcio_dircache[2], sizeof(struct MCFsEntry));

		mce_1st = (struct MCCacheEntry *)*pmcio_mccache;
		mce_1st->wr_flag = -1;

		r = Card_FlushMCCache();
		if (r != sceMcResSucceed)
			return r;

		return sceMcResSucceed;
	}
	else {
		uint16_t fmode = ((flags & sceMcFileAttrHidden) | sceMcFileAttrReadable | sceMcFileAttrWriteable \
				| sceMcFileAttrExecutable | sceMcFileAttrFile | sceMcFile0400 | sceMcFileAttrExists) /* 0x8417 */
					| (flags & (sceMcFileAttrPS1 | sceMcFileAttrPDAExec));

		append_le_uint16((uint8_t *)&fse2->mode, fmode);
		append_le_uint32((uint8_t *)&fse2->cluster, -1);
		fh->cluster = dircache_cluster;
		fh->status = 1;
		fh->fsindex = cacheDir.maxent;

		r = Card_ReadDirEntry(fh->parent_cluster, fh->parent_fsindex, &fse1);
		if (r != sceMcResSucceed)
			return r;

		memcpy((void *)fse1, (void *)&mcio_dircache[2], sizeof(struct MCFsEntry));

		mce_1st = (struct MCCacheEntry *)*pmcio_mccache;
		mce_1st->wr_flag = -1;

		r = Card_FlushMCCache();
		if (r != sceMcResSucceed)
			return r;
	}

	return fd;
}

static int Card_FileClose(int fd)
{
	int r;
	uint16_t fmode;
	struct MCFHandle *fh = (struct MCFHandle *)&mcio_fdhandles[fd];
	struct MCFsEntry *fse1, *fse2;
	struct sceMcStDateTime mcio_fsmodtime;

	r = Card_ReadDirEntry(fh->cluster, fh->fsindex, &fse1);
	if (r != sceMcResSucceed)
		return r;

	uint16_t mode = read_le_uint16((uint8_t *)&fse1->mode);
	if (fh->unknown2 == 0)
		fmode = mode | sceMcFileAttrClosed;
	else
		fmode = mode & 0xff7f;
	append_le_uint16((uint8_t *)&fse1->mode, fmode);

	mcio_getmcrtime(&fse1->modified);

	append_le_uint32((uint8_t *)&fse1->cluster, fh->freeclink);
	append_le_uint32((uint8_t *)&fse1->length, fh->filesize);

	struct MCCacheEntry *mce = (struct MCCacheEntry *)*pmcio_mccache;
	mce->wr_flag = -1;

	append_le_uint64((uint8_t *)&mcio_fsmodtime, read_le_uint64((uint8_t *)&fse1->modified));

	r = Card_ReadDirEntry(fh->parent_cluster, fh->parent_fsindex, &fse2);
	if (r != sceMcResSucceed)
		return r;

	append_le_uint64((uint8_t *)&fse2->modified, read_le_uint64((uint8_t *)&mcio_fsmodtime));

	mce = (struct MCCacheEntry *)*pmcio_mccache;
	mce->wr_flag = -1;

	return sceMcResSucceed;
}

static int Card_FileRead(int fd, void *buffer, int nbyte)
{
	int r;
	int32_t temp, rpos, size, offset;
	struct MCFHandle *fh = (struct MCFHandle *)&mcio_fdhandles[fd];
	struct MCDevInfo *mcdi = (struct MCDevInfo *)&mcio_devinfo;
	struct MCCacheEntry *mce;

	if (fh->position < fh->filesize) {

		temp = fh->filesize - fh->position;
		if (nbyte > temp)
			nbyte = temp;

		rpos = 0;
		if (nbyte > 0) {

			do {
				int32_t cluster_size = (int32_t)read_le_uint32((uint8_t *)&mcdi->cluster_size);
				offset = fh->position % cluster_size;  /* file pointer offset % cluster size */
				temp = cluster_size - offset;
				if (temp < nbyte)
					size = temp;
				else
					size = nbyte;

				r = Card_FatRSeek(fd);

				if (r <= 0)
					return r;

				r = Card_ReadCluster(r, &mce);
				if (r != sceMcResSucceed)
					return r;

				uint8_t *p = (uint8_t *)buffer;
				memcpy(&p[rpos], &mce->cl_data[offset], size);

				rpos += size;
				mce->rd_flag = 1;
				nbyte -= size;
				fh->position += size;

			} while (nbyte);
		}
		return rpos;
	}

	return 0;
}

static int Card_FileWrite(int fd, void *buffer, int nbyte)
{
	int r, r2;
	int32_t wpos, size, offset;
	struct MCFHandle *fh = (struct MCFHandle *)&mcio_fdhandles[fd];
	struct MCDevInfo *mcdi = (struct MCDevInfo *)&mcio_devinfo;
	struct MCCacheEntry *mce;

	if (nbyte) {
		if (fh->unknown2 == 0) {
			fh->unknown2 = 1;

			r = Card_FileClose(fd);
			if (r != sceMcResSucceed)
				return r;
			r = Card_FlushMCCache();
			if (r != sceMcResSucceed)
				return r;
		}
	}

	wpos = 0;
	if (nbyte) {
		do {
			r = Card_FatRSeek(fd);
			if (r == sceMcResFullDevice) {

				r2 = Card_FatWSeek(fd);
				if (r2 == r)
					return sceMcResFullDevice;

				if (r2 != sceMcResSucceed)
					return r2;

				r = Card_FatRSeek(fd);
			}
			else {
				if (r < 0)
					return r;
			}

			r = Card_ReadCluster(r, &mce);
			if (r != sceMcResSucceed)
				return r;

			mce->rd_flag = 1;

			uint32_t cluster_size = read_le_uint32((uint8_t *)&mcdi->cluster_size);

			offset = fh->position % cluster_size; /* file pointer offset % cluster size */
			r2 = cluster_size - offset;
			if (r2 < nbyte)
				size = r2;
			else
				size = nbyte;

			memcpy((void *)(mce->cl_data + offset), (void *)((uint8_t *)buffer + wpos), size);

			mce->wr_flag = 1;

			r = fh->position + size;
			fh->position += size;

			if (r < (int32_t)fh->filesize)
				r = fh->filesize;

			fh->filesize = r;

			nbyte -= size;
			wpos += size;

		} while (nbyte);
	}

	r = Card_FileClose(fd);
	if (r != sceMcResSucceed)
		return r;

	return wpos;
}


int mcio_vmcInit(const char* vmc)
{
	int r;

	mcio_vmcFinish();

	if (read_buffer(vmc, &vmc_data, &vmc_size) < 0)
		return sceMcResFailIO;

	strncpy(vmcpath, vmc, sizeof(vmcpath));
	Card_InitCache();

	r = mcio_mcDetect();
	if (r == sceMcResChangedCard)
		return sceMcResSucceed;

	return r;
}

void mcio_vmcFinish(void)
{
	if (vmc_data)
	{
		write_buffer(vmcpath, vmc_data, vmc_size);
		free(vmc_data);
	}

	vmcpath[0] = 0;
	vmc_data = NULL;
}

int mcio_mcDetect(void)
{
	int r=0;
	struct MCDevInfo *mcdi = (struct MCDevInfo *)&mcio_devinfo;

	if ((mcdi->cardtype == sceMcTypeNoCard) || (mcdi->cardtype == sceMcTypePS2)) {
		r = Card_Probe();
		if (!(r < -9)) {
			mcdi->cardtype = sceMcTypePS2;
			return r;
		}
	}

	mcdi->cardtype = 0;
	append_le_uint32((uint8_t *)&mcdi->cardform, 0);
	Card_InvFileHandles();
	Card_ClearCache();

	return r;
}

int mcio_mcOpen(const char *filename, int flag)
{
	int r;

	r = mcio_mcDetect();
	if (r != sceMcResSucceed)
		return r;

	struct MCDevInfo *mcdi = (struct MCDevInfo *)&mcio_devinfo;
	if ((int32_t)read_le_uint32((uint8_t *)&mcdi->cardform) == sceMcResNoFormat)
		return sceMcResNoFormat;

	r = Card_FileOpen(filename, flag);
	if (r < -9) {
		Card_InvFileHandles();
		Card_ClearCache();
	}

	return r;
}

int mcio_mcClose(int fd)
{
	int r;
	struct MCFHandle *fh;

	if (!(fd < MAX_FDHANDLES))
		return sceMcResDeniedPermit;

	fh = (struct MCFHandle *)&mcio_fdhandles[fd];
	if (!fh->status)
		return sceMcResDeniedPermit;

	fh->status = 0;
	r = mcio_mcDetect();
	if (r != sceMcResSucceed)
		return r;

	r = Card_FlushMCCache();
	if (r < -9) {
		Card_InvFileHandles();
		Card_ClearCache();
	}

	if (r != sceMcResSucceed)
		return r;

	if (fh->unknown2 != 0) {
		fh->unknown2 = 0;
		r = Card_FileClose(fd);
		if (r < -9) {
			Card_InvFileHandles();
			Card_ClearCache();
		}

		if (r != sceMcResSucceed)
			return r;
	}

	r = Card_FlushMCCache();
	if (r < -9) {
		Card_InvFileHandles();
		Card_ClearCache();
	}

	return r;
}

int mcio_mcRead(int fd, void *buf, int length)
{
	int r;
	struct MCFHandle *fh;

	if (!(fd < MAX_FDHANDLES))
		return sceMcResDeniedPermit;

	fh = (struct MCFHandle *)&mcio_fdhandles[fd];
	if (!fh->status)
		return sceMcResDeniedPermit;

	if (!fh->rdflag)
		return sceMcResDeniedPermit;

	r = mcio_mcDetect();
	if (r != sceMcResSucceed)
		return r;

	r = Card_FileRead(fd, buf, length);
	if (r < 0) {
		fh->status = 0;
	}

	if (r < -9) {
		Card_InvFileHandles();
		Card_ClearCache();
	}

	return r;
}

int mcio_mcWrite(int fd, void *buf, int length)
{
	int r;
	struct MCFHandle *fh;

	if (!(fd < MAX_FDHANDLES))
		return sceMcResDeniedPermit;

	fh = (struct MCFHandle *)&mcio_fdhandles[fd];
	if (!fh->status)
		return sceMcResDeniedPermit;

	if (!fh->wrflag)
		return sceMcResDeniedPermit;

	r = mcio_mcDetect();
	if (r != sceMcResSucceed)
		return r;

	r = Card_FileWrite(fd, buf, length);
	if (r < 0)
		fh->status = 0;

	if (r < -9) {
		Card_InvFileHandles();
		Card_ClearCache();
	}

	return r;
}

int mcio_mcSeek(int fd, int offset, int origin)
{
	int r;
	struct MCFHandle *fh;

	if (!(fd < MAX_FDHANDLES))
		return sceMcResDeniedPermit;

	fh = (struct MCFHandle *)&mcio_fdhandles[fd];
	if (!fh->status)
		return sceMcResDeniedPermit;

	r = mcio_mcDetect();
	if (r != sceMcResSucceed)
		return r;

	switch (origin) {
		default:
		case SEEK_CUR:
			r = fh->position + offset;
			break;
		case SEEK_SET:
			r = offset;
			break;
		case SEEK_END:
			r = fh->filesize + offset;
			break;
	}

	return fh->position = (r < 0) ? 0 : r;
}

int mcio_mcStat(const char *filename, struct io_dirent *dirent)
{
	int r, fd;
	struct MCFsEntry *pfse;

	fd = mcio_mcOpen(filename, sceMcFileAttrSubdir | sceMcFileAttrReadable);
	if (fd < 0)
		return fd;

	struct MCFHandle *fh = (struct MCFHandle *)&mcio_fdhandles[fd];

	r = Card_ReadDirEntry(fh->cluster, fh->fsindex, &pfse);
	if (r < 0) {
		mcio_mcClose(fd);
		return r;
	}

	mcio_copy_dirent(dirent, pfse);
	r = mcio_mcClose(fd);

	return r;
}

int mcio_mcSetStat(const char *filename, const struct io_dirent *dirent)
{
	int r, fd;
	struct MCFsEntry *pfse, *pfse2;
	struct MCCacheEntry *pmce;

	fd = mcio_mcOpen(filename, sceMcFileAttrSubdir | sceMcFileAttrReadable);
	if (fd < 0)
		return fd;

	struct MCFHandle *fh = (struct MCFHandle *)&mcio_fdhandles[fd];

	int32_t cluster = Card_GetDirEntryCluster(fh->cluster, fh->fsindex);
	r = Card_ReadDirEntry(fh->cluster, fh->fsindex, &pfse2);
	if (r < 0) {
		mcio_mcClose(fd);
		return r;
	}
	mcio_mcClose(fd);

	r = Card_ReadCluster(cluster, &pmce);
	if (r < 0)
		return r;

	pfse = (struct MCFsEntry *)pmce->cl_data;
	if (pfse->cluster != pfse2->cluster)
		pfse++;
	if (pfse->cluster != pfse2->cluster)
		return r;

	mcio_copy_mcentry(pfse, dirent);
	pmce->wr_flag = 1;

	r = Card_FlushMCCache();
	if (r != sceMcResSucceed)
		return r;

	return r;
}

int mcio_mcCreateCrossLinkedFile(const char *real_filepath, const char *dummy_filepath)
{
	int r, fd;
	struct MCFsEntry *pfse;
	struct MCCacheEntry *pmce;

	fd = mcio_mcOpen(real_filepath, sceMcFileAttrFile | sceMcFileAttrReadable);
	if (fd < 0)
		return fd;

	struct MCFHandle *fh = (struct MCFHandle *)&mcio_fdhandles[fd];
	if (fh->drdflag) {
		mcio_mcClose(fd);
		return sceMcResNotFile;
	}

	r = Card_ReadDirEntry(fh->cluster, fh->fsindex, &pfse);
	if (r < 0) {
		mcio_mcClose(fd);
		return r;
	}

	int32_t real_file_cluster = read_le_uint32((uint8_t *)&pfse->cluster);
	int32_t real_file_length = read_le_uint32((uint8_t *)&pfse->length);

	mcio_mcClose(fd);

	fd = mcio_mcOpen(dummy_filepath, sceMcFileAttrWriteable | sceMcFileCreateFile | sceMcFileAttrFile);
	if (fd < 0)
		return fd;

	fh = (struct MCFHandle *)&mcio_fdhandles[fd];

	r = Card_ReadDirEntry(fh->cluster, fh->fsindex, &pfse);
	if (r < 0) {
		mcio_mcClose(fd);
		return r;
	}

	char dummy_filename[33];
	strncpy(dummy_filename, pfse->name, 32);

	int32_t cluster = Card_GetDirEntryCluster(fh->cluster, fh->fsindex);
	if (r < 0) {
		mcio_mcClose(fd);
		return r;
	}

	mcio_mcClose(fd);

	r = Card_ReadCluster(cluster, &pmce);
	if (r < 0)
		return r;

	struct MCFsEntry *fse = (struct MCFsEntry *)pmce->cl_data;
	if (strcmp(dummy_filename, fse->name))
		fse++;
	if (strcmp(dummy_filename, fse->name))
		return r;

	append_le_uint32((uint8_t *)&fse->cluster, real_file_cluster);
	append_le_uint32((uint8_t *)&fse->length, real_file_length);

	pmce->wr_flag = 1;

	r = Card_FlushMCCache();
	if (r != sceMcResSucceed)
		return r;

	return r;
}

int mcio_mcDopen(const char *dirname)
{
	int r;

	r = mcio_mcOpen(dirname, sceMcFileAttrSubdir);
	if (r >= 0) {
		struct MCFHandle *fh = (struct MCFHandle *)&mcio_fdhandles[r];
		if (!fh->drdflag) {
			mcio_mcClose(r);
			return sceMcResNotDir;
		}
	}

	return r;
}

int mcio_mcDclose(int fd)
{
	int r;

	r = mcio_mcClose(fd);

	return r;
}

int mcio_mcDread(int fd, struct io_dirent *dirent)
{
	int r;
	struct MCFHandle *fh = (struct MCFHandle *)&mcio_fdhandles[fd];
	struct MCFsEntry *fse;
	uint16_t mode;

	if (fh->position >= fh->filesize)
		return 0;

	do {
		r = Card_ReadDirEntry(fh->freeclink, fh->position, &fse);
		if (r != sceMcResSucceed)
			return r;

		mode = read_le_uint16((uint8_t *)&fse->mode);
		if (mode & sceMcFileAttrExists)
			break;

		fh->position++;
	} while (fh->position < fh->filesize);

	if (fh->position >= fh->filesize)
		return 0;

	fh->position++;
	memset((void *)dirent, 0, sizeof(struct io_dirent));
	strncpy(dirent->name, fse->name, 32);
	dirent->name[32] = 0;

	if (mode & sceMcFileAttrReadable)
		dirent->stat.mode |= sceMcFileAttrReadable;
	if (mode & sceMcFileAttrWriteable)
		dirent->stat.mode |= sceMcFileAttrWriteable;
	if (mode & sceMcFileAttrExecutable)
		dirent->stat.mode |= sceMcFileAttrExecutable;
	if (mode & sceMcFileAttrPS1)
		dirent->stat.mode |= sceMcFileAttrPS1;
	if (mode & sceMcFileAttrPDAExec)
		dirent->stat.mode |= sceMcFileAttrPDAExec;
	if (mode & sceMcFileAttrDupProhibit)
		dirent->stat.mode |= sceMcFileAttrDupProhibit;
	if (mode & sceMcFileAttrSubdir)
		dirent->stat.mode |= sceMcFileAttrSubdir;
	else
		dirent->stat.mode |= sceMcFileAttrFile;

	dirent->stat.attr = read_le_uint32((uint8_t *)&fse->attr);
	dirent->stat.size = read_le_uint32((uint8_t *)&fse->length);

	dirent->stat.ctime.Resv2 = fse->created.Resv2;
	dirent->stat.ctime.Sec = fse->created.Sec;
	dirent->stat.ctime.Min = fse->created.Min;
	dirent->stat.ctime.Hour = fse->created.Hour;
	dirent->stat.ctime.Day = fse->created.Day;
	dirent->stat.ctime.Month = fse->created.Month;
	dirent->stat.ctime.Year = read_le_uint16((uint8_t *)&fse->created.Year);

	dirent->stat.mtime.Resv2 = fse->modified.Resv2;
	dirent->stat.mtime.Sec = fse->modified.Sec;
	dirent->stat.mtime.Min = fse->modified.Min;
	dirent->stat.mtime.Hour = fse->modified.Hour;
	dirent->stat.mtime.Day = fse->modified.Day;
	dirent->stat.mtime.Month = fse->modified.Month;
	dirent->stat.mtime.Year = read_le_uint16((uint8_t *)&fse->modified.Year);

	return 1;
}

int mcio_mcMkDir(const char *dirname)
{
	return mcio_mcOpen(dirname, 0x40);
}

int mcio_mcGetInfo(int *pagesize, int *blocksize, int *cardsize, int *cardflags)
{
	int r;

	r = mcio_mcDetect();
	if (r != sceMcResSucceed)
		return r;

	uint8_t _cardflags;
	uint16_t _pagesize, _blocksize;
	int32_t _cardsize;
	if (Card_GetSpecs(&_pagesize, &_blocksize, &_cardsize, &_cardflags) != sceMcResSucceed)
		return r;

	_pagesize = read_le_uint16((uint8_t *)&_pagesize);
	*pagesize = (int)_pagesize;
	*blocksize = (int)read_le_uint16((uint8_t *)&_blocksize);
	*cardsize = (int)_cardsize * _pagesize;
	*cardflags = (int)_cardflags;

	return 0;
}

int mcio_mcGetAvailableSpace(int *cardfree)
{
	int r;

	r = mcio_mcDetect();
	if (r != sceMcResSucceed)
		return r;

	struct MCDevInfo *mcdi = (struct MCDevInfo *)&mcio_devinfo;
	if ((int32_t)read_le_uint32((uint8_t *)&mcdi->cardform) == sceMcResNoFormat)
		return sceMcResNoFormat;

	r = Card_FindFree(0);
	if (r == sceMcResFullDevice)
		*cardfree = 0;
	else if (r < 0)
		return r;

	*cardfree = r * MCIO_CLUSTERSIZE;

	return 0;
}

int mcio_mcReadPage(int pagenum, void *buf, void *ecc)
{
	int r;

	r = Card_ReadPage((int32_t)pagenum, (uint8_t *)buf);

	if (ecc)
	{
		struct MCDevInfo *mcdi = (struct MCDevInfo *)&mcio_devinfo;
		uint16_t pagesize = read_le_uint16((uint8_t *)&mcdi->pagesize);
		uint8_t* p_ecc = ecc;

		memset(ecc, 0, pagesize >> 5);
		for (int i = 0; i < pagesize; i += 128, p_ecc += 3)
			Card_DataChecksum(buf + i, p_ecc);
	}

	return r;
}

int mcio_mcUnformat(void)
{
	int r;

	r = mcio_mcDetect();
	if (r != sceMcResSucceed)
		return r;

	r = Card_Unformat();
	if (r != sceMcResSucceed)
		return r;

	return 0;
}

int mcio_mcFormat(void)
{
	int r;

	r = mcio_mcDetect();
	if (r != sceMcResSucceed)
		return r;

	r = Card_Format();
	if (r != sceMcResSucceed)
		return r;

	return 0;
}

int mcio_mcRemove(const char *filename)
{
	int r;

	r = mcio_mcDetect();
	if (r != sceMcResSucceed)
		return r;

	r = Card_Delete(filename, 0);
	if (r < -9) {
		Card_InvFileHandles();
		Card_ClearCache();
	}

	return r;
}

int mcio_mcRmDir(const char *dirname)
{
	int r;

	r = mcio_mcDetect();
	if (r != sceMcResSucceed)
		return r;

	r = Card_Delete(dirname, 0);
	if (r < -9) {
		Card_InvFileHandles();
		Card_ClearCache();
	}

	return r;
}

int mcio_vmcExportImage(const char *output, int add_ecc)
{
	int r;
	int pagesize, blocksize, cardsize, cardflags;

	r = mcio_mcGetInfo(&pagesize, &blocksize, &cardsize, &cardflags);
	if (r < 0)
		return -1;

	FILE *fh = fopen(output, "wb");
	if (fh == NULL)
		return -2;

	void *ecc = malloc(pagesize >> 5);
	void *buf = malloc(pagesize);
	if (buf == NULL || ecc == NULL) {
		fclose(fh);
		return -3;
	}

	for (int i = 0; i < (cardsize / pagesize); i++) {
		mcio_mcReadPage(i, buf, ecc);
		r = fwrite(buf, 1, pagesize, fh);
		if (r != pagesize) {
			free(buf);
			fclose(fh);
			return -4;
		}

		if (!add_ecc)
			continue;

		r = fwrite(ecc, 1, pagesize >> 5, fh);
		if (r != pagesize >> 5) {
			free(buf);
			fclose(fh);
			return -4;
		}
	}

	fclose(fh);
	free(buf);
	free(ecc);

	return sceMcResSucceed;
}
