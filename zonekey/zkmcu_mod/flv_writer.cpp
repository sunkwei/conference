#include "stdafx.h"
#include <vector>
#include <sstream>
#include <map>
#include "flv_writer.h"
#include <cc++/thread.h>

#define FLVX_HEADER "FLV\x1\x5\0\0\0\x9\0\0\0\0"
#define FLVX_HEADER_LEN (sizeof(FLVX_HEADER)-1)

/* offsets for packed values */
#define FLV_AUDIO_SAMPLESSIZE_OFFSET 1
#define FLV_AUDIO_SAMPLERATE_OFFSET  2
#define FLV_AUDIO_CODECID_OFFSET     4

#define FLV_VIDEO_FRAMETYPE_OFFSET   4

/* bitmasks to isolate specific values */
#define FLV_AUDIO_CHANNEL_MASK    0x01
#define FLV_AUDIO_SAMPLESIZE_MASK 0x02
#define FLV_AUDIO_SAMPLERATE_MASK 0x0c
#define FLV_AUDIO_CODECID_MASK    0xf0

#define FLV_VIDEO_CODECID_MASK    0x0f
#define FLV_VIDEO_FRAMETYPE_MASK  0xf0

#define AMF_END_OF_OBJECT         0x09

enum {
	FLV_HEADER_FLAG_HASVIDEO = 1,
	FLV_HEADER_FLAG_HASAUDIO = 4,
};

enum {
	FLV_TAG_TYPE_AUDIO = 0x08,
	FLV_TAG_TYPE_VIDEO = 0x09,
	FLV_TAG_TYPE_META  = 0x12,
};

enum {
	FLV_MONO   = 0,
	FLV_STEREO = 1,
};

enum {
	FLV_SAMPLESSIZE_8BIT  = 0,
	FLV_SAMPLESSIZE_16BIT = 1 << FLV_AUDIO_SAMPLESSIZE_OFFSET,
};

enum {
	FLV_SAMPLERATE_SPECIAL = 0, /**< signifies 5512Hz and 8000Hz in the case of NELLYMOSER */
	FLV_SAMPLERATE_11025HZ = 1 << FLV_AUDIO_SAMPLERATE_OFFSET,
	FLV_SAMPLERATE_22050HZ = 2 << FLV_AUDIO_SAMPLERATE_OFFSET,
	FLV_SAMPLERATE_44100HZ = 3 << FLV_AUDIO_SAMPLERATE_OFFSET,
};

enum {
	FLV_CODECID_PCM                  = 0,
	FLV_CODECID_ADPCM                = 1 << FLV_AUDIO_CODECID_OFFSET,
	FLV_CODECID_MP3                  = 2 << FLV_AUDIO_CODECID_OFFSET,
	FLV_CODECID_PCM_LE               = 3 << FLV_AUDIO_CODECID_OFFSET,
	FLV_CODECID_NELLYMOSER_8KHZ_MONO = 5 << FLV_AUDIO_CODECID_OFFSET,
	FLV_CODECID_NELLYMOSER           = 6 << FLV_AUDIO_CODECID_OFFSET,
	FLV_CODECID_AAC                  = 10<< FLV_AUDIO_CODECID_OFFSET,
	FLV_CODECID_SPEEX                = 11<< FLV_AUDIO_CODECID_OFFSET,
};

enum {
	FLV_CODECID_H263    = 2,
	FLV_CODECID_SCREEN  = 3,
	FLV_CODECID_VP6     = 4,
	FLV_CODECID_VP6A    = 5,
	FLV_CODECID_SCREEN2 = 6,
	FLV_CODECID_H264    = 7,
};

enum {
	FLV_FRAME_KEY        = 1 << FLV_VIDEO_FRAMETYPE_OFFSET,
	FLV_FRAME_INTER      = 2 << FLV_VIDEO_FRAMETYPE_OFFSET,
	FLV_FRAME_DISP_INTER = 3 << FLV_VIDEO_FRAMETYPE_OFFSET,
};

typedef enum {
	AMF_DATA_TYPE_NUMBER      = 0x00,
	AMF_DATA_TYPE_BOOL        = 0x01,
	AMF_DATA_TYPE_STRING      = 0x02,
	AMF_DATA_TYPE_OBJECT      = 0x03,
	AMF_DATA_TYPE_NULL        = 0x05,
	AMF_DATA_TYPE_UNDEFINED   = 0x06,
	AMF_DATA_TYPE_REFERENCE   = 0x07,
	AMF_DATA_TYPE_MIXEDARRAY  = 0x08,
	AMF_DATA_TYPE_ARRAY       = 0x0a,
	AMF_DATA_TYPE_DATE        = 0x0b,
	AMF_DATA_TYPE_UNSUPPORTED = 0x0d,
} AMFDataType;

#define FLV_MAKER_VERSION "1.0 rc2"

#pragma pack(push)
#pragma pack(1)

namespace {
	/// 全是char型, 单字节对齐, sizeof 就是长度
	struct FlvTagHead{
		unsigned char TagType;
		unsigned char BE_DataSize[3];
		unsigned char BE_TimeStamp[3];
		unsigned char TimeStampExtended;
		unsigned char BE_StreamID[3]; // always 0;
	};

	struct FlvAVCVideoTagHead{
		unsigned char FrameTypeAndCodecID;
		unsigned char AVCPacketType;
		unsigned char BE_CompositionTime[3];
	};

	struct FlvAACAudioTagHead{
		unsigned char CodecInfo;
		unsigned char AACPacketType;
	};
};

#pragma pack(pop)

typedef unsigned short uint16_t;
typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef unsigned __int64 uint64_t;

enum AtomType
{
	Number = 0,
	Boolean = 1,
	String = 2,
	Object = 3,
	MoveClip = 4,
	Null = 5,
	Undefined = 6,
	Reference = 7,
	MixArray = 8,
	EndOfObject = 9,
	Array = 10,
	Date = 11,
	LongString = 12,
	Unsupported = 13,
	RecordSet = 14,
	Xml = 15,
	ClassObject = 16,
	AMF3Object = 17,
};

namespace {
	struct atom
	{
		AtomType type;
		union {
			struct {
				double val;
			} Number;

			struct {
				bool val;
			} Boolean;

			struct {
				std::string *val;
			} String;

			struct {
				std::vector<std::pair<std::string, atom*> > *val;
			} Object;

			struct {
				int notused;
			} Null;

			struct {
				int notused;
			} EndOfObject;

			struct {
				int cnt;
				std::vector<std::pair<std::string, atom*> > *val;
			} MixArray;
		} data;
	};
};
#define strcasecmp stricmp

static int aac_sample_freq_table[] = {
	96000, 88200, 64000, 48000,
	44100, 32000, 24000, 22050,
	16000, 12000, 11025, 8000,
	7359, -1, -1, -1 
};

/** 提取 aac adts，总是要求 ptr 中的前 7 个字节
*
*	adts:
*	0	12bits	sync			0xFFF
*		1bit	id			1=mpeg4, 0=mpeg2
*		2bits	layer			00 always
*		1bit	protect absent		1
*	2	2bits	profile			
*		4bits	sample index	
*		1bit	private
*		3bits	channel
*		1bit	original/copy
*		1bit	home
*		1bit	copy
*		1bit
*		13bits	len include hdr
*		11bits	adts buffer fullness
*		2bits	no raw data blocks in frame
*		
*	7	16bits	CRC if (protect absent = 0)
*
*		
*/
// sample: ff f1 4c 80 29 7f fc
int aac_parse_adts (unsigned char *ptr, int *pch, int *psamples, int *pbits, int *plen)
{
	int len, chs, samples_index;

	if (ptr[0] != 0xff || ptr[1] != 0xf1) return -1;	// sync, 201b出的数据格式

	len = (ptr[3] & 0x3) << 11;
	len |= ptr[4] << 3;
	len |= (ptr[5] & 0xe0) >> 5;
	*plen = len;

	chs = (ptr[2] & 0x1) << 2;
	chs |= (ptr[3] & 0xc0) >> 6;
	*pch = chs;

	samples_index = (ptr[2] & 0x3c) >> 2;
	*psamples = aac_sample_freq_table[samples_index];

	*pbits = 16;	// 

	return 1;
}

/** 根据当前 adts，生成 audio ConfigExtension
*/
int buildAudioConfigExt (unsigned char data[3],
	int samplerate, int chs)
{
	int index = -1;
	for (unsigned int i = 0; i < sizeof(aac_sample_freq_table)/sizeof(int); i++) {
		if (samplerate == aac_sample_freq_table[i]) {
			index = i;
			break;
		}
	}

	unsigned short d;

	if (index >= 0) {
		d = (unsigned short)index;
		d <<= 7;
		d |= 0x1000;
		d |= (chs << 3);

		data[0] = *((unsigned char*)&d+1);
		data[1] = *((unsigned char*)&d+0);
		data[2] = 0x06;

		return 1;
	}

	return 0;
}

typedef int (*PFN_decode)(atom *, int len, const char *, int *);
static int decode_number (atom *, int, const char *, int*);
static int decode_boolean (atom*, int, const char*, int*);
static int decode_string (atom*, int, const char*, int*);
static int decode_object (atom*, int, const char*, int *);
static int decode_endofobject (atom*, int, const char*, int *);
static int decode_ERROR (atom*, int, const char*, int *);
static int decode_null (atom *, int, const char*, int*);
static int decode_undefined (atom *var, int len, const char *data, int *used);

static int my_fprintf(FILE *fp, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	char buf[1024];

	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	return fprintf(stderr, buf);
}

#define fprintf(fp, fmt, ...) my_fprintf(fp, fmt, __VA_ARGS__)

static PFN_decode getdecoder (char c)
{
	switch (c) {
	case Number:
		return decode_number;
	case Boolean:
		return decode_boolean;
	case String:
		return decode_string;
	case Object:
		return decode_object;
	case EndOfObject:
		return decode_endofobject;
	case Null:
		return decode_null;
	case Undefined:
		return decode_undefined;
	default:
		return decode_ERROR;
	}
}

int atom_encode (const atom *var, std::ostream &os)
{
	switch (var->type) {
	case Number:
		do {
			/// double ???停?使?? 9???纸?
			unsigned char *ptr = (unsigned char *)&var->data.Number.val;
			os << (char)0;
			os << *(ptr+7) << *(ptr+6) << *(ptr+5) << *(ptr+4) << *(ptr+3) << *(ptr+2) << *(ptr+1) << *ptr;
		} while (0);
		break;

	case Boolean:
		do {
			/// bool ???停?使?? 2???纸?
			os << (char)1;
			os << (var->data.Boolean.val ? (char)1 : (char)0);
		} while (0);
		break;

	case String:
		do {
			// ?址???????
			os << (char)2;
			int size = var->data.String.val->size();
			unsigned char *ptr = (unsigned char*)&size;
			os << *(ptr+1) << *ptr;
			os << *(var->data.String.val);
		} while (0);
		break;

	case Object:
		do {
			// Object ????
			os << (char)3;
			std::vector<std::pair<std::string, atom*> >::const_iterator it;
			for (it = var->data.Object.val->begin(); 
				it != var->data.Object.val->end(); ++it) {
					// key = len(2) + str
					int size = it->first.size();
					unsigned char *ptr = (unsigned char*)&size;
					os << *(ptr+1) << *ptr;
					os << it->first;
					// value
					atom_encode(it->second, os);
			}

			// End Of Object
			os << (char)0 << (char)0 << (char)9;
		} while (0);
		break;

	case MixArray:
		do {
			// mixarray, 08 + 4B(cnt) + kv1 + kv2 + ...
			os << (char)8;
			unsigned char *p = (unsigned char*)&var->data.MixArray.cnt;
			os << (unsigned char)(p[3]) << (unsigned char)(p[2])
				<< (unsigned char)(p[1]) << (unsigned char)(p[0]);
			std::vector<std::pair<std::string, atom*> >::const_iterator it;
			for (it = var->data.MixArray.val->begin();
				it != var->data.MixArray.val->end(); ++it) {
					int size = it->first.size();
					p = (unsigned char*)&size;
					os << *(p+1) << *p;
					os << it->first;
					atom_encode(it->second, os);
			}
			os << (char)0 << (char)0 << (char)9;
		} while (0);
		break;

	case Null:
		os << (char)Null;
		break;

	default:
		fprintf(stderr, "[atom] encode UNDEFINED type, %d\n", var->type);
		exit(-1);
		break;
	}
	return 1;
}

int atom_size (const atom *var)
{
	switch (var->type) {
	case Number:
		return 9;
	case Boolean:
		return 2;
	case String:
		return 3 + var->data.String.val->size();
	case Object:
		do {
			int size = 1;
			std::vector<std::pair<std::string, atom *> >::const_iterator it;
			for (it = var->data.Object.val->begin(); 
				it != var->data.Object.val->end(); ++it) {
					size += 2 + it->first.size();
					size += atom_size(it->second);
			}
			size += 3;	// end of Object
			return size;
		} while (0);
		break;

	case MixArray:
		do {
			int size = 1 + 4;	// 8 + 4Bytes cnt
			std::vector<std::pair<std::string, atom *> >::const_iterator it;
			for (it = var->data.MixArray.val->begin();
				it != var->data.Object.val->end(); ++it) {
					size += 2 + it->first.size();
					size += atom_size(it->second);
			}
			size += 3;	// end
			return size;
		} while (0);
		break;

	case Null:
		return 1;
	case Undefined:
		return 1;
	default:
		fprintf(stderr, "[atom] get atom size, UNDEF type=%d\n", var->type);
		exit(-1);
	}
	return 0;
}

void atom_release (atom *var)
{
	if (!var) return;

	if (var->type == String) {
		delete (var->data.String.val);
	}
	else if (var->type == Object) {
		std::vector<std::pair<std::string, atom*> >::iterator it;
		for (it = var->data.Object.val->begin(); 
			it != var->data.Object.val->end(); ++it) {
				atom_release(it->second);
		}
		delete (var->data.Object.val);
	}
	else if (var->type == MixArray) {
		std::vector<std::pair<std::string, atom*> >::iterator it;
		for (it = var->data.MixArray.val->begin(); 
			it != var->data.MixArray.val->end(); ++it) {
				atom_release(it->second);
		}
		delete (var->data.MixArray.val);
	}

	free(var);
}

atom *atom_decode (const char *data, size_t len, size_t *used)
{
	atom *var = (atom*)malloc(sizeof(atom));
	memset(var, 0, sizeof(atom));
	var->type = (AtomType)(*data);
	PFN_decode proc = getdecoder(*data);
	int rc = proc(var, len, data, (int*)used);
	if (rc < 0) {
		atom_release(var);
		var = 0;
		fprintf(stderr, "[rtmpsrv] parse atom ERR!\n");
		//log2file("[] atom_decode exception: ....\n");
		//log2file(data, len);
		return 0;
	}
	return var;
}

atom *atom_make_string (const char *p, size_t len)
{
	atom *var = (atom *)malloc(sizeof(atom));
	var->type = String;
	var->data.String.val = new std::string(p, len);
	return var;
}

atom *atom_make_string (const char *p)
{
	atom *var = (atom *)malloc(sizeof(atom));
	var->type = String;
	var->data.String.val = new std::string(p, strlen(p));
	return var;
}

atom *atom_make_number (double v)
{
	atom *var = (atom *)malloc(sizeof(atom));
	var->type = Number;
	var->data.Number.val = v;
	return var;
}

atom *atom_make_null ()
{
	atom *var = (atom *)malloc(sizeof(atom));
	var->type = Null;
	return var;
}

atom *atom_make_mixarray (size_t cnt)
{
	atom *var = (atom *)malloc(sizeof(atom));
	var->type = MixArray;
	var->data.MixArray.cnt = cnt;
	var->data.MixArray.val = new std::vector<std::pair<std::string, atom*> >;
	return var;
}

atom *atom_make_boolean (bool v)
{
	atom *var = (atom *)malloc(sizeof(atom));
	var->type = Boolean;
	var->data.Boolean.val = v;
	return var;
}

atom *atom_make_object ()
{
	atom *var = (atom *)malloc(sizeof(atom));
	var->type = Object;
	var->data.Object.val = new std::vector<std::pair<std::string, atom*> >;
	return var;
}

void atom_object_append (atom *obj, const char *key, atom *var)
{
	std::pair<std::string, atom*> pair(key, var);
	if (obj->type == Object)
		obj->data.Object.val->push_back(pair);
	else if (obj->type == MixArray)
		obj->data.MixArray.val->push_back(pair);
}

/// TYPE_NUMBER
static int decode_number (atom*var, int len, const char *data, int *used)
{
	char d[8];
	memcpy(d, data+1, sizeof(d));
	for (int i = 0; i < 4; i++) {
		char x = d[i];
		d[i] = d[7-i];
		d[7-i] = x;
	}
	var->data.Number.val = *(double*)d;
	*used = 9;

	return 1;
}

/// TYPE_BOOLEAN
static int decode_boolean (atom *var, int len, const char *data, int *used)
{
	var->data.Boolean.val = *(data+1) == 0x00 ? false : true;
	*used = 2;

	return 1;
}

/// TYPE_STRING
static int decode_string (atom *var, int len, const char *data, int *used)
{
	uint16_t strlen = ntohs(*(uint16_t*)(data+1));
	std::string *str = new std::string(data+3, strlen);
	var->data.String.val = str;
	*used = strlen+3;

	return 1;
}

/// TYPE_OBJECT
static int decode_object (atom *var, int len, const char *data, int *used)
{
	var->data.Object.val = new std::vector<std::pair<std::string, atom*> >;

	const char *next = data+1;
	int rest = len-1;

	*used = 1;

	while (1) {
		uint16_t strlen = ntohs(*(uint16_t*)next);
		if (strlen > len - *used) {
			fprintf(stderr, "[decode_object] ERR: TOOOOO long size? %d:%d:%d\n", strlen, len, len-*used);
			return -1;
		}
		std::string key = std::string(next+2, strlen);

		next += 2+strlen;
		rest -= 2+strlen;

		*used += 2+strlen;

		int u;
		atom *value = atom_decode(next, rest, (size_t*)&u);
		if (!value) {
			return -1;
		}
		next += u;
		rest -= u;

		*used += u;

		std::pair<std::string, atom*> kv(key, value);
		var->data.Object.val->push_back(kv);

		if (value->type == EndOfObject)
			break;
	}

	return 1;
}

static int decode_endofobject (atom *var, int len, const char *data, int *used)
{
	*used = 1;
	return 1;
}

static int decode_null (atom*var, int len, const char *data, int *used)
{
	*used = 1;
	return 1;
}

static int decode_undefined (atom *var, int len, const char *data, int *used)
{
	*used = 1;
	return 1;
}

/// TYPE_ERROR :)
static int decode_ERROR (atom *var, int len, const char *data, int *used)
{
	fprintf(stderr, "[rtmp var] decode ERR\n");
	return 1;
}

void dumpatom (const atom *var)
{
	if (var->type == String) {
		char fmt[32];
		sprintf(fmt, "[dumpatom] String: %%.%ds\n", var->data.String.val->size());
		fprintf(stderr, fmt, var->data.String.val->c_str());
	}
	else if (var->type == Object) {
		fprintf(stderr, "[dumpatom] Object:\n");
		for (size_t i = 0; i < var->data.Object.val->size(); i++) {
			fprintf(stderr, "\tkey=%s, ", (*var->data.Object.val)[i].first.c_str());
			dumpatom((*var->data.Object.val)[i].second);
		}
	}
	else if (var->type == Number) {
		fprintf(stderr, "[dumpatom] Number: %f\n", var->data.Number.val);
	}
	else if (var->type == Null) {
		fprintf(stderr, "[dumpatom] Null\n");
	}
	else if (var->type == Boolean) {
		fprintf(stderr, "[dumpatom] Boolean: %d\n", var->data.Boolean.val);
	}
	else if (var->type == Undefined) {
		fprintf(stderr, "[dumpatom] Undefined\n");
	}
	else if (var->type == EndOfObject) {
		fprintf(stderr, "[dumpatom] EndOfObject\n");
	}
	else {
		fprintf(stderr, "[dumpatom] UNKNOWN atom\n");
	}
}

static int flv_write_data(HANDLE file, const void *data, uint32_t len)
{
	const char *p = (const char *)data;
	uint32_t rest = len;
	while (rest > 0) {
		DWORD bytes;
		BOOL rc = WriteFile(file, p, rest, &bytes, 0);
		if (!rc) {
			fprintf(stderr, "[flv_writer] Writer %u bytes ERR\n", len);
			return -1;
		}

		p += bytes;
		rest -= bytes;
	}

	return len;
}

static int flv_write_file_header(HANDLE file)
{
	return flv_write_data(file, FLVX_HEADER, FLVX_HEADER_LEN);
}

static int flv_make_metadata_tag (std::ostream &os,
	int _video_width,
	int _video_height,
	int _audio_samplesize = 16,
	int _audio_samplerate=32000,
	int _audio_chs = 2,
	int _audio_id = 10)
{
	FlvTagHead meta_head;
	meta_head.TagType = FLV_TAG_TYPE_META;
	meta_head.BE_TimeStamp[0]
	= meta_head.BE_TimeStamp[1]
	= meta_head.BE_TimeStamp[2]
	= 0;
	meta_head.TimeStampExtended = 0;
	meta_head.BE_StreamID[0]
	= meta_head.BE_StreamID[1]
	= meta_head.BE_StreamID[2]
	= 0;
	std::stringstream ss;

	atom *tmp = atom_make_string("onMetaData");
	atom_encode(tmp, ss);
	atom_release(tmp);

	tmp = atom_make_mixarray(12);
	//	tmp = atom_make_object();
	if (tmp) {
		atom *tt;

		tt = atom_make_number(0);
		atom_object_append(tmp, "duration", tt);

		tt = atom_make_number(0);
		atom_object_append(tmp, "filesize", tt);

		// 写入 video: width, hegiht, id
		tt = atom_make_number(_video_width);
		atom_object_append(tmp, "width", tt);
		tt = atom_make_number(_video_height);
		atom_object_append(tmp, "height", tt);
		tt = atom_make_number(7);	// avc
		atom_object_append(tmp, "videocodecid", tt);

		// 写入 audio: id, samplesize, sample rate, id
		tt = atom_make_number(_audio_samplesize);	// 16 bits
		atom_object_append(tmp, "audiosamplesize", tt);
		tt = atom_make_number(_audio_samplerate);	// 32k
		atom_object_append(tmp, "audiosamplerate", tt);
		tt = atom_make_boolean(_audio_chs == 2);
		atom_object_append(tmp, "stereo", tt);
		tt = atom_make_number(_audio_id);	// aac
		atom_object_append(tmp, "audiocodecid", tt);

		tt = atom_make_string("Zonekey flv muxer");
		atom_object_append(tmp, "builder", tt);

		tt = atom_make_string(FLV_MAKER_VERSION);
		atom_object_append(tmp, "version", tt);

		// ....
	}
	atom_encode(tmp, ss);
	atom_release(tmp);

	// 此时 ss 中包含了 metadata 的实际内容
	std::string body = ss.str();

	size_t datasize = body.size();
	unsigned char *plen = (unsigned char *)&datasize;
	meta_head.BE_DataSize[0] = plen[2];
	meta_head.BE_DataSize[1] = plen[1];
	meta_head.BE_DataSize[2] = plen[0];

	size_t tagsize = datasize + 11;	// 11 为 FlvTagHead 占用的长度
	plen = (unsigned char*)&tagsize;

	os << std::string((char*)&meta_head, 11);
	os << body;
	os << *(plen+3);
	os << *(plen+2);
	os << *(plen+1);
	os << *(plen+0);

	return tagsize + 4;
}

static int flv_write_meta_tag(HANDLE file, int width, int height, 
	int a_bits, int a_rate, int a_chs, int a_id)
{
	std::stringstream ss;
	flv_make_metadata_tag(ss, width, height, a_bits, a_rate, a_chs, a_id);
	std::string str = ss.str();

	return flv_write_data(file, str.data(), str.size());
}

static const unsigned char *avc_find_startcode(const unsigned char *p, const unsigned char *end)
{
	const unsigned char *a = p + 4 - ((intptr_t)p & 3);

	for (end -= 3; p < a && p < end; p++) {
		if (p[0] == 0 && p[1] == 0 && p[2] == 1)
			return p;
	}

	for (end -= 3; p < end; p += 4) {
		unsigned int x = *(const unsigned int*)p;
		if ((x - 0x01010101) & (~x) & 0x80808080) { // generic
			if (p[1] == 0) {
				if (p[0] == 0 && p[2] == 1)
					return p;
				if (p[2] == 0 && p[3] == 1)
					return p+1;
			}
			if (p[3] == 0) {
				if (p[2] == 0 && p[4] == 1)
					return p+2;
				if (p[4] == 0 && p[5] == 1)
					return p+3;
			}
		}
	}

	for (end += 3; p < end; p++) {
		if (p[0] == 0 && p[1] == 0 && p[2] == 1)
			return p;
	}

	return end + 3;
}

/** @warning buf 必须释放
	h264 annex b 格式转换为 nals 格式
*/
static int avc_parse_nal_units (const unsigned char *data,
	unsigned char **buf, int *size)
{
	/** 把 startcode 转换为 len
	*/
	const unsigned char *p = data;
	const unsigned char *end = p + *size;
	const unsigned char *nal_start, *nal_end;

	unsigned char *pbuf = (unsigned char*)malloc(*size + 512);	// FIXME: 512
	//unsigned char *pbuf = *buf;
	int n = 0;

	nal_start = avc_find_startcode(p, end);
	while (nal_start < end) {
		while (!*(nal_start++));

		nal_end = avc_find_startcode(nal_start, end);
		int len = nal_end - nal_start;

		// 每个 nal 的 startcode 变为 nal长度
		unsigned char *tmp = (unsigned char*)&len;
		pbuf[n++] = tmp[3];
		pbuf[n++] = tmp[2];
		pbuf[n++] = tmp[1];
		pbuf[n++] = tmp[0];

		memcpy(&pbuf[n], nal_start, len);
		n += len;

		nal_start = nal_end;
	}

	*size = n;
	*buf = pbuf;

	return 1;
}

static int avc_remove_sps_pps_nals (const unsigned char *data,
	unsigned char **buf, int *size)
{
	// 类似 avc_parse_nal_units，但是去掉 sps, pps nal
	const unsigned char *p = data;
	const unsigned char *end = p+*size;
	const unsigned char *nal_start, *nal_end;

	unsigned char *pbuf = (unsigned char *)malloc(*size+512);
	int n = 0;

	nal_start = avc_find_startcode(p, end);
	while (nal_start < end) {
		while (!*(nal_start++));  // 跳过 start code
		nal_end = avc_find_startcode(nal_start, end);   // nal_end 指向下一个
		int len = nal_end - nal_start;

		// 剔除 sps nal, pps nal
		unsigned char nal_type = nal_start[0] & 0x1f;
		if (nal_type != 7 && nal_type != 8) {
			// 写入 buf
			unsigned char *pl = (unsigned char*)&len;
			pbuf[n++] = pl[3];
			pbuf[n++] = pl[2];
			pbuf[n++] = pl[1];
			pbuf[n++] = pl[0];
			memcpy(&pbuf[n], nal_start, len);
			n += len;
		}

		nal_start = nal_end;
	}

	*size = n;
	*buf = pbuf;

	return n;
}

static void modifyDuration(HANDLE file, double duration)
{
	char *p = (char *)&duration;
	DWORD bytes;

	// 指向meta tag 的duration
	SetFilePointer(file, 0x35, 0, FILE_BEGIN);

	for (int i = 7; i >= 0; i--) {
		WriteFile(file, p+i, 1, &bytes, 0);
	}
}

static void modifyFilesize (HANDLE file, uint64_t pos)
{
	double fs = (double)pos;
	char *p = (char*)&fs;
	DWORD bytes;

	// file->seek(0x35 + 8 + 2 + 9); // 指向meta tag 的 filesize
	//  fseek(fp, 0x35 + 8 + 2 + 9, SEEK_SET);
	SetFilePointer(file, 0x35+8+2+9, 0, FILE_BEGIN);

	for (int i = 7; i >= 0; i--) {
		//file->putChar(*(p+i));
		WriteFile(file, p+i, 1, &bytes, 0);
	}
}

namespace {

#define AAC 10
#define SPEEX 11
#define H264 7

	struct Ctx
	{
		HANDLE file_;
		int audio_type_;	// 0 没有初始化，10 aac, 11 speex
		unsigned int stamp_audio_, stamp_video_, stamp_last_video_;
		double stamp_begin_audio_, stamp_begin_video_;
		double stamp_av_origin_diff;
		bool video_spec_;	// 是否已经写入 video spec.
		uint64_t file_size_;
		int av_delta_;	// 用于微调音视频时间戳差值
		ost::Mutex cs_writer_;

		Ctx()
		{
			file_ = INVALID_HANDLE_VALUE;
			audio_type_ = 0; // 必须收到了 audio 之后，再写入 meta tag. 
			video_spec_ = false;
			stamp_audio_ = 0;
			stamp_video_ = stamp_last_video_ = 0;
			file_size_ = 0;
			av_delta_ = 0;
		}

		// 打开文件写.
		int open(const char *filename)
		{
			ost::MutexLock al(cs_writer_);

			file_ = CreateFileA(filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, 0);
			if (file_ == INVALID_HANDLE_VALUE) {
				fprintf(stderr, "[flv_writer] %s: CreateFile for %s err\n",
					__FUNCTION__, filename);
				return -1;
			}

			int size = flv_write_file_header(file_);
			if (size < 0) {
				CloseHandle(file_);
				file_ = INVALID_HANDLE_VALUE;
				return -1;
			}

			file_size_ += size;

			return 0;
		}

		void close()
		{
			ost::MutexLock al(cs_writer_);

			if (file_ != INVALID_HANDLE_VALUE) {

				if (audio_type_ != 0) {
					modifyDuration(file_, stamp_audio_ / 1000.0);
					modifyFilesize(file_, file_size_);
				}

				CloseHandle(file_);
			}
			file_ = INVALID_HANDLE_VALUE;
		}

		int write_h264_spec(const void *data, int len)
		{
			ost::MutexLock al(cs_writer_);

			unsigned char *out_buf = 0; // must free
			int out_size = len;
			avc_parse_nal_units((unsigned char*)data, &out_buf, &out_size);

			unsigned char *ptr = out_buf;
			unsigned char *tail = ptr + out_size;
			unsigned char *sps = 0; int sps_size = 0;
			unsigned char *pps = 0; int pps_size = 0;

			while (ptr < tail) {
				unsigned int nal_size = htonl(*(unsigned int*)ptr); // nal len
				unsigned char nal_type = ptr[4] & 0x1f;

				if (nal_type == 7) {
					// sps
					sps = ptr + 4;
					sps_size = nal_size;
				}
				else if (nal_type == 8) {
					// pps
					pps = ptr + 4;
					pps_size = nal_size;
				}

				ptr += nal_size + 4;    // next nal
			}

			unsigned char extra[128];
			extra[0] = 1;               // version
			extra[1] = sps[1];          // profile
			extra[2] = sps[2];          // profile compat
			extra[3] = sps[3];          // level
			extra[4] = 0xff;		// 6 bits reserved (111111) + 2 bits nal size length - 1 (11)
			extra[5] = 0xe1;		// 3 bits reserved (111) + 5 bits number of sps (00001)
			extra[6] = (sps_size & 0xff00) >> 8;
			extra[7] = sps_size & 0xff;
			memcpy(&extra[8], sps, sps_size);   // cp sps
			extra[8+sps_size] = 1;      // num of pps ?
			extra[8+sps_size+1] = (pps_size & 0xff00) >> 8;
			extra[8+sps_size+2] = pps_size & 0xff;
			memcpy(&extra[8+sps_size+3], pps, pps_size);
			int extra_size = 8+sps_size+3+pps_size;

			unsigned char tag[128];
			/*
			09              // video tag
			00 00 2c        // data size
			00 00 00        // time stamp
			00              // time stamp ext
			00 00 00        // stream id: always 0
			17              // video frame type & codec id
			00              // AVCPacketType 0: AVC sequence header 1: AVC NALU
			00 00 00        // if AVCPacketType == 1: Composition time offset
			01 4d 40 1e ff e1 00 18 67 4d 40 1e 9e 52 01 60  24 d8 07 28 30 30 32 00 00 1c 20 00 05 7e 41 08  01 00 04 68 cb 8d c8 //extra data
			00 00 00 37     // PreviousTagSize (this tag)
			*/
			memset(tag, 0, sizeof(tag));
			tag[0] = 9;
			tag[11] = 0x17;
			memcpy(tag+16, extra, extra_size);

			do {
				// previous tag size
				int prev_tag_size = 16 + extra_size;
				unsigned char *p = (unsigned char*)&prev_tag_size;
				unsigned char *ps = tag + 16 + extra_size;
				ps[0] = p[3];
				ps[1] = p[2];
				ps[2] = p[1];
				ps[3] = p[0];
			} while (0);

			do {
				// tag data size
				int data_size = 5 + extra_size;
				unsigned char *p = (unsigned char*)&data_size;
				unsigned char *ds = tag + 1;
				ds[0] = p[2];
				ds[1] = p[1];
				ds[2] = p[0];
			} while (0);

			int rc = flv_write_data(file_, tag, 16 + extra_size + 4);

			free(out_buf);
			file_size_ += rc;
			return rc;
		}

		int save_h264(const void *data, int len, 
			int key, double stamp,	// stamp 为 秒单位
			int width, int height)
		{
			ost::MutexLock al(cs_writer_);
			if (audio_type_ == 0) return 0; // 必须等待写入音频之后，再开始视频
			if (!video_spec_) {
				if (key) {
					write_h264_spec(data, len);
					video_spec_ = true;
					stamp_begin_video_ = stamp;
					stamp_av_origin_diff = stamp_audio_;
				}
			}

			if (!video_spec_) return 0;

			// 根据时间差值，构造 stamp_video_;
			stamp_video_ = 1000.0 * (stamp - stamp_begin_video_) + stamp_av_origin_diff;
			stamp_video_ += av_delta_;

			// 防止视频时间戳错乱 :)
			if (stamp_video_ < stamp_last_video_)
				stamp_video_ = stamp_last_video_ + 5;
			stamp_last_video_ = stamp_video_;

			// 根据 stamp_audio_ 进行时间戳同步.
			// 当 abs(media_audio_ - media_video_) > 300 ms 时，开始调整：每次修正1毫秒
			//		总是保持音频时间戳不变！修正视频时间戳.
			//		总是一分钟后，再调整.
			if (stamp_audio_ > 60000) {
				// 大于一分钟.

				if (stamp_audio_ > stamp_video_) {
					if (stamp_audio_ - stamp_video_ > 300) {
						// 这个说明视频时间戳慢，需要增加 av_delta 的值
						av_delta_ += 1;
					}
				}
				else if (stamp_audio_ < stamp_video_) {
					if (stamp_video_ - stamp_audio_ > 300) {
						// 设置说明视频时间戳太快了，需要慢点 :).
						av_delta_ -= 1;
					}
				}
			}

			// 写入数据帧
#define AVC_FLAGS_SIZE 5

			unsigned char *ptr = 0;
			int raw_size = len;

			// XXX: 为了支持分辨率变化，需要保留 sps/pps
			//    avc_remove_sps_pps_nals((unsigned char*)data, &ptr, &raw_size);
			avc_parse_nal_units((unsigned char*)data, &ptr, &raw_size);

			// write tag head
			do {
				FlvTagHead head;
				memset(&head, 0, sizeof(head));

				head.TagType = FLV_TAG_TYPE_VIDEO;

				head.BE_StreamID[0] = head.BE_StreamID[1] = head.BE_StreamID[2] = 0;

				unsigned char *pstamp = (unsigned char*)&stamp_video_;
				head.BE_TimeStamp[0] = pstamp[2];
				head.BE_TimeStamp[1] = pstamp[1];
				head.BE_TimeStamp[2] = pstamp[0];
				head.TimeStampExtended = pstamp[3];

				int datasize = raw_size + AVC_FLAGS_SIZE;
				unsigned char *pds = (unsigned char*)&datasize;
				head.BE_DataSize[0] = pds[2];
				head.BE_DataSize[1] = pds[1];
				head.BE_DataSize[2] = pds[0];

				if (flv_write_data(file_, &head, sizeof(head)) < 0) {
					free(ptr);
					return -1;
				}
			} while (0);

			// write avc flags
			do {
				unsigned char avc_flags[AVC_FLAGS_SIZE];
				avc_flags[0] = key ? (FLV_CODECID_H264 | FLV_FRAME_KEY) : (FLV_CODECID_H264 | FLV_FRAME_INTER);
				avc_flags[1] = 1; // 0: avc sequence header, 1: NALU
				avc_flags[2] = avc_flags[3] = avc_flags[4] = 0; // composition, pts - dts

				if (flv_write_data(file_, avc_flags, AVC_FLAGS_SIZE) < 0) {
					free(ptr);
					return -1;
				}
			} while (0);

			// write video data
			do {
				if (flv_write_data(file_, ptr, raw_size) < 0) {
					free(ptr);
					return -1;
				}

				free(ptr);  // non used

			} while (0);

			// write tag size
			int tagsize = (htonl(raw_size + AVC_FLAGS_SIZE + sizeof(FlvTagHead)));
			if (flv_write_data(file_, &tagsize, 4) < 0) {
				return -1;
			}

			file_size_ += tagsize+4;
			return tagsize + 4;
		}

		int write_aac_spec(const void *data, int len)
		{
#define AUDIO_EXTRA_SIZE 2
			ost::MutexLock al(cs_writer_);

			// 构造 audio tag of spec
			int chs, samplesize, rate, framelen;
			aac_parse_adts((unsigned char*)data, &chs, &rate, &samplesize, &framelen);
			unsigned char extra[AUDIO_EXTRA_SIZE+1];
			buildAudioConfigExt(extra, rate, chs);

			unsigned char tag[128];
			/*
			08
			00 00 04
			00 00 00
			00
			00 00 00
			af
			00
			11 90
			00 00 00 0f
			*/
			memset(tag, 0, sizeof(tag));
			tag[0] = 0x08;
			tag[11] = 0xaf;
			memcpy(tag+13, extra, AUDIO_EXTRA_SIZE);

			do {
				// 设置 previous tag size
				unsigned char *prev_tag_size = tag + 13 + AUDIO_EXTRA_SIZE;
				int prev_size = 13 + AUDIO_EXTRA_SIZE;
				unsigned char *p = (unsigned char*)&prev_size;
				prev_tag_size[0] = p[3];
				prev_tag_size[1] = p[2];
				prev_tag_size[2] = p[1];
				prev_tag_size[3] = p[0];
			} while (0);

			do {
				// data size
				unsigned char *tag_size = tag + 1;     // tag size
				int datasize = 2 + AUDIO_EXTRA_SIZE;
				unsigned char *p = (unsigned char*)&datasize;
				tag_size[0] = p[2];
				tag_size[1] = p[1];
				tag_size[2] = p[0];
			} while (0);

			// 此时 tag:13 + AUDIO_EXTRA_SIZE + 4
			file_size_ += 13+AUDIO_EXTRA_SIZE+4;
			return flv_write_data(file_, tag, 13+AUDIO_EXTRA_SIZE+4);
		}

		int save_aac(const void *data, int len, double stamp)
		{
			ost::MutexLock al(cs_writer_);

			/** FIXME: 目前 aac 仅仅支持 2/32000/16 的格式
			 */

			if (audio_type_ == 0) {
				audio_type_ = AAC;

				// FIXME: 此时还没有获取到视频呢！不过既然需要支持视频分辨率变化，随便写一个吧. 
				int chs, samplesize, rate, framelen;
				aac_parse_adts((unsigned char*)data, &chs, &rate, &samplesize, &framelen);
				flv_write_meta_tag(file_, 960, 540, samplesize, rate, chs, AAC);

				stamp_begin_audio_ = stamp;

				// 写入 audio spec
				write_aac_spec(data, len);
			}
			else if (audio_type_ != AAC) {
				fprintf(stderr, "[flv_writer] %s: only SPEEX audio supported!\n", __FUNCTION__);
				return -1;
			}

			// 写入 audio data
#define AAC_FLAGS_SIZE 2
#define ADTS_SIZE 7

			// write audio tag
			do {
				FlvTagHead head;
				memset(&head, 0, sizeof(head));

				// type
				head.TagType = FLV_TAG_TYPE_AUDIO;

				// stream id
				head.BE_StreamID[0] = head.BE_StreamID[1] = head.BE_StreamID[2] = 0;

				// stamp
				unsigned char *pstamp = (unsigned char*)&stamp_audio_;
				head.BE_TimeStamp[0] = pstamp[2];
				head.BE_TimeStamp[1] = pstamp[1];
				head.BE_TimeStamp[2] = pstamp[0];
				head.TimeStampExtended = pstamp[3];

				// data size
				int tag_data_size = len - ADTS_SIZE + AAC_FLAGS_SIZE;
				unsigned char *pds = (unsigned char*)&tag_data_size;
				head.BE_DataSize[0] = pds[2];
				head.BE_DataSize[1] = pds[1];
				head.BE_DataSize[2] = pds[0];

				if (flv_write_data(file_, &head, sizeof(head)) < 0) {
					return -2;  // write err
				}
			} while (0);

			/** FIXME：音频时间戳总是单调的增加 32ms
			 */
			stamp_audio_ += 32;

			// aac flag
			unsigned char aac_head[AAC_FLAGS_SIZE] = {
				FLV_CODECID_AAC | FLV_SAMPLERATE_44100HZ | FLV_SAMPLESSIZE_16BIT | FLV_STEREO,
				1 
			};
			if (flv_write_data(file_, aac_head, AAC_FLAGS_SIZE) < 0) {
				return -3;  // write err
			}

			// aac raw data
			if (flv_write_data(file_, (unsigned char*)data+ ADTS_SIZE, len - ADTS_SIZE) < 0) {
				return -4;
			}

			// write tag size
			int tag_size = htonl(len-ADTS_SIZE + AAC_FLAGS_SIZE + sizeof(FlvTagHead));
			if (flv_write_data(file_, (unsigned char*)&tag_size, 4) < 0) {
				return -5;
			}

			file_size_ += ntohl(tag_size)+4;
			return ntohl(tag_size) + 4;
		}

		int save_speex(const void *data, int len,
			double stamp, int channels, int rate, int bits)
		{
			ost::MutexLock al(cs_writer_);

			// TODO: to save speex data
			if (audio_type_ == 0) {
				// 第一个包.
				audio_type_ = SPEEX;
				stamp_begin_audio_ = stamp;
				flv_write_meta_tag(file_, 960, 540, 16, 16000, 1, SPEEX);
			}
			else if (audio_type_ != SPEEX) {
				// 不能中间修改音频格式！.
				fprintf(stderr, "[flv_writer] %s: only AAC audio supported!\n", __FUNCTION__);
				return -1;
			}

			/** XXX: 对于 flv 中，总是要求 1/16000/16 格式
			 */
			// write audio tag
			do {
				FlvTagHead head;
				memset(&head, 0, sizeof(head));

				head.TagType = FLV_TAG_TYPE_AUDIO;
				head.BE_StreamID[0] = head.BE_StreamID[1] = head.BE_StreamID[2] = 0;

				unsigned char *pstamp = (unsigned char*)&stamp_audio_;
				head.BE_TimeStamp[0] = pstamp[2];
				head.BE_TimeStamp[1] = pstamp[1];
				head.BE_TimeStamp[2] = pstamp[0];
				head.TimeStampExtended = pstamp[3];

				int tag_data_size = len + 1;	// flags bytes
				unsigned char *pds = (unsigned char*)&tag_data_size;
				head.BE_DataSize[0] = pds[2];
				head.BE_DataSize[1] = pds[1];
				head.BE_DataSize[2] = pds[0];

				if (flv_write_data(file_, &head, sizeof(head)) < 0) {
					return -2;
				}
			} while (0);

			/** FIXME：音频时间戳总是单调增加 20ms
			 */
			stamp_audio_ += 20;

			// Audio data header
			do {
				unsigned char audio_data_head = FLV_CODECID_SPEEX | FLV_SAMPLERATE_11025HZ | FLV_SAMPLESSIZE_16BIT;
				if (flv_write_data(file_, &audio_data_head, 1) < 0) {
					return -3;
				}
			} while (0);

			// speex data
			if (flv_write_data(file_, (unsigned char*)data, len) < 0) {
				return -4;
			}

			// tag size
			int tag_size = htonl(len + sizeof(FlvTagHead) + 1);
			if (flv_write_data(file_, (unsigned char*)&tag_size, 4) < 0) {
				return -5;
			}
	
			file_size_ += ntohl(tag_size)+4;
			return ntohl(tag_size) + 4;
		}
	};
};

flv_writer_t *flv_writer_open(const char *filename)
{
	Ctx *ctx = new Ctx;
	if (ctx->open(filename) == 0) {
		return (flv_writer_t *)ctx;
	}
	else {
		return 0;
	}
}

void flv_writer_close(flv_writer_t *f)
{
	Ctx *ctx = (Ctx*)f;
	ctx->close();
	delete ctx;
}

int flv_writer_save_h264(flv_writer_t *flv, const void *data, int len, 
	int key, double stamp, int width, int height)
{
	Ctx *ctx = (Ctx*)flv;
	return ctx->save_h264(data, len, key, stamp, width, height);
}

int flv_writer_save_aac(flv_writer_t *flv, const void *data, int len, double stamp)
{
	Ctx *ctx = (Ctx*)flv;
	return ctx->save_aac(data, len, stamp);
}

int flv_writer_save_speex(flv_writer_t *flv, const void *data, int len,	double stamp, int channels, int rate, int bits)
{
	Ctx *ctx = (Ctx*)flv;
	return ctx->save_speex(data, len, stamp, channels, rate, bits);
}
