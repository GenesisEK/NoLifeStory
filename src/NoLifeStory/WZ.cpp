////////////////////////////////////////////////////
// This file is part of NoLifeStory.              //
// Please see SuperGlobal.h for more information. //
////////////////////////////////////////////////////
#include "Global.h"
#include "Keys.h"

string Path;
NLS::Node NLS::WZ;
uint8_t *WZKey = 0;
uint8_t BMSKey[0xFFFF];
uint8_t *WZKeys[] = {GMSKey, BMSKey};
uint8_t GMSKeyIV[4] = {0x4D, 0x23, 0xC7, 0x2B};
uint32_t OffsetKey = 0x581C3F6D;
int16_t EncVersion;
uint16_t Version = 0;
uint32_t VersionHash;

#pragma region File reading stuff
template <class T>
inline T Read(ifstream* file) {
	T v;
	file->read((char*)&v, sizeof(v));
	return v;
}

inline int32_t ReadCInt(ifstream* file) {
	int8_t a = Read<int8_t>(file);
	if (a != -128) {
		return a;
	} else {
		int32_t b = Read<int32_t>(file);
		return b;
	}
}

inline string ReadEncString(ifstream* file) {
	int8_t slen = Read<int8_t>(file);
	if (slen == 0) {
		return string();
	} else if (slen > 0) {
		int32_t len;
		if (slen == 127) {
			len = Read<int32_t>(file);
		} else {
			len = slen;
		}
		if (len <= 0) {
			return string();
		}
		string s(len, '\0');
		uint16_t mask = 0xAAAA;
		for (int i = 0; i < len; i++) {
			uint16_t enc = Read<uint16_t>(file);
			enc ^= mask;
			enc ^= (WZKey[i*2+1]<<8)+WZKey[i*2];
			mask++;
			s[i] = enc;
		}
		return s;
	} else {
		int32_t len;
		if (slen == -128) {
			len = Read<int32_t>(file);
		} else {
			len = -slen;
		}
		if (len <= 0) {
			return string();
		}
		uint8_t mask = 0xAA;
		static char str[1024];
		file->read(str, len);
		for (int i = 0; i < len; i++) {
			str[i] ^= mask;
			str[i] ^= WZKey[i];
			mask++;
		}
		str[len] = '\0';
		string s = str;
		return s;
	}
}

inline void ReadEncFast(ifstream* file) {
	int8_t slen = Read<int8_t>(file);
	if (slen == 0) {return;}
	if (slen > 0) {
		int32_t len;
		if (slen == 127) {
			len = Read<int32_t>(file);
		} else {
			len = slen;
		}
		if (len <= 0) {return;}
		file->seekg(len*2, ios::cur);
	} else {
		int32_t len;
		if (slen == -128) {
			len = Read<int32_t>(file);
		} else {
			len = -slen;
		}
		if (len <= 0) {return;}
		file->seekg(len, ios::cur);
	}
}

inline string ReadString(ifstream* file, uint32_t offset) {
	uint8_t a = Read<uint8_t>(file);
	switch (a) {
	case 0x00:
	case 0x73:
		return ReadEncString(file);
	case 0x01:
	case 0x1B:
		{
			int32_t off = Read<int32_t>(file);
			offset += off;
			uint32_t p = file->tellg();
			file->seekg(offset);
			string s = ReadEncString(file);
			file->seekg(p);
			return s;
		}
	default:
		return string();
	}
}

inline string ReadStringOffset(ifstream* file, uint32_t offset) {
	uint32_t p = file->tellg();
	file->seekg(offset);
	string s = ReadEncString(file);
	file->seekg(p);
	return s;
}
#pragma endregion

#pragma region WZ Parsing
void NLS::InitWZ(const string& path) {
	function <uint32_t(ifstream*, uint32_t)> ReadOffset;
	function <void(Node)> File;
	function <void(Node, ifstream*, uint32_t)> Directory;
	ReadOffset = [](ifstream* file, uint32_t fileStart)->uint32_t {
		uint32_t p = file->tellg();
		p = (p-fileStart)^0xFFFFFFFF;
		p *= VersionHash;
		p -= OffsetKey;
		p = (p<<(p&0x1F))|(p>>(32-p&0x1F));
		uint32_t more = Read<uint32_t>(file);
		p ^= more;
		p += fileStart*2;
		return p;
	};
	File = [&ReadOffset, &Directory](Node n) {
		string filename = Path+n.Name()+".wz";
		ifstream *file = new ifstream(filename, ios::in|ios::binary);
		if (!file->is_open()) {
			C("ERROR") << "Failed to load " << filename << endl;
			return;//Don't throw an error because of Nexon's stupid ExcelReport crap
		}
		string ident(4, '\0');
		file->read(const_cast<char*>(ident.c_str()), 4);
		if (ident != "PKG1") {
			C("ERROR") << "Invalid ident header for " << filename << endl;
			throw(273);
		}
		uint64_t fileSize = Read<uint64_t>(file);
		uint32_t fileStart = Read<uint32_t>(file);
		string copyright;
		*file >> copyright;
		file->seekg(fileStart);
		if (!Version) {
			EncVersion = Read<int16_t>(file);
			int32_t count = ReadCInt(file);
			uint32_t c = 0;
			for (int k = 0; k < count; k++) {
				uint8_t type = Read<uint8_t>(file);
				if (type == 3) {
					ReadEncFast(file);
					ReadCInt(file);
					ReadCInt(file);
					Read<uint32_t>(file);
					continue;
				} else if (type == 4) {
					ReadEncFast(file);
					ReadCInt(file);
					ReadCInt(file);
					c = file->tellg();
					break;
				} else {
					C("ERROR") << "Malformed WZ structure" << endl;
					throw(273);
				}
			}
			if (c == 0) {
				C("ERROR") << "Unable to find a top level .img for hash verification" << endl;
				throw(273);
			}
			bool success = false;
			for (uint8_t j = 0; j < 2 and !success; j++) {
				WZKey = WZKeys[j];
				for (Version = 0; Version < 256; Version++) {
					string s = tostring(Version);
					VersionHash = 0;
					for (int i = 0; i < s.size(); i++) {
						VersionHash = 32*VersionHash+s[i]+1;
					}
					uint32_t result = 0xFF^(VersionHash>>24)^(VersionHash<<8>>24)^(VersionHash<<16>>24)^(VersionHash<<24>>24);
					if (result == EncVersion) {
						file->clear();
						file->seekg(c);
						uint32_t offset = ReadOffset(file, fileStart);
						if (offset > fileSize) {
							continue;
						}
						file->seekg(offset);
						uint8_t a = Read<uint8_t>(file);
						if(a != 0x73) {
							continue;
						}
						string ss = ReadEncString(file);
						if (ss != "Property") {
							continue;
						}
						C("WZ") << "Detected WZ version: " << Version << endl;
						success = true;
						break;
					}
				}
			}
			if (!success) {
				C("ERROR") << "Unable to determine WZ version" << endl;
				throw(273);
			}
			file->seekg(fileStart+2);
		} else {
			int16_t eversion = Read<int16_t>(file);
			if (eversion != EncVersion) {
				C("ERROR") << "Version of WZ file does not match existing files" << endl;
				throw(273);
			}
		}
		Directory(n, file, fileStart);
	};
	Directory = [&File, &ReadOffset, &Directory](Node n, ifstream* file, uint32_t fileStart) {
		int32_t count = ReadCInt(file);
		if (count == 0) {
			File(n);
			return;
		}
		set<pair<string, uint32_t>> dirs;
		for (int i = 0; i < count; i++) {
			string name;
			uint8_t type = Read<uint8_t>(file);
			if (type == 1) {
				file->seekg(10, ios::cur);
				continue;
			} else if (type == 2) {
				int32_t s = Read<int32_t>(file);
				uint32_t p = file->tellg();
				file->seekg(fileStart+s);
				type = Read<uint8_t>(file);
				name = ReadEncString(file);
				file->seekg(p);
			} else if (type == 3) {
				name = ReadEncString(file);
			} else if (type == 4) {
				name = ReadEncString(file);
			} else {
				C("ERROR") << "Wat?" << endl;
				throw(273);
			}
			int32_t fsize = ReadCInt(file);
			int32_t checksum = ReadCInt(file);
			uint32_t offset = ReadOffset(file, fileStart);
			if (type == 3) {
				dirs.insert(pair<string, uint32_t>(name, offset));
			} else if (type == 4) {
				name.erase(name.size()-4);
				new Img(file, n.g(name), offset);
			} else {
				C("ERROR") << "Wat?" << endl;
				throw(273);
			}
		}
		for (auto it = dirs.begin(); it != dirs.end(); it++) {
			file->seekg(it->second);
			Directory(n.g(it->first), file, fileStart);
		}
	};
	memset(BMSKey, 0, 0xFFFF);
	string paths[6] = {path, "", "C:/Nexon/MapleStory/", "/home/snake/", "/", "T:/"};
	for (int i = 0; i < 5; i++) {
		Path = paths[i];
		if (exists(Path+"Data.wz")) {
			C("WZ") << "Loading beta WZ file structure" << endl;
			WZ.Name("Data");
			File(WZ);
			return;
		}
		if(exists(Path+"Base.wz")) {
			C("WZ") << "Loading standard WZ file structure" << endl;
			WZ.Name("Base");
			File(WZ);
			return;
		}
	}
	C("ERROR") << "I CAN'T FIND YOUR WZ FILES YOU NUB" << endl;
	throw(273);
}

NLS::Img::Img(ifstream* file, Node n, uint32_t offset) {
	this->n = n;
	n.Set(this);
	this->offset = offset;
	this->file = file;
}

void NLS::Img::Parse() {
	function <void(ifstream*, Node, uint32_t)> SubProperty;
	function <void(ifstream*, Node, uint32_t)> ExtendedProperty;
	function <void(Node)> Resolve;
	SubProperty = [&ExtendedProperty](ifstream* file, Node n, uint32_t offset) {
		int32_t count = ReadCInt(file);
		for (int i = 0; i < count; i++) {
			string name = ReadString(file, offset);
			uint8_t a = Read<uint8_t>(file);
			switch (a) {
			case 0x00:
				n.g(name);
				break;
			case 0x0B:
			case 0x02:
				n.g(name).Set(Read<uint16_t>(file));
				break;
			case 0x03:
				n.g(name).Set(ReadCInt(file));
				break;
			case 0x04:
				if (Read<uint8_t>(file) == 0x80) {
					n.g(name).Set(Read<float>(file));
				}
				break;
			case 0x05:
				n.g(name).Set(Read<double>(file));
				break;
			case 0x08:
				n.g(name).Set(ReadString(file, offset));
				break;
			case 0x09:
				{
					uint32_t temp = Read<uint32_t>(file);
					temp += file->tellg();
					ExtendedProperty(file, n.g(name), offset);
					file->seekg(temp);
					break;
				}
			default:
				C("ERROR") << "Unknown Property type" << endl;
				throw(273);
			}
		}
	};
	ExtendedProperty = [&SubProperty, &ExtendedProperty](ifstream* file, Node n, uint32_t offset) {
		string name;
		uint8_t a = Read<uint8_t>(file);
		if (a == 0x1B) {
			int32_t inc = Read<int32_t>(file);
			uint32_t pos = offset+inc;
			uint32_t p = file->tellg();
			file->seekg(pos);
			name = ReadEncString(file);
			file->seekg(p);
		} else {
			name = ReadEncString(file);
		}
		if (name == "Property") {
			file->seekg(2, ios::cur);
			SubProperty(file, n, offset);
		} else if (name == "Canvas") {
			file->seekg(1, ios::cur);
			uint8_t b = Read<uint8_t>(file);
			if (b == 1) {
				file->seekg(2, ios::cur);
				SubProperty(file, n, offset);
			}
			new PNGProperty(file, n);
		} else if (name == "Shape2D#Vector2D") {
			n.g("x").Set(ReadCInt(file));
			n.g("y").Set(ReadCInt(file));
		} else if (name == "Shape2D#Convex2D") {
			int32_t ec = ReadCInt(file);
			for (int i = 0; i < ec; i++) {
				ExtendedProperty(file, n.g(tostring(ec)), offset);
			}
		} else if (name == "Sound_DX8") {
			new SoundProperty(file, n);
		} else if (name == "UOL") {
			file->seekg(1, ios::cur);
			uint8_t b = Read<uint8_t>(file);
			switch (b) {
			case 0:
				n.g(name).Set(ReadEncString(file));
				break;
			case 1:
				{
					uint32_t off = Read<uint32_t>(file);
					n.g(name).Set(ReadStringOffset(file, offset+off));
					break;
				}
			default:
				C("ERROR") << "Unknown UOL type" << endl;
				throw(273);
			}
		} else {
			C("ERROR") << "Unknown ExtendedProperty type" << endl;
			throw(273);
		};
	};
	Resolve = [&Resolve](Node n) {
		if (n["UOL"]) {
			string s = n["UOL"];
			string str;
			vector <string> parts;
			for (int i = 0; i < s.size(); i++) {
				if (s[i] == '/') {
					parts.push_back(str);
					str = "";
				} else {
					str.push_back(s[i]);
				}
			}
			parts.push_back(str);
			Node nn = n.Parent();
			for (auto it = parts.begin(); it != parts.end(); it++) {
				if (!nn) {
					break;
				}
				if (*it == "..") {
					nn = nn.Parent();
				} else {
					nn = nn[*it];
				}
			}
			if (nn) {
				n.Assign(nn);
			}
		} else {
			for (auto it = n.begin(); it != n.end(); it++) {
				if (it->second) {
					Resolve(it->second);
				}
			}
		}
	};
	file->seekg(offset);
	uint8_t a = Read<uint8_t>(file);
	if (a != 0x73) {
		C("WZ") << "Invalid WZ image!" << endl;
		throw(273);
	}
	string s = ReadEncString(file);
	if (s != "Property") {
		C("WZ") << "Invalid WZ image!" << endl;
		throw(273);
	}
	uint16_t b = Read<uint16_t>(file);
	if (b != 0) {
		C("WZ") << "Invalid WZ image!" << endl;
		throw(273);
	}
	SubProperty(file, n, offset);
	Resolve(n);
	delete this;
}

NLS::PNGProperty::PNGProperty(ifstream* file, Node n) {
	this->file = file;
	sprite.data = new SpriteData;
	sprite.data->loaded = false;
	sprite.data->width = ReadCInt(file);
	sprite.data->height = ReadCInt(file);
	sprite.data->png = this;
	sprite.data->originx = n["origin"]["x"];
	sprite.data->originy = n["origin"]["y"];
	n.Set(sprite);
	format = ReadCInt(file);
	format2 = Read<uint8_t>(file);
	file->seekg(4, ios::cur);
	length = Read<int32_t>(file);
	if (length <= 0) {
		C("ERROR") << "What sort of shit is this?" << endl;
		throw(273);
	}
	offset = file->tellg();
	offset++;
}

void NLS::PNGProperty::Parse() {
	static uint8_t Buf1[0x1000000];
	static uint8_t Buf2[0x1000000];
	auto Decompress = [&](uint32_t inLen, uint32_t outLen) {
		z_stream strm;
		strm.next_in = Buf2;
		strm.avail_in = inLen;
		strm.opaque = nullptr;
		strm.zfree = nullptr;
		strm.zalloc = nullptr;
		inflateInit(&strm);
		strm.next_out = Buf1;
		strm.avail_out = outLen;
		int err = inflate(&strm, Z_FINISH);
		switch(err){
		case Z_BUF_ERROR:
			break;
		default:
			NLS::C("ERROR") << "I hate zlib!" << endl;
			throw(273);
		}
		if (strm.total_out != outLen) {
			NLS::C("ERROR") << "I hate zlib!" << endl;
			throw(273);
		}
		inflateEnd(&strm);
	};
	file->seekg(offset);
	file->read((char*)Buf2, length);
	int32_t f = format+format2;
	glGenTextures(1, &sprite.data->texture);
	glBindTexture(GL_TEXTURE_2D, sprite.data->texture);
	switch (f) {
	case 1:
		{
			uint32_t len = 2*sprite.data->width*sprite.data->height;
			Decompress(length, len);
			for (uint32_t i = 0; i < len; i++) {//TODO - Figure out if I can avoid this step somehow
				Buf2[i*2] = (Buf1[i]&0x0F)*0x11;
				Buf2[i*2+1] = ((Buf1[i]&0xF0)>>4)*0x11;
			}
			glTexImage2D(GL_TEXTURE_2D, 0, 4, sprite.data->width, sprite.data->height, 0, GL_BGRA, GL_UNSIGNED_BYTE, Buf2);
			break;
		}
	case 2:
		{
			uint32_t len = 4*sprite.data->width*sprite.data->height;
			Decompress(length, len);
			glTexImage2D(GL_TEXTURE_2D, 0, 4, sprite.data->width, sprite.data->height, 0, GL_BGRA, GL_UNSIGNED_BYTE, Buf1);
			break;
		}
	case 513:
		{
			uint32_t len = 2*sprite.data->width*sprite.data->height;
			Decompress(length, len);
			glTexImage2D(GL_TEXTURE_2D, 0, 4, sprite.data->width, sprite.data->height, 0, GL_BGR, GL_UNSIGNED_SHORT_5_6_5_REV, Buf1);
			break;
		}
	case 517:
		{
			uint32_t len = sprite.data->width*sprite.data->height/128;
			Decompress(length, len);
			for (uint32_t i = 0; i*2 < len; i++) {
				for (uint32_t j = 0; j < 512; j++) {//TODO - Check for built in methods to do this in a single function call
					Buf2[i*512+j*2] = Buf1[2*i];
					Buf2[i*512+j*2+1] = Buf1[2*i+1];
				}
			}
			glTexImage2D(GL_TEXTURE_2D, 0, 4, sprite.data->width, sprite.data->height, 0, GL_BGR, GL_UNSIGNED_SHORT_5_6_5_REV, Buf2);
			break;
		}
	default:
		C("ERROR") << "Unknown sprite format" << endl;
		throw(273);
	}
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	sprite.data->loaded = true;
}

NLS::SoundProperty::SoundProperty(ifstream* file, Node n) {
	this->file = file;
	file->seekg(1, ios::cur);
	len = ReadCInt(file);
	ReadCInt(file);
	offset = file->tellg();
	offset += 82;
	data = 0;
	n.Set(Sound(this));
}

uint32_t NLS::SoundProperty::GetStream(bool loop) {
	if (!data) {
		file->seekg(offset);
		data = new uint8_t[len];
		file->read((char*)data, len);
	}
	if (loop) {
		return BASS_StreamCreateFile(true, data, 0, len, BASS_SAMPLE_FLOAT|BASS_SAMPLE_LOOP);
	} else {
		return BASS_StreamCreateFile(true, data, 0, len, BASS_SAMPLE_FLOAT);
	}
}
#pragma endregion

#pragma region Node Stuff
class NLS::NodeData {
public:
	NodeData() {
		image = 0;
		intValue = 0;
		floatValue = 0;
	}
	string stringValue;
	double floatValue;
	int intValue;
	Sprite sprite;
	Sound sound;
	Node parent;
	string name;
	map <string, Node> children;
	Img* image;
private:
	NodeData(const NodeData&);
	NodeData& operator= (const NodeData&);
};

NLS::Node::Node() {
	data = 0;
}

NLS::Node::Node(const Node& other) {
	data = other.data;
}

NLS::Node& NLS::Node::operator= (const Node& other) {
	data = other.data;
	return *this;
}

NLS::Node NLS::Node::operator[] (const string& key) {
	if (!data) {
		return Node();
	}
	if (data->image) {
		Img* img = data->image;;
		data->image = 0;
		img->Parse();
	}
	auto n = data->children.find(key);
	if (n == data->children.end()) {
		return Node();
	}
	return n->second;
}

NLS::Node NLS::Node::operator[] (const char key[]) {
	return (*this)[string(key)];
}

NLS::Node NLS::Node::operator[] (const int& key) {
	return (*this)[tostring(key)];
}

NLS::Node NLS::Node::g(const string& key) {
	assert(data);
	Node& n = data->children[key];
	n.data = new NodeData();
	n.data->parent = *this;
	n.data->name = key;
	return n;
}

map<string, NLS::Node>::iterator NLS::Node::begin() {
	if (!data) {
		return map<string, NLS::Node>::iterator();
	}
	return data->children.begin();
}

map<string, NLS::Node>::iterator NLS::Node::end() {
	if (!data) {
		return map<string, NLS::Node>::iterator();
	}
	return data->children.end();
}

string NLS::Node::Name() {
	if (!data) {
		return string();
	}
	return data->name;
}

void NLS::Node::Name(const string& s) {
	if (!data) {
		data = new NodeData();
	}
	data->name = s;
}

NLS::Node NLS::Node::Parent() {
	if (!data) {
		return Node();
	}
	return data->parent;
}

void NLS::Node::Assign(const Node& other) {
	data->children = other.data->children;
	data->floatValue = other.data->floatValue;
	data->intValue = other.data->intValue;
	data->stringValue = other.data->stringValue;
	data->sprite = other.data->sprite;
	data->image = other.data->image;
}

NLS::Node::operator bool() {
	return (bool)data;
}

NLS::Node::operator string() {
	if (!data) {
		return string();
	}
	return data->stringValue;
}

NLS::Node::operator double() {
	if (!data) {
		return 0;
	}
	return data->floatValue;
}

NLS::Node::operator int() {
	if (!data) {
		return 0;
	}
	return data->intValue;
}

NLS::Node::operator NLS::Sprite() {
	if (!data) {
		return Sprite();
	}
	return data->sprite;
}

NLS::Node::operator NLS::Sound() {
	if (!data) {
		return Sound();
	}
	return data->sound;
}

void NLS::Node::Set(const string& v) {
	data->intValue = toint(v);
	data->floatValue = todouble(v);
	data->stringValue = v;
}

void NLS::Node::Set(const double& v) {
	data->intValue = v;
	data->floatValue = v;
	data->stringValue = tostring(v);
}

void NLS::Node::Set(const int& v) {
	data->intValue = v;
	data->floatValue = v;
	data->stringValue = tostring(v);
}

void NLS::Node::Set(const Sprite& v) {
	data->sprite = v;
}

void NLS::Node::Set(const Sound& v) {
	data->sound = v;
}

void NLS::Node::Set(Img* v) {
	data->image = v;
}
#pragma endregion
