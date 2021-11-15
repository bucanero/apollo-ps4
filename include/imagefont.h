
// imagefont.bin header structure definition
typedef struct {
	uint16_t bitOrder;
	uint16_t nbrEntries;
	uint32_t indexStart;
} imagefontHeader_t;

// index entries structure definition
typedef struct {
	uint32_t paletteStart;
	uint16_t paletteCompSize;
	uint16_t paletteDecompSize;
	uint16_t unicodeId;
	uint16_t imageWidth;
	uint16_t imageHeight;
	uint16_t unknownData1;
} indexEntry_t;

// palette header structure definition
typedef struct {
	uint16_t colorsCount;
	uint8_t colorChannel;
	uint8_t framesCount;
	uint16_t animTime;
} paletteHeader_t;

// frame structure definition
typedef struct {
	uint32_t frameDataOffset;
	uint16_t frameDataLength;
	uint16_t frameTime;
	uint8_t unknownData2;
	uint8_t alphaMask;
	uint16_t unknownData3;
} frameInfo_t;
