#ifndef _BVH_
#define _BVH_

#include <vector>
#include "bvh_math.h"

/** bvh mocap data */

class BVH {
	public:

	enum Channel { Xpos=1, Ypos, Zpos, Xrot, Yrot, Zrot };

    struct Part {

        char*               	name;
        int                 	index;
        int                 	parent;
        int                 	channels;
        int                 	childCount;
        BVH_Math::vec3       	offset;
        BVH_Math::vec3       	end;
        BVH_Math::Transform*	motion;
		std::vector<int>    	childIndices;
    };

	public:
	BVH();
	~BVH();

	bool load(const char* data);

	int         getPartCount() const		{ return m_partCount; }
	const Part* getPart(int index) const    { return m_parts[index]; }
	int         getFrames() const           { return m_frames; }
	float       getFrameTime() const        { return m_frameTime; }


	private:
	Part* readHeirachy(const char*& data);

	protected:
	Part*  m_root;
	Part** m_parts;
	int    m_partCount;
	int    m_frames;
	float  m_frameTime;


};



#endif

