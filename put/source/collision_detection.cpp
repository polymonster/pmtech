#if 0

#include "put_math.h"

/*======================== VARIABLES ======================*/

/*=========================================================*\
|	SPHERE vs SPHERE
\*=========================================================*/
bool put::maths::SPHERE_vs_SPHERE(SPHERE *s1, SPHERE *s2)
{
	//get the distance between the sphere's.
	f32 d = distance(s1->m_position,s2->m_position);

	//add radius of the 2 spheres
	f32 r = s1->m_radius + s2->m_radius;

	if(d < r)
	{
		s1->m_under_collision = true;
		s2->m_under_collision = true;
		//if the combine length of radii is less than the distance
		//between the 2 spheres they must overlap.
		return true;
	}

	return false;
}

/*=========================================================*\
|	AA_ELLIPSOID vs AA_ELLIPSOID
\*=========================================================*/
bool put::maths::AA_ELLIPSOID_vs_AA_ELLIPSOID(AA_ELLIPSOID *e1, AA_ELLIPSOID *e2)
{
	//hard
	return false;
}

/*=========================================================*\
|	AA_ELLIPSOID vs SPHERE
\*=========================================================*/
bool put::maths::AA_ELLIPSOID_vs_SPHERE(AA_ELLIPSOID *e1, SPHERE *s1)
{
	//not precise

	//get closest pos32 on the sphere
	vec3f p = s1->m_position + normalise(e1->m_position - s1->m_position) * s1->m_radius;

	if(POs32_inside_AA_ELLIPSOID(*e1,p)) return true;

	return false;
}

/*=========================================================*\
|	SPHERE vs TRIANGLE
\*=========================================================*/
bool put::maths::SPHERE_vs_TRIANGLE(SPHERE *s1, TRIANGLE *t1)
{
	//get the normal of the triangle
	vec3f normal = get_normal(*t1);

	//classify the sphere to the plane
	f32 plane_distance;
	s32 classification = classify_sphere(*s1,t1->m_vertices[0],normal,&plane_distance);

	//if an s32ersection occurs
	if(classification == s32ERSECTS) 
	{
		//offset the sphere so that the centre is offset from the polygon by distance
		vec3f offset = normal * plane_distance;

		//get the closest pos32 on the plane
		vec3f p = s1->m_position - offset;

		//if the closest pos32 on the plane is inside the triangle
		if(POs32_inside_TRIANGLE(*t1,p))
		{
			//now simple sphere distance checks
			f32 d = distance(s1->m_position,p);

			if(d <= s1->m_radius)
			{
				s1->m_under_collision = true;
				t1->m_under_collision = true;

				return true;
			}
		}
		else
		{
			//there could be a collision on the triangles edges
			//loop through all the vertices in the triangle
			for(s32 i = 0; i < 3; i++)
			{
				s32 j = i + 1;

				//make a line from vertex i to vertex j
				vec3f p = closest_point_on_line(t1->m_vertices[i], t1->m_vertices[j % 3], s1->m_position);
			
				// simple distance checks
				f32 d = distance(s1->m_position,p);

				if(d <= s1->m_radius)
				{
					s1->m_under_collision = true;
					t1->m_under_collision = true;

					return true;
				}	
			}
		}
	}

	return false;
}

/*=========================================================*\
|	AA_ELLIPSOID vs TRIANGLE
\*=========================================================*/
bool put::maths::AA_ELLIPSOID_vs_TRIANGLE(AA_ELLIPSOID *e1, TRIANGLE *t1)
{
	//make a bounding sphere around the ellipsoid
	//find the largets of the radii
	f32 s1_r = e1->m_radii.x;
	if(e1->m_radii.y > s1_r) s1_r = e1->m_radii.y;
	if(e1->m_radii.z > s1_r) s1_r = e1->m_radii.z;

	SPHERE *s1 = new SPHERE(e1->m_position,s1_r);

	//get the normal of the triangle
	vec3f normal = get_normal(*t1);

	//classify the sphere to the plane
	f32 plane_distance;
	s32 classification = classify_sphere(*s1,t1->m_vertices[0],normal,&plane_distance);

	//if an s32ersection occurs
	if(classification == s32ERSECTS) 
	{
		//offset the sphere so that the centre is offset from the polygon by distance
		vec3f offset = normal * plane_distance;

		//get the closest pos32 on the plane
		vec3f p = e1->m_position - offset;

		//if the closest pos32 on the plane is inside the triangle
		if(POs32_inside_TRIANGLE(*t1,p))
		{
			f32 d = distance(e1->m_position / e1->m_radii ,p / e1->m_radii);

			if(d <= 1.0f)
			{
				/*f32 s32ersection_ammount = 1.0f - d;

				Vector3f offset = normal * s32ersection_ammount;

				e1->m_position += (offset * e1->m_radii);*/

				t1->m_under_collision = true;
				e1->m_under_collision = true;

				return true;
			}
		}
		else
		{
			//there could be a collision on the triangles edges
			//loop through all the vertices in the triangle
			for(s32 i = 0; i < 3; i++)
			{
				s32 j = i + 1;

				//make a line from vertex i to vertex j
				vec3f p = closest_point_on_line(t1->m_vertices[i], t1->m_vertices[j % 3], e1->m_position);

				f32 d = distance(e1->m_position / e1->m_radii ,p / e1->m_radii);

				if(d <= 1.0f)
				{
					/*f32 s32ersection_ammount = 1.0f - d;

					Vector3f offset = normal * s32ersection_ammount;

					e1->m_position += (offset * e1->m_radii);*/

					t1->m_under_collision = true;
					e1->m_under_collision = true;

					return true;
				}
			}
		}
	}

	return false;
}

/*=========================================================*\
|	POs32 inside TRIANGLE
|		- tests if a pos32 lies inside a triangle, using
|		- bayrecentric space
\*=========================================================*/
bool put::maths::POs32_inside_TRIANGLE(TRIANGLE t1, vec3f p)
{
	const f32 MATCH_FACTOR = 0.99f;

	f32 angle = 0.0;					

	vec3f v1, v2;					
	
	// Go in a circle to each vertex and get the angle between vectors adding them up
	for(s32 i = 0; i < 3; i++)		
	{	
		s32 j = i + 1;
		// Subtract the s32ersection pos32 from the current vertex
		v1 = t1.m_vertices[i] - p;			
		
		// Subtract the pos32 from the next vertex
		v2 = t1.m_vertices[j % 3] - p; 
		
		//add the angles up 
		angle += angle_between_vectors(v1, v2);	
	}

	// If the angle is greater than 2 PI, (360 degrees)	
	// The pos32 is inside of the polygon
	if(angle >= (MATCH_FACTOR * (2.0 * PI) ) )
	{
		return true;		
	}
	
	return false;
}

/*=========================================================*\
|	POs32 inside AA_ELLIPSOID
|		- tests if a pos32 lies inside an ellipsoid, using
|		- ellipsoid space
\*=========================================================*/
bool put::maths::POs32_inside_AA_ELLIPSOID(AA_ELLIPSOID e1, vec3f p)
{
	f32 d = distance(e1.m_position / e1.m_radii ,p / e1.m_radii);

	if(d <= 1.0f)
	{
		return true;
	}

	return false;
}

/*=========================================================*\
|	POs32 inside SPHERE
|		- tests if a pos32 lies inside a sphere, using
|		- simple distance test
\*=========================================================*/
bool put::maths::POs32_inside_SPHERE(SPHERE s1, vec3f p)
{
	f32 d = distance(s1.m_position, p);
	
	if(d <= s1.m_radius)
	{
		return true;
	}

	return false;
}

/*=========================================================*\
|	AABB3D vs AABB3D
\*=========================================================*/
bool put::maths::AABB3D_vs_AABB3D(AABB3D *b1, AABB3D *b2)
{
	//this could be optimised out
	vec3f b1_extent_min = b1->m_position - b1->m_size;
	vec3f b1_extent_max = b1->m_position + b1->m_size;

	vec3f b2_extent_min = b2->m_position - b2->m_size;
	vec3f b2_extent_max = b2->m_position + b2->m_size;

	//overlap on x axis
	if(		b1_extent_min.x > b2_extent_min.x && b1_extent_min.x < b2_extent_max.x
		||  b1_extent_max.x < b2_extent_max.x && b1_extent_max.x > b2_extent_min.x)
	{} else return false;

	//overlap on y axis
	if(		b1_extent_min.y > b2_extent_min.y && b1_extent_min.y < b2_extent_max.y
		||  b1_extent_max.y < b2_extent_max.y && b1_extent_max.y > b2_extent_min.y)
	{} else return false;


	//overlap on z axis
	if(		b1_extent_min.z > b2_extent_min.z && b1_extent_min.z < b2_extent_max.z
		||  b1_extent_max.z < b2_extent_max.z && b1_extent_max.z > b2_extent_min.z)
	{} else return false;

	b1->m_under_collision = true;
	b2->m_under_collision = true;

	return true;
}

/*=========================================================*\
|	POs32 inside AABB3D - returns true if a pos32 is inside
|						an aabb
\*=========================================================*/
bool put::maths::POs32_inside_AABB3D(AABB3D b1, vec3f p)
{
	//this could be optimised out
	vec3f b1_extent_min = b1.m_position - b1.m_size;
	vec3f b1_extent_max = b1.m_position + b1.m_size;

	if(p.x > b1_extent_min.x && p.x < b1_extent_max.x)
	{} else return false;

	if(p.y > b1_extent_min.y && p.y < b1_extent_max.y)
	{} else return false;

	if(p.z > b1_extent_min.x && p.z < b1_extent_max.z)
	{} else return false;

	return true;
}

/*=========================================================*\
|	AABB3D_vs_SPHERE
\*=========================================================*/
bool put::maths::AABB3D_vs_SPHERE(AABB3D *b1, SPHERE *s1)
{
	vec3f p = closest_point_on_AABB3D(*b1,s1->m_position);

	f32 d = distance(p,s1->m_position);

	if(d < s1->m_radius)
	{
		b1->m_under_collision = true;
		s1->m_under_collision = true;

		return true;
	}

	return false;
}

/*=========================================================*\
|	AABB3D_vs_AA_ELLIPSOID
\*=========================================================*/
bool put::maths::AABB3D_vs_AA_ELLIPSOID(AABB3D *b1, AA_ELLIPSOID *e1)
{
	vec3f p = closest_point_on_AABB3D(*b1,e1->m_position);

	f32 d = distance(p / e1->m_radii,e1->m_position / e1->m_radii);

	if(d < 1.0f)
	{
		b1->m_under_collision = true;
		e1->m_under_collision = true;

		return true;
	}

	return false;
}

bool put::maths::OBB3D_vs_OBB3D(OBB3D *b1, OBB3D *b2)
{
	//get the first box's axes
	vec3f b1_axis[3];
	vec3f b2_axis[3];

	get_axes_from_OBB(*b1,b1_axis);
	get_axes_from_OBB(*b2,b2_axis);

	vec3f axis_list[15];
	
	for(s32 i = 0; i < 3; i++)
	{
		axis_list[i] = b1_axis[i];
		axis_list[i + 3] = b2_axis[i];
	}

	//9 cross products to complete the bill
	for(s32 i = 0; i < 3; i++)
	{
		for(s32 j = 0; j < 3; j++)
		{
			s32 index = (i * 3) + j;

			axis_list[index + 6] = normalise(cross(b2_axis[i],b1_axis[j]));
		}
	}

	//b2's and b1's transformed verts (starting with a unit cube)
	vec3f b1_verts[8];
	vec3f b2_verts[8];

	for(s32 i = 0; i < 8; i++) 
	{
		b2_verts[i] = b2->m_orientation_matrix * unit_cube_vertices[i];
		b1_verts[i] = b1->m_orientation_matrix * unit_cube_vertices[i];
	}


	//project each of the boxes vertices onto each axis, to find the min and max extents on each axis
	for(s32 i = 0; i < 15; i++)
	{
		f32 b1_min,b1_max;
		f32 b2_min,b2_max;

		//get the extents on each axis of b1
		find_extents(axis_list[i],b1_verts,8,&b1_min,&b1_max);
		find_extents(axis_list[i],b2_verts,8,&b2_min,&b2_max);


		if( ((b1_min >= b2_min) && (b1_min <= b2_max)) ||
			((b1_max >= b2_min) && (b1_max <= b2_max)) ||
			((b2_min >= b1_min) && (b2_min <= b1_max)) ||
			((b2_max >= b1_min) && (b2_max <= b1_max)))
		{
		}
		else
		{
			return false;
		}
	}

	b1->m_under_collision = true;
	b2->m_under_collision = true;

	return true;
}

bool put::maths::CONVEX_HULL_vs_CONVEX_HULL(CONVEX_HULL *h1, CONVEX_HULL *h2)
{
	Vector3fArray axis_list;

	Vector3fArray h1_transformed_verts;
	Vector3fArray h2_transformed_verts;

	//add each axis from h1 and h2 s32o the axis list
	for(unsigned s32 i = 0; i < h1->m_axes.size(); i++)
	{
		axis_list.push_back(h1->m_rotation_matrix * h1->m_axes.at(i));
	}

	for(unsigned s32 i = 0; i < h2->m_axes.size(); i++)
	{
		axis_list.push_back(h2->m_rotation_matrix * h2->m_axes.at(i));
	}

	//transform the verts
	for(unsigned s32 i = 0; i < h1->m_vertices.size(); i++)
	{
		vec3f transformed_vert;

		transformed_vert = h1->m_orientation_matrix * h1->m_vertices.at(i);

		h1_transformed_verts.push_back(transformed_vert);
	}

	for(unsigned s32 i = 0; i < h2->m_vertices.size(); i++)
	{
		vec3f transformed_vert;

		transformed_vert = h2->m_orientation_matrix * h2->m_vertices.at(i);

		h2_transformed_verts.push_back(transformed_vert);
	}

	//get the cross products of each hulls axes
	for(unsigned s32 i = 0; i < h1->m_axes.size(); i++)
	{
		for(unsigned s32 j = 0; j < h2->m_axes.size(); j++)
		{
			vec3f cross_axis;

			cross_axis = normalise(cross(h1->m_rotation_matrix * h1->m_axes.at(i),h2->m_rotation_matrix * h2->m_axes.at(j)));

			axis_list.push_back(cross_axis);
		}
	}

	//keep track of the shortest overlap.
	f32 shortest_overlap = 0.0f;
	s32 shortest_overlap_axis = -1;

	for(unsigned s32 i = 0; i < axis_list.size(); i++)
	{
		f32 h1_min,h1_max;
		f32 h2_min,h2_max;

		//get the extents on each axis of h1, h2
		find_extents(axis_list.at(i),h1_transformed_verts,&h1_min,&h1_max);
		find_extents(axis_list.at(i),h2_transformed_verts,&h2_min,&h2_max);

		s32 s32ersection_case = 0;

		if((h1_min >= h2_min) && (h1_min <= h2_max)) s32ersection_case = 1;
		else if((h1_max >= h2_min) && (h1_max <= h2_max)) s32ersection_case = 2;
		else if((h2_min >= h1_min) && (h2_min <= h1_max)) s32ersection_case = 3;
		else if((h2_max >= h1_min) && (h2_max <= h1_max)) s32ersection_case = 4;
		else return false;

		f32 overlap = 0.0f;

		switch(s32ersection_case)
		{
			case 1:

				overlap = absolute_smallest_of(h2_min - h1_max,h2_max - h1_min);

			break;

			case 2:

				overlap = absolute_smallest_of(h2_min - h1_max,h2_max - h1_min);

			break;

			case 3:

				overlap = absolute_smallest_of(h1_min - h2_max,h1_max - h2_min);

			break;

			case 4:

				overlap = absolute_smallest_of(h1_min - h2_max,h1_max - h2_min);

			break;
		}

		if(i == 0) 
		{
			shortest_overlap = overlap;
			shortest_overlap_axis = i;
		}
		else if(overlap != 0.0f)
		{
			f32 old_shortest = shortest_overlap;
			shortest_overlap = absolute_smallest_of(shortest_overlap,overlap);

			if(shortest_overlap != old_shortest) shortest_overlap_axis = i;
		}
	}

	h1->m_under_collision = true;
	h2->m_under_collision = true;


	vec3f offset = axis_list.at(shortest_overlap_axis) * shortest_overlap;
	h1->m_position -= offset;
	h1->build_matrix();

	return true;
}

#endif

