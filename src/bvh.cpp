#include <cstring>
#include <cstdlib>
#include <cstdio>

#include "bvh.h"

BVH::BVH() : m_root(0), m_parts(0), m_partCount(0) {}

BVH::~BVH() {
    
	for (int i=0; i<m_partCount; ++i) {
        
		delete [] m_parts[i]->name;
		delete [] m_parts[i]->motion;
        
        m_parts[i]->childIndices = std::vector<int>();  // Release memory by replacing with an empty vectpr (https://stackoverflow.com/a/63196454)
        
		delete m_parts[i];
	}
    
	delete [] m_parts;
}

inline void whitespace (const char*& s) {
    
	while (*s==' ' || *s=='\t' || *s=='\n' || *s=='\r') ++s;
}

inline void nextLine (const char*& s) {

    while (*s && *s != '\n' && *s != '\r') ++s;
	whitespace (s);
}

inline bool word (const char*& s, const char* key, int len) {
    
	if (strncmp (s, key, len) != 0) return false;
	s += len;
	return true;
}

inline bool readFloat (const char*& data, float& out) {
    
	char* end;
	out = strtod (data, &end);
    
    if (end > data) {
        
        data += end - data;
        return true;
        
    } else return false;
}

inline bool readInt (const char*& data, int& out) {
    
	char* end;
	out = (int)strtol (data, &end, 10);
    
    if (end > data) {
        
        data += end - data;
        return true;
        
    } else return false;
}

BVH::Part* BVH::readHeirachy (const char*& data) {
    
	whitespace (data);

	int len          = 0;                       // Parse name
	const char *name = data;
    
	while ((data[len]>='*' && data[len]<='z') || data[len]==' ') ++len;
    
	data += len;
	whitespace (data);
    
	while (name[len]==' ') --len;               // Trim

	if (!word (data, "{", 1)) return 0;         // Block start

	Part* part = new Part;                      // Create part
    
	part->parent       = -1;
	part->name         = 0;
	part->channels     = 0;
	part->motion       = 0;
    part->childIndices = std::vector<int>();

	if (len>0) {                                // Get part name
        
		part->name  = new char[len+1];
		memcpy (part->name, name, len);
		part->name[len] = 0;                    // null terminated
	}

	if (~m_partCount & 0x7) {                   // Add part to flat list
        
		Part** list = new Part*[(m_partCount&0xff8) + 0x8];
        
		if (m_partCount) memcpy (list, m_parts, m_partCount * sizeof (Part*));
        
		delete []   m_parts;
		m_parts     = list;
	}
    
	int partIndex      = m_partCount;
    int channelCount   = 0;
    int childCount     = 0;
    
    part->index        = partIndex;
    
	m_parts[partIndex] = part;
    
	m_partCount++;

	while (*data) {                             // Part data
        
		whitespace (data);

		if (word (data, "OFFSET", 6)) {         // Read joint offset
            
			readFloat (data, part->offset.x);
			readFloat (data, part->offset.y);
			readFloat (data, part->offset.z);
		}

		else if (word (data, "CHANNELS", 8)) {  // Read active channels
            
			readInt (data, channelCount);
            
			for (int i=0; i<channelCount; ++i) {
                
				whitespace (data);
                
				if      (word (data, "Xposition", 9)) part->channels |= Xpos << (i*3);
				else if (word (data, "Yposition", 9)) part->channels |= Ypos << (i*3);
				else if (word (data, "Zposition", 9)) part->channels |= Zpos << (i*3);
				else if (word (data, "Xrotation", 9)) part->channels |= Xrot << (i*3);
				else if (word (data, "Yrotation", 9)) part->channels |= Yrot << (i*3);
				else if (word (data, "Zrotation", 9)) part->channels |= Zrot << (i*3);
				else    { printf ("BVH::readHeirachy: Invalid channel %.10s\n", data); break; }
			}
		}

		else if (word (data, "JOINT", 5)) {     // Read child part
            
			Part* child = readHeirachy (data);
            
			if (!child) break;

            ++childCount;

            child->parent = partIndex;
			part->end     = part->end + child->offset;

            part->childIndices.push_back (child->index);
		}

		else if (word (data, "End Site", 8)) {  // End point
            
			whitespace (data);
            
			word (data, "{", 1);                // get length of end bone
            
			while (*data) {
                
				whitespace (data);
                
				if (word (data, "}", 1)) break;
				if (word (data, "OFFSET", 6)) {
                    
					readFloat (data, part->end.x);
					readFloat (data, part->end.y);
					readFloat (data, part->end.z);
				}
			}
		}

		else if (word (data, "}", 1)) {         // End block
            
			if (childCount>0) part->end *= 1.0 / childCount;
            
            part->childCount = childCount;
            
			return part;
		}

		else nextLine (data);                   // Error?
	}
    
	delete [] part->name;
	delete part;
	return 0;
}

bool BVH::load (const char* data) {
    
	while (*data) {
        
		whitespace (data);

		if (word (data, "HIERARCHY", 9)) {      // Load bone heirachy
            
			nextLine (data);
            
			if (word (data, "ROOT", 4)) {
                
				m_root = readHeirachy (data);
                
				if (!m_root) return false;
			}
		}

        else if (word (data, "MOTION", 6)) {    // Load motion data
            
            whitespace (data);
            
            if (word (data, "Frames:", 7)) {
                
                readInt (data, m_frames);
                whitespace (data);
            }
            
            if (word (data, "Frame Time:", 11)) {
                
                readFloat (data, m_frameTime);
                whitespace (data);
            }
            
            for (int i=0; i<m_partCount; ++i) { // Initialise memory
                
                m_parts[i]->motion = new BVH_Math::Transform[m_frames];
            }
            
            int     partIndex;
            int     channel;
            Part*   part;
            float   value;
            
            BVH_Math::vec3       pos;
            BVH_Math::Quaternion rot;
            
            const BVH_Math::vec3 xAxis (1,0,0);
            const BVH_Math::vec3 yAxis (0,1,0);
            const BVH_Math::vec3 zAxis (0,0,1);
            
            const float toRad = 3.141592653592f / 180;
            
            for (int frame=0; frame<m_frames; ++frame) {    // Read frames
                
                partIndex   = 0;
                part        = m_parts[0];
                channel     = part->channels;
                
                while (*data) {
                    
                    readFloat (data, value);

                    switch (channel & 0x7) {
                            
                        case Xpos: pos.x = value; break;
                        case Ypos: pos.y = value; break;
                        case Zpos: pos.z = value; break;
                            
                        case Xrot: rot = rot * BVH_Math::Quaternion (xAxis, value*toRad); break;
                        case Yrot: rot = rot * BVH_Math::Quaternion (yAxis, value*toRad); break;
                        case Zrot: rot = rot * BVH_Math::Quaternion (zAxis, value*toRad); break;
                    }
                    
                    channel >>= 3;
                    
                    if (channel == 0) {                     // Read all values for this part
                        
                        part->motion[frame].rotation = rot;
                        part->motion[frame].offset   = pos;
                        
                        ++partIndex;                        // Next part
                        
                        rot     = BVH_Math::Quaternion();
                        
                        if (partIndex == m_partCount) break;
                        
                        part    = m_parts[partIndex];
                        channel = part->channels;
                    }
                }
                
                nextLine (data);
            }
            
        } else return false;
	}
    
	return m_root && m_frames;
}
